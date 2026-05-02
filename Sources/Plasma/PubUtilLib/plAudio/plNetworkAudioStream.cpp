/*==LICENSE==*

CyanWorlds.com Engine - MMOG client, server and tools
Copyright (C) 2011  Cyan Worlds, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

Additional permissions under GNU GPL version 3 section 7

If you modify this Program, or any covered work, by linking or
combining it with any of RAD Game Tools Bink SDK, Autodesk 3ds Max SDK,
NVIDIA PhysX SDK, Microsoft DirectX SDK, OpenSSL library, Independent
JPEG Group JPEG library, Microsoft Windows Media SDK, or Apple QuickTime SDK
(or a modified version of those libraries),
containing parts covered by the terms of the Bink SDK EULA, 3ds Max EULA,
PhysX SDK EULA, DirectX SDK EULA, OpenSSL and SSLeay licenses, IJG
JPEG Library README, Windows Media SDK EULA, or QuickTime SDK EULA, the
licensors of this Program grant you additional
permission to convey the resulting work. Corresponding Source for a
non-source form of such a combination shall include the source code for
the parts of OpenSSL and IJG JPEG Library used as well as that of the covered
work.

You can contact Cyan Worlds, Inc. by email legal@cyan.com
 or by snail mail at:
      Cyan Worlds, Inc.
      14617 N Newport Hwy
      Mead, WA   99021

*==LICENSE==*/

#include "plNetworkAudioStream.h"

#ifdef USE_MPG123
#   include <mpg123.h>
#   include <curl/curl.h>
#   include <AL/al.h>
#endif

#include "plStatusLog/plStatusLog.h"

#include <chrono>
#include <sstream>
#include <string>
#include <thread>

// ---------------------------------------------------------------------------
// Ring buffer (lock-free SPSC)
// ---------------------------------------------------------------------------

size_t plNetworkAudioStream::IRingAvailable() const
{
    size_t w = fRingWrite.load(std::memory_order_acquire);
    size_t r = fRingRead.load(std::memory_order_acquire);
    return (w >= r) ? (w - r) : (kRingBufSize - r + w);
}

size_t plNetworkAudioStream::IRingFree() const
{
    // Always keep one slot empty to distinguish full from empty
    return kRingBufSize - IRingAvailable() - 1;
}

size_t plNetworkAudioStream::IRingWrite(const uint8_t* data, size_t len)
{
    size_t free = IRingFree();
    if (len > free) len = free;
    size_t w = fRingWrite.load(std::memory_order_relaxed);
    for (size_t i = 0; i < len; i++)
        fRing[(w + i) % kRingBufSize] = data[i];
    fRingWrite.store((w + len) % kRingBufSize, std::memory_order_release);
    return len;
}

size_t plNetworkAudioStream::IRingRead(uint8_t* out, size_t len)
{
    size_t avail = IRingAvailable();
    if (len > avail) len = avail;
    size_t r = fRingRead.load(std::memory_order_relaxed);
    for (size_t i = 0; i < len; i++)
        out[i] = fRing[(r + i) % kRingBufSize];
    fRingRead.store((r + len) % kRingBufSize, std::memory_order_release);
    return len;
}

// ---------------------------------------------------------------------------
// Construction / Open / Close
// ---------------------------------------------------------------------------

plNetworkAudioStream::plNetworkAudioStream()
    : fRing(kRingBufSize)
{
}

plNetworkAudioStream::~plNetworkAudioStream()
{
    Close();
}

bool plNetworkAudioStream::Open(const ST::string& url, float volume, bool positional)
{
#ifdef USE_MPG123
    Close();
    fVolume = volume;
    fPositional = positional;
    fStopRequested.store(false, std::memory_order_relaxed);
    fRingWrite.store(0, std::memory_order_relaxed);
    fRingRead.store(0, std::memory_order_relaxed);
    fFormatReady.store(false, std::memory_order_relaxed);
    fALReady = false;

    fThread = std::thread(&plNetworkAudioStream::IDownloadThread, this, url);
    return true;
#else
    plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: built without USE_MPG123");
    return false;
#endif
}

