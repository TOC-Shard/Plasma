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
#ifndef plNetworkAudioStream_h
#define plNetworkAudioStream_h

#include "HeadSpin.h"
#include <string_theory/string>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#if defined(USE_MPG123) || defined(USE_VORBIS_STREAM)
#   include <curl/curl.h>
#endif
#ifdef USE_VORBIS_STREAM
#   include <vorbis/vorbisfile.h>
#endif

// Plays a live HTTP/Icecast audio stream (MP3 or OGG/Vorbis) through OpenAL.
// Downloads via libcurl. MP3 decoded via libmpg123, OGG via libvorbisfile.
// PCM is buffered in a lock-free ring buffer and fed to OpenAL from the main thread.
class plNetworkAudioStream
{
public:
    plNetworkAudioStream();
    ~plNetworkAudioStream();

    // Open and start streaming from a URL (http:// or https://).
    // Format is detected from the URL extension (.ogg/.oga → Vorbis, else → MP3).
    // positional=true forces mono so OpenAL can apply 3D positioning.
    bool Open(const ST::string& url, float volume = 1.0f, bool positional = false);

    // Stop playback and release all resources.
    void Close();

    // Must be called every frame from the main/audio thread.
    void Update();

    void  SetVolume(float vol);
    float GetVolume() const { return fVolume; }
    void  SetMuted(bool muted);

    // Set 3D world-space position. Call after Open().
    // minDist: full-volume radius; maxDist: silence beyond this.
    void SetPosition(float x, float y, float z, float minDist = 15.f, float maxDist = 10000.f);

    bool IsPlaying() const;
    bool IsValid()   const { return fValid.load(std::memory_order_relaxed); }

private:
    // Ring buffer capacity: ~4 MB — approx. 23 s of stereo 44.1 kHz 16-bit PCM.
    static constexpr size_t kRingBufSize = 4 * 1024 * 1024;
    static constexpr int    kNumALBufs   = 8;
    static constexpr size_t kALBufBytes  = 16 * 1024;

    // --- shared state ---
    std::thread       fThread;
    std::atomic<bool> fStopRequested{false};
    std::atomic<bool> fValid{false};

    // Format discovered after the first decoded frame
    std::atomic<bool> fFormatReady{false};
    long fSampleRate = 44100;
    int  fChannels   = 1;

    // Lock-free SPSC ring buffer (writer: download/decode thread; reader: main thread)
    std::vector<uint8_t> fRing;
    std::atomic<size_t>  fRingWrite{0};
    std::atomic<size_t>  fRingRead{0};

    size_t IRingAvailable() const;
    size_t IRingFree()      const;
    size_t IRingWrite(const uint8_t* data, size_t len);
    size_t IRingRead(uint8_t* out, size_t len);

    // --- OpenAL ---
    unsigned int fALSource = 0;
    unsigned int fALBufs[kNumALBufs] = {};
    bool         fALReady  = false;
    float        fVolume   = 1.0f;
    int          fALFormat = 0; // AL_FORMAT_MONO16 or AL_FORMAT_STEREO16
    float        fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    float        fMinDist = 15.f, fMaxDist = 10000.f;
    bool         fHasPosition = false;
    bool         fPositional  = false;
    bool         fMuted       = false;

    bool IInitAL();
    void IFillAndQueueALBuf(unsigned int bufId);
    void IDestroyAL();

    // --- common download helpers ---
    void IDownloadThread(ST::string url);
    static bool IIsOggUrl(const ST::string& url);
    static int  SCurlProgressCb(void* userdata, curl_off_t dltotal, curl_off_t dlnow,
                                 curl_off_t ultotal, curl_off_t ulnow);

    // --- MP3 path (libmpg123 + libcurl) ---
#ifdef USE_MPG123
    struct mpg123_handle_struct* fMpg123 = nullptr;

    void IDownloadThreadMP3(const ST::string& url);
    bool IFeedAndDecode(const uint8_t* data, size_t size);
    static size_t SCurlWriteCbMP3(char* ptr, size_t size, size_t nmemb, void* userdata);
#endif

    // --- OGG/Vorbis path (libvorbisfile + libcurl) ---
#ifdef USE_VORBIS_STREAM
    // Intermediate feed buffer: curl writes here, vorbisfile reads via callbacks.
    std::vector<uint8_t>    fOggFeedBuf;
    size_t                  fOggFeedPos  = 0;
    std::mutex              fOggFeedMutex;
    std::condition_variable fOggFeedCV;
    bool                    fOggCurlDone = false;
    std::thread             fOggDecodeThread;

    void IDownloadThreadOgg(const ST::string& url);
    void IOggDecodeThread();

    static size_t SCurlWriteCbOgg(char* ptr, size_t size, size_t nmemb, void* userdata);

    // ov_callbacks
    static size_t SOggRead(void* ptr, size_t size, size_t nmemb, void* ds);
    static int    SOggSeek(void* ds, ogg_int64_t offset, int whence);
    static long   SOggTell(void* ds);
#endif
};

#endif // plNetworkAudioStream_h
