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
#endif

#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
#   include <curl/curl.h>
#   include <AL/al.h>
#endif

#include "plStatusLog/plStatusLog.h"

#include <algorithm>
#include <chrono>
#include <cstring>
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
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
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
    plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: built without audio stream support");
    return false;
#endif
}

void plNetworkAudioStream::Close()
{
    fStopRequested.store(true, std::memory_order_relaxed);

#ifdef USE_VORBIS_STREAM
    // Unblock the OGG read callback so the decode thread can exit
    {
        std::lock_guard<std::mutex> lock(fOggFeedMutex);
        fOggCurlDone = true;
    }
    fOggFeedCV.notify_all();
    // fOggDecodeThread is joined inside IDownloadThreadOgg before fThread exits
#endif

    if (fThread.joinable())
        fThread.join();

    IDestroyAL();
    fValid.store(false, std::memory_order_relaxed);
    fFormatReady.store(false, std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// URL helpers
// ---------------------------------------------------------------------------

bool plNetworkAudioStream::IIsOggUrl(const ST::string& url)
{
    std::string lower = url.to_lower().c_str();
    // Strip query string before checking extension
    auto q = lower.find('?');
    if (q != std::string::npos)
        lower = lower.substr(0, q);
    return lower.size() >= 4 &&
           (lower.substr(lower.size() - 4) == ".ogg" ||
            lower.substr(lower.size() - 4) == ".oga");
}

// ---------------------------------------------------------------------------
// Curl progress callback — aborts curl when stop is requested.
// Called at least once per second by curl regardless of data flow,
// so Close() unblocks within ~1 s even on a stalled live stream socket.
// ---------------------------------------------------------------------------

#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
int plNetworkAudioStream::SCurlProgressCb(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    auto* self = static_cast<plNetworkAudioStream*>(userdata);
    return self->fStopRequested.load(std::memory_order_relaxed) ? 1 : 0;
}
#endif

// ---------------------------------------------------------------------------
// Playlist resolution (M3U / PLS → actual stream URL)  [shared by both paths]
// ---------------------------------------------------------------------------

#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
static size_t SPlaylistWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* buf = static_cast<std::string*>(userdata);
    if (buf->size() < 65536)
        buf->append(ptr, size * nmemb);
    return size * nmemb;
}

static ST::string IResolvePlaylistUrl(const ST::string& url)
{
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
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  SPlaylistWriteCb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "PlasmaClient/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
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
#endif // USE_MPG123 || USE_VORBIS_STREAM

// ---------------------------------------------------------------------------
// Content-Type probe — HEAD request to detect OGG when URL has no extension
// ---------------------------------------------------------------------------

#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
static size_t SHeaderCollectCb(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* out = static_cast<std::string*>(userdata);
    std::string line(ptr, size * nmemb);
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower.rfind("content-type:", 0) == 0) {
        *out = line.substr(13);
        auto s = out->find_first_not_of(" \t\r\n");
        if (s != std::string::npos) *out = out->substr(s);
        auto e = out->find_last_not_of(" \t\r\n");
        if (e != std::string::npos) *out = out->substr(0, e + 1);
    }
    return size * nmemb;
}

// Write callback that aborts immediately — used so curl still processes
// response headers from a GET without downloading the stream body.
static size_t SProbeAbortWriteCb(char*, size_t, size_t, void*)
{
    return 0; // returning 0 causes CURLE_WRITE_ERROR, aborting the transfer
}

static bool IProbeIsOgg(const ST::string& url)
{
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::string contentType;
    curl_easy_setopt(curl, CURLOPT_URL,            url.c_str());
    // Use a real GET (not HEAD) so Icecast servers send accurate Content-Type
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  SProbeAbortWriteCb);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,      "PlasmaClient/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,        10L);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, SHeaderCollectCb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA,     &contentType);
    // CURLE_WRITE_ERROR is expected — ignore it
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    std::string lower = contentType;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    bool isOgg = lower.find("ogg") != std::string::npos ||
                 lower.find("vorbis") != std::string::npos;

    plStatusLog::AddLineSF("audio.log",
        "plNetworkAudioStream: probed Content-Type: '{}' -> {}",
        contentType.empty() ? "(none)" : contentType.c_str(),
        isOgg ? "OGG" : "MP3");
    return isOgg;
}
#endif // USE_MPG123 || USE_VORBIS_STREAM