void plNetworkAudioStream::Close()
{
    fStopRequested.store(true, std::memory_order_relaxed);
    if (fThread.joinable())
        fThread.join();
    IDestroyAL();
    fValid.store(false, std::memory_order_relaxed);
    fFormatReady.store(false, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Download / decode thread (runs on background thread)
// ---------------------------------------------------------------------------

#ifdef USE_MPG123
size_t plNetworkAudioStream::SCurlWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* self = static_cast<plNetworkAudioStream*>(userdata);
    if (self->fStopRequested.load(std::memory_order_relaxed))
        return 0; // returning 0 aborts curl

    size_t total = size * nmemb;
    self->IFeedAndDecode(reinterpret_cast<const uint8_t*>(ptr), total);
    return total;
}

bool plNetworkAudioStream::IFeedAndDecode(const uint8_t* data, size_t size)
{
    if (mpg123_feed(fMpg123, data, size) != MPG123_OK)
        return false;

    uint8_t pcm[4096];
    size_t  done = 0;
    int     ret;

    while (!fStopRequested.load(std::memory_order_relaxed)) {
        ret = mpg123_read(fMpg123, pcm, sizeof(pcm), &done);

        if (ret == MPG123_NEW_FORMAT) {
            long rate; int channels, encoding;
            mpg123_getformat(fMpg123, &rate, &channels, &encoding);
            fSampleRate = rate;
            fChannels   = channels;
            // Lock the format so it doesn't change again
            mpg123_format_none(fMpg123);
            mpg123_format(fMpg123, rate, channels, MPG123_ENC_SIGNED_16);
            fFormatReady.store(true, std::memory_order_release);
        }

        if (done > 0) {
            // If ring buffer is full, wait briefly rather than dropping audio
            size_t written = 0;
            while (!fStopRequested.load(std::memory_order_relaxed) && written < done) {
                written += IRingWrite(pcm + written, done - written);
                if (written < done)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        }

        if (ret == MPG123_NEED_MORE || ret == MPG123_DONE || ret != MPG123_OK)
            break;
    }
    return true;
}
#endif // USE_MPG123

// ---------------------------------------------------------------------------
// Playlist resolution (M3U / PLS → actual stream URL)
// ---------------------------------------------------------------------------

#ifdef USE_MPG123
static size_t SPlaylistWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buf = static_cast<std::string*>(userdata);
    if (buf->size() < 65536)
        buf->append(ptr, size * nmemb);
    return size * nmemb;
}

static ST::string IResolvePlaylistUrl(const ST::string& url)
{
    // Only probe if the path ends with a known playlist extension
    std::string lower = url.to_lower().c_str();
    bool isPlaylist = (lower.size() >= 4 &&
        (lower.substr(lower.size() - 4) == ".m3u" ||
         lower.substr(lower.size() - 4) == ".pls")) ||
        (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".m3u8");
    if (!isPlaylist)
        return url;

    CURL* curl = curl_easy_init();
    if (!curl) return url;

    std::string buf;
    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SPlaylistWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "PlasmaClient/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       10L);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    std::istringstream ss(buf);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        // PLS: File1=http://...
        if (line.rfind("File", 0) == 0) {
            auto eq = line.find('=');
            if (eq != std::string::npos)
                line = line.substr(eq + 1);
        }

        if (line.rfind("http://", 0) == 0 || line.rfind("https://", 0) == 0) {
            plStatusLog::AddLineSF("audio.log",
                "plNetworkAudioStream: resolved playlist -> {}", line.c_str());
            return ST::string(line.c_str());
        }
    }

    plStatusLog::AddLineS("audio.log",
        "plNetworkAudioStream: playlist parse found no stream URL, using original");
    return url;
}
#endif // USE_MPG123

void plNetworkAudioStream::IDownloadThread(ST::string url)
{
#ifdef USE_MPG123
    url = IResolvePlaylistUrl(url);

    int err = MPG123_OK;
    fMpg123 = mpg123_new(nullptr, &err);
    if (!fMpg123) {
        plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: mpg123_new failed: {}",
                               mpg123_plain_strerror(err));
        return;
    }
    // Allow all common sample rates. Force mono for positional streams —
    // OpenAL only applies 3D positioning to mono sources.
    mpg123_format_none(fMpg123);
    static const long kRates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 0 };
    int channels = fPositional ? MPG123_MONO : (MPG123_STEREO | MPG123_MONO);
    for (int i = 0; kRates[i]; ++i)
        mpg123_format(fMpg123, kRates[i], channels, MPG123_ENC_SIGNED_16);
    mpg123_open_feed(fMpg123);

    CURL* curl = curl_easy_init();
    if (!curl) {
        plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: curl_easy_init failed");
        mpg123_delete(fMpg123);
        fMpg123 = nullptr;
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL,           url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SCurlWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     this);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,     "PlasmaClient/1.0");
    // Icy streams send headers that curl must pass through to detect Icecast metadata
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,  CURL_HTTP_VERSION_1_1);
    // Don't time out the connection — live streams run indefinitely
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: connecting to {}", url);
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK && !fStopRequested.load(std::memory_order_relaxed))
        plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: curl error: {}",
                               curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    mpg123_close(fMpg123);
    mpg123_delete(fMpg123);
    fMpg123 = nullptr;
    fValid.store(false, std::memory_order_release);
    plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: stream ended");
#endif
}

// ---------------------------------------------------------------------------
// OpenAL management (main thread)
// ---------------------------------------------------------------------------

