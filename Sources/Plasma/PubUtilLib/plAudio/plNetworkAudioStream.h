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
#include <thread>
#include <vector>

// Plays a live HTTP/Icecast MP3 audio stream through OpenAL.
// Downloads via libcurl, decodes via libmpg123, buffers PCM in a lock-free
// ring buffer, and feeds OpenAL from the main thread via Update().
class plNetworkAudioStream
{
public:
    plNetworkAudioStream();
    ~plNetworkAudioStream();

    // Open and start streaming from a URL (http:// or https://).
    // positional=true forces mono decode so OpenAL can apply 3D positioning.
    // Returns false immediately if mpg123/curl are unavailable.
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
    // If never called, plays as 2D (listener-relative).
    void SetPosition(float x, float y, float z, float minDist = 15.f, float maxDist = 10000.f);

    bool IsPlaying() const;
    bool IsValid()   const { return fValid.load(std::memory_order_relaxed); }

private:
    // Ring buffer capacity: ~4 MB — approx. 23 s of stereo 44.1 kHz 16-bit PCM.
    static constexpr size_t kRingBufSize = 4 * 1024 * 1024;
    static constexpr int    kNumALBufs   = 8;
    static constexpr size_t kALBufBytes  = 16 * 1024; // 16 KB per OpenAL buffer

    // --- download / decode thread ---
    struct mpg123_handle_struct* fMpg123 = nullptr;
    std::thread      fThread;
    std::atomic<bool> fStopRequested{false};
    std::atomic<bool> fValid{false};

    // Format discovered after first MP3 frame
    std::atomic<bool> fFormatReady{false};
    long fSampleRate = 44100;
    int  fChannels   = 1;

    // Lock-free SPSC ring buffer (one writer: download thread; one reader: main thread)
    std::vector<uint8_t> fRing;
    std::atomic<size_t>  fRingWrite{0};
    std::atomic<size_t>  fRingRead{0};

    size_t IRingAvailable() const;
    size_t IRingFree()      const;
    size_t IRingWrite(const uint8_t* data, size_t len);
    size_t IRingRead(uint8_t* out,   size_t len);

    // --- OpenAL ---
    unsigned int  fALSource = 0;
    unsigned int  fALBufs[kNumALBufs] = {};
    bool          fALReady  = false;
    float         fVolume   = 1.0f;
    int           fALFormat = 0; // AL_FORMAT_MONO16 or AL_FORMAT_STEREO16

    float         fPosX = 0.f, fPosY = 0.f, fPosZ = 0.f;
    float         fMinDist = 15.f, fMaxDist = 10000.f;
    bool          fHasPosition = false;
    bool          fPositional = false;
    bool          fMuted = false;

    bool IInitAL();
    void IFillAndQueueALBuf(unsigned int bufId);
    void IDestroyAL();

    // --- curl / mpg123 thread ---
    void IDownloadThread(ST::string url);
    bool IFeedAndDecode(const uint8_t* data, size_t size);
    static size_t SCurlWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata);
};

#endif // plNetworkAudioStream_h