// ---------------------------------------------------------------------------
// IDownloadThread — dispatches to MP3 or OGG path
// ---------------------------------------------------------------------------

void plNetworkAudioStream::IDownloadThread(ST::string url)
{
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
    url = IResolvePlaylistUrl(url);

    // Prefer URL extension; fall back to a HEAD probe when no extension present
    bool isOgg = IIsOggUrl(url) || IProbeIsOgg(url);

#   ifdef USE_VORBIS_STREAM
    if (isOgg) {
        IDownloadThreadOgg(url);
        return;
    }
#   endif

#   ifdef USE_MPG123
    IDownloadThreadMP3(url);
#   else
    plStatusLog::AddLineSF("audio.log",
        "plNetworkAudioStream: no MP3 decoder available (built without USE_MPG123): {}", url);
#   endif
#endif
}

// ---------------------------------------------------------------------------
// MP3 path — libmpg123 + libcurl
// ---------------------------------------------------------------------------

#ifdef USE_MPG123
size_t plNetworkAudioStream::SCurlWriteCbMP3(char* ptr, size_t size, size_t nmemb, void* userdata)
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
            // Lock the format so it doesn't change mid-stream
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

void plNetworkAudioStream::IDownloadThreadMP3(const ST::string& url)
{
    int err = MPG123_OK;
    fMpg123 = mpg123_new(nullptr, &err);
    if (!fMpg123) {
        plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: mpg123_new failed: {}",
                               mpg123_plain_strerror(err));
        return;
    }
    // Allow all common sample rates. Force mono for positional streams.
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

    curl_easy_setopt(curl, CURLOPT_URL,              url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    SCurlWriteCbMP3);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        this);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "PlasmaClient/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,     CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, SCurlProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     this);

    plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: connecting (MP3) to {}", url);
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK && !fStopRequested.load(std::memory_order_relaxed))
        plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: curl error: {}",
                               curl_easy_strerror(res));

    curl_easy_cleanup(curl);
    mpg123_close(fMpg123);
    mpg123_delete(fMpg123);
    fMpg123 = nullptr;
    fValid.store(false, std::memory_order_release);
    plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: MP3 stream ended");
}
#endif // USE_MPG123

// ---------------------------------------------------------------------------
// OGG/Vorbis path — libvorbisfile + libcurl (two-thread: curl + ov_read)
// ---------------------------------------------------------------------------

#ifdef USE_VORBIS_STREAM
size_t plNetworkAudioStream::SCurlWriteCbOgg(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    auto* self = static_cast<plNetworkAudioStream*>(userdata);
    if (self->fStopRequested.load(std::memory_order_relaxed))
        return 0; // abort curl

    size_t total = size * nmemb;
    {
        std::lock_guard<std::mutex> lock(self->fOggFeedMutex);
        const uint8_t* src = reinterpret_cast<const uint8_t*>(ptr);
        self->fOggFeedBuf.insert(self->fOggFeedBuf.end(), src, src + total);
    }
    self->fOggFeedCV.notify_one();
    return total;
}

// ov_callbacks: read — waits until data is available in the feed buffer
size_t plNetworkAudioStream::SOggRead(void* ptr, size_t size, size_t nmemb, void* ds)
{
    auto* self = static_cast<plNetworkAudioStream*>(ds);
    size_t want = size * nmemb;
    if (want == 0)
        return 0;

    std::unique_lock<std::mutex> lock(self->fOggFeedMutex);
    self->fOggFeedCV.wait(lock, [&] {
        return self->fStopRequested.load(std::memory_order_relaxed) ||
               self->fOggCurlDone ||
               (self->fOggFeedBuf.size() - self->fOggFeedPos) > 0;
    });

    if (self->fStopRequested.load(std::memory_order_relaxed))
        return 0;

    size_t avail  = self->fOggFeedBuf.size() - self->fOggFeedPos;
    size_t toRead = std::min(want, avail);
    std::memcpy(ptr, self->fOggFeedBuf.data() + self->fOggFeedPos, toRead);
    self->fOggFeedPos += toRead;

    // Compact consumed bytes from the front once a threshold is reached
    if (self->fOggFeedPos > 256 * 1024) {
        self->fOggFeedBuf.erase(self->fOggFeedBuf.begin(),
                                 self->fOggFeedBuf.begin() + self->fOggFeedPos);
        self->fOggFeedPos = 0;
    }

    return toRead;
}