bool plNetworkAudioStream::IInitAL()
{
#ifdef USE_MPG123
    fALFormat = (fChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

    alGenSources(1, &fALSource);
    alGenBuffers(kNumALBufs, fALBufs);
    alSourcef(fALSource, AL_GAIN, fVolume);

    alSourcef(fALSource, AL_ROLLOFF_FACTOR, 0.3048f);  // same as all other Plasma sounds
    if (fHasPosition) {
        alSourcei(fALSource, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(fALSource, AL_POSITION, fPosX, fPosZ, -fPosY);  // Plasma(X,Y,Z) → OpenAL(X,Z,-Y)
        alSourcef(fALSource, AL_REFERENCE_DISTANCE, fMinDist);
        alSourcef(fALSource, AL_MAX_DISTANCE, fMaxDist);
    } else {
        // No position set → listener-relative 2D
        alSourcei(fALSource, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(fALSource, AL_POSITION, 0.f, 0.f, 0.f);
    }

    // Pre-fill all buffers; use silence if ring buffer not yet ready
    uint8_t silence[kALBufBytes] = {};
    for (int i = 0; i < kNumALBufs; i++) {
        if (IRingAvailable() >= kALBufBytes)
            IFillAndQueueALBuf(fALBufs[i]);
        else {
            alBufferData(fALBufs[i], fALFormat, silence, (ALsizei)kALBufBytes,
                         (ALsizei)fSampleRate);
            alSourceQueueBuffers(fALSource, 1, &fALBufs[i]);
        }
    }

    alSourcePlay(fALSource);
    fALReady = true;
    fValid.store(true, std::memory_order_release);
    plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: playing {}Hz {}ch",
                           fSampleRate, fChannels);
    return true;
#else
    return false;
#endif
}

void plNetworkAudioStream::IFillAndQueueALBuf(unsigned int bufId)
{
#ifdef USE_MPG123
    uint8_t data[kALBufBytes] = {};
    size_t  got = IRingRead(data, kALBufBytes);
    // If we got less than a full buffer, the rest stays as silence
    alBufferData(bufId, fALFormat, data, (ALsizei)got, (ALsizei)fSampleRate);
    alSourceQueueBuffers(fALSource, 1, &bufId);
#endif
}

void plNetworkAudioStream::IDestroyAL()
{
#ifdef USE_MPG123
    if (!fALReady) return;
    alSourceStop(fALSource);
    ALint queued = 0;
    alGetSourcei(fALSource, AL_BUFFERS_QUEUED, &queued);
    while (queued-- > 0) {
        ALuint buf;
        alSourceUnqueueBuffers(fALSource, 1, &buf);
    }
    alDeleteBuffers(kNumALBufs, fALBufs);
    alDeleteSources(1, &fALSource);
    fALSource = 0;
    fALReady  = false;
#endif
}

// ---------------------------------------------------------------------------
// Update — called every frame from the main thread
// ---------------------------------------------------------------------------

void plNetworkAudioStream::Update()
{
#ifdef USE_MPG123
    if (!fFormatReady.load(std::memory_order_acquire))
        return; // Still waiting for first MP3 frame

    if (!fALReady) {
        if (!IInitAL())
            return;
    }

    // Unqueue processed buffers and refill them
    ALint processed = 0;
    alGetSourcei(fALSource, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint buf;
        alSourceUnqueueBuffers(fALSource, 1, &buf);
        if (IRingAvailable() > 0)
            IFillAndQueueALBuf(buf);
        // else: buffer is not re-queued — source will stall until data arrives
    }

    // Restart source if it stalled due to buffer underrun
    ALint state = 0;
    alGetSourcei(fALSource, AL_SOURCE_STATE, &state);
    if (state != AL_PLAYING) {
        ALint queued = 0;
        alGetSourcei(fALSource, AL_BUFFERS_QUEUED, &queued);
        if (queued > 0)
            alSourcePlay(fALSource);
    }
#endif
}

bool plNetworkAudioStream::IsPlaying() const
{
#ifdef USE_MPG123
    if (!fALReady) return false;
    ALint state = 0;
    alGetSourcei(fALSource, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
#else
    return false;
#endif
}

void plNetworkAudioStream::SetVolume(float vol)
{
    fVolume = vol;
#ifdef USE_MPG123
    if (fALReady)
        alSourcef(fALSource, AL_GAIN, vol);
#endif
}

void plNetworkAudioStream::SetPosition(float x, float y, float z, float minDist, float maxDist)
{
    fPosX = x; fPosY = y; fPosZ = z;
    fMinDist = minDist; fMaxDist = maxDist;
    fHasPosition = true;
#ifdef USE_MPG123
    if (fALReady) {
        alSourcei(fALSource, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(fALSource, AL_POSITION, x, z, -y);  // Plasma(X,Y,Z) → OpenAL(X,Z,-Y)
        alSourcef(fALSource, AL_REFERENCE_DISTANCE, minDist);
        alSourcef(fALSource, AL_MAX_DISTANCE, maxDist);
    }
#endif
}