// ov_callbacks: seek / tell — not supported for live HTTP streams
int  plNetworkAudioStream::SOggSeek(void*, ogg_int64_t, int) { return -1; }
long plNetworkAudioStream::SOggTell(void*)                    { return -1; }

void plNetworkAudioStream::IOggDecodeThread()
{
    OggVorbis_File vf;
    ov_callbacks cb;
    cb.read_func  = SOggRead;
    cb.seek_func  = SOggSeek;
    cb.close_func = nullptr;
    cb.tell_func  = SOggTell;

    int ret = ov_open_callbacks(this, &vf, nullptr, 0, cb);
    if (ret != 0) {
        if (!fStopRequested.load(std::memory_order_relaxed))
            plStatusLog::AddLineSF("audio.log",
                "plNetworkAudioStream: ov_open_callbacks failed ({})", ret);
        fValid.store(false, std::memory_order_release);
        return;
    }

    vorbis_info* info = ov_info(&vf, -1);
    fSampleRate = info->rate;
    // Force mono for 3D-positional sources; OpenAL only spatialises mono
    int srcChannels = info->channels;
    fChannels = fPositional ? 1 : srcChannels;
    fFormatReady.store(true, std::memory_order_release);

    plStatusLog::AddLineSF("audio.log",
        "plNetworkAudioStream: OGG {}Hz {}ch", fSampleRate, fChannels);

    // 4096 stereo 16-bit samples = 16 KB
    static constexpr int kPcmFrames = 4096;
    char pcm[kPcmFrames * 2 * sizeof(int16_t)];
    int  bitstream = 0;

    while (!fStopRequested.load(std::memory_order_relaxed)) {
        long got = ov_read(&vf, pcm, (int)sizeof(pcm),
                           0,  // little-endian
                           2,  // 16-bit samples
                           1,  // signed
                           &bitstream);

        if (got == OV_HOLE)  continue;
        if (got <= 0)        break; // OV_EOSTREAM, OV_EBADLINK, or end

        size_t toWrite = (size_t)got;

        // Downmix stereo → mono for positional streams
        if (fPositional && srcChannels == 2) {
            int16_t* s = reinterpret_cast<int16_t*>(pcm);
            size_t frames = (size_t)got / (2 * sizeof(int16_t));
            for (size_t i = 0; i < frames; i++)
                s[i] = (int16_t)(((int32_t)s[i * 2] + s[i * 2 + 1]) / 2);
            toWrite = frames * sizeof(int16_t);
        }

        // Write to ring buffer; wait if full
        size_t written = 0;
        while (!fStopRequested.load(std::memory_order_relaxed) && written < toWrite) {
            written += IRingWrite(reinterpret_cast<const uint8_t*>(pcm) + written,
                                  toWrite - written);
            if (written < toWrite)
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    ov_clear(&vf);
    fValid.store(false, std::memory_order_release);
    plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: OGG stream ended");
}

void plNetworkAudioStream::IDownloadThreadOgg(const ST::string& url)
{
    {
        std::lock_guard<std::mutex> lock(fOggFeedMutex);
        fOggFeedBuf.clear();
        fOggFeedPos  = 0;
        fOggCurlDone = false;
    }

    // Start the decode thread first so it's waiting when curl delivers data
    fOggDecodeThread = std::thread(&plNetworkAudioStream::IOggDecodeThread, this);

    CURL* curl = curl_easy_init();
    if (!curl) {
        plStatusLog::AddLineS("audio.log", "plNetworkAudioStream: curl_easy_init failed (OGG)");
        {
            std::lock_guard<std::mutex> lock(fOggFeedMutex);
            fOggCurlDone = true;
        }
        fOggFeedCV.notify_all();
        if (fOggDecodeThread.joinable())
            fOggDecodeThread.join();
        return;
    }

    curl_easy_setopt(curl, CURLOPT_URL,              url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,    SCurlWriteCbOgg);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,        this);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,   1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT,        "PlasmaClient/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION,     CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,          0L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS,       0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, SCurlProgressCb);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA,     this);

    plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: connecting (OGG) to {}", url);
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK && !fStopRequested.load(std::memory_order_relaxed))
        plStatusLog::AddLineSF("audio.log", "plNetworkAudioStream: curl error: {}",
                               curl_easy_strerror(res));

    curl_easy_cleanup(curl);

    // Signal the decode thread that no more data is coming
    {
        std::lock_guard<std::mutex> lock(fOggFeedMutex);
        fOggCurlDone = true;
    }
    fOggFeedCV.notify_all();

    if (fOggDecodeThread.joinable())
        fOggDecodeThread.join();
}
#endif // USE_VORBIS_STREAM

// ---------------------------------------------------------------------------
// OpenAL management (main thread)
// ---------------------------------------------------------------------------

#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
bool plNetworkAudioStream::IInitAL()
{
    fALFormat = (fChannels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

    alGenSources(1, &fALSource);
    alGenBuffers(kNumALBufs, fALBufs);
    alSourcef(fALSource, AL_GAIN, fMuted ? 0.f : fVolume);

    alSourcef(fALSource, AL_ROLLOFF_FACTOR, 0.3048f); // same as all other Plasma sounds
    if (fHasPosition) {
        alSourcei(fALSource, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(fALSource, AL_POSITION, fPosX, fPosZ, -fPosY); // Plasma(X,Y,Z) → OpenAL(X,Z,-Y)
        alSourcef(fALSource, AL_REFERENCE_DISTANCE, fMinDist);
        alSourcef(fALSource, AL_MAX_DISTANCE, fMaxDist);
    } else {
        // No position → listener-relative 2D
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
}

void plNetworkAudioStream::IFillAndQueueALBuf(unsigned int bufId)
{
    uint8_t data[kALBufBytes] = {};
    size_t  got = IRingRead(data, kALBufBytes);
    alBufferData(bufId, fALFormat, data, (ALsizei)got, (ALsizei)fSampleRate);
    alSourceQueueBuffers(fALSource, 1, &bufId);
}

void plNetworkAudioStream::IDestroyAL()
{
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
}
#else
bool plNetworkAudioStream::IInitAL()           { return false; }
void plNetworkAudioStream::IFillAndQueueALBuf(unsigned int) {}
void plNetworkAudioStream::IDestroyAL()        {}
#endif

// ---------------------------------------------------------------------------
// Update — called every frame from the main thread
// ---------------------------------------------------------------------------

void plNetworkAudioStream::Update()
{
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
    if (!fFormatReady.load(std::memory_order_acquire))
        return;

    if (!fALReady) {
        if (!IInitAL())
            return;
    }

    ALint processed = 0;
    alGetSourcei(fALSource, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0) {
        ALuint buf;
        alSourceUnqueueBuffers(fALSource, 1, &buf);
        if (IRingAvailable() > 0)
            IFillAndQueueALBuf(buf);
    }

    // Restart if stalled due to buffer underrun
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
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
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
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
    if (fALReady && !fMuted)
        alSourcef(fALSource, AL_GAIN, vol);
#endif
}

void plNetworkAudioStream::SetMuted(bool muted)
{
    fMuted = muted;
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
    if (fALReady)
        alSourcef(fALSource, AL_GAIN, muted ? 0.f : fVolume);
#endif
}

void plNetworkAudioStream::SetPosition(float x, float y, float z, float minDist, float maxDist)
{
    fPosX = x; fPosY = y; fPosZ = z;
    fMinDist = minDist; fMaxDist = maxDist;
    fHasPosition = true;
#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
    if (fALReady) {
        alSourcei(fALSource, AL_SOURCE_RELATIVE, AL_FALSE);
        alSource3f(fALSource, AL_POSITION, x, z, -y); // Plasma(X,Y,Z) → OpenAL(X,Z,-Y)
        alSourcef(fALSource, AL_REFERENCE_DISTANCE, minDist);
        alSourcef(fALSource, AL_MAX_DISTANCE, maxDist);
    }
#endif
}
