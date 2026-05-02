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

#include "plMP3Codec.h"

#ifdef USE_MPG123
#   include <mpg123.h>
#endif

#include "plStatusLog/plStatusLog.h"

#include <climits>
#include <cstring>
#include <mutex>

#ifdef USE_MPG123
static std::once_flag s_mpg123Init;
static void IMpg123Init() {
    std::call_once(s_mpg123Init, []() {
        mpg123_init();
        std::atexit([]() { mpg123_exit(); });
    });
}
#endif

plMP3Codec::plMP3Codec(const plFileName& path, plAudioCore::ChannelSelect whichChan)
{
#ifdef USE_MPG123
    IMpg123Init();
    int err = MPG123_OK;
    fHandle = mpg123_new(nullptr, &err);
    if (!fHandle) {
        plStatusLog::AddLineSF("audio.log", "plMP3Codec: mpg123_new failed: {}", mpg123_plain_strerror(err));
        return;
    }

    // Restrict output to signed 16-bit PCM before opening so mpg123 honours it
    // during the initial scan rather than requiring a mid-stream format change
    // (which can leave the seek position at EOF on some builds).
    mpg123_format_none(fHandle);
    // Allow all standard rates, mono or stereo, signed 16-bit only
    static const long kRates[] = { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 0 };
    for (int i = 0; kRates[i]; ++i)
        mpg123_format(fHandle, kRates[i], MPG123_STEREO | MPG123_MONO, MPG123_ENC_SIGNED_16);

    if (mpg123_open(fHandle, path.AsString().c_str()) != MPG123_OK) {
        plStatusLog::AddLineSF("audio.log", "plMP3Codec: cannot open '{}': {}",
                               path.AsString(), mpg123_strerror(fHandle));
        mpg123_delete(fHandle);
        fHandle = nullptr;
        return;
    }

    // Scan for an accurate frame count; read the format mpg123 chose
    if (mpg123_scan(fHandle) != MPG123_OK)
        plStatusLog::AddLineSF("audio.log", "plMP3Codec: scan warning for '{}'", path.AsString());

    long rate; int channels, encoding;
    if (mpg123_getformat(fHandle, &rate, &channels, &encoding) != MPG123_OK) {
        plStatusLog::AddLineSF("audio.log", "plMP3Codec: getformat failed for '{}'", path.AsString());
        mpg123_close(fHandle);
        mpg123_delete(fHandle);
        fHandle = nullptr;
        return;
    }

    // Capture length while scan index is fresh (before any further state changes)
    fTotalSamples = mpg123_length(fHandle);
    if (fTotalSamples < 0)
        fTotalSamples = 0;  // Unknown length; EOF detection will stop playback

    // Seek back to the start
    int64_t seekResult = mpg123_seek(fHandle, 0, SEEK_SET);
    if (seekResult < 0) {
        plStatusLog::AddLineSF("audio.log", "plMP3Codec: seek to 0 failed for '{}': {}",
                               path.AsString(), mpg123_strerror(fHandle));
        mpg123_close(fHandle);
        mpg123_delete(fHandle);
        fHandle = nullptr;
        return;
    }

    uint16_t bytesPerSample = static_cast<uint16_t>(channels * 2); // 16-bit
    fHeader.fFormatTag        = 1; // PCM
    fHeader.fNumChannels      = static_cast<uint16_t>(channels);
    fHeader.fNumSamplesPerSec = static_cast<uint32_t>(rate);
    fHeader.fBitsPerSample    = 16;
    fHeader.fBlockAlign       = bytesPerSample;
    fHeader.fAvgBytesPerSec   = static_cast<uint32_t>(rate) * bytesPerSample;

    fValid = true;
#endif
}

plMP3Codec::~plMP3Codec()
{
    Close();
}

void plMP3Codec::Close()
{
#ifdef USE_MPG123
    if (fHandle) {
        mpg123_close(fHandle);
        mpg123_delete(fHandle);
        fHandle = nullptr;
    }
#endif
    fValid = false;
}

uint32_t plMP3Codec::GetDataSize()
{
#ifdef USE_MPG123
    if (!fValid) return 0;
    if (fTotalSamples > 0)
        return static_cast<uint32_t>(fTotalSamples) * fHeader.fBlockAlign;
    return UINT32_MAX / 2;  // Unknown length; large placeholder
#else
    return 0;
#endif
}

float plMP3Codec::GetLengthInSecs()
{
#ifdef USE_MPG123
    if (!fValid || fHeader.fNumSamplesPerSec == 0) return 0.f;
    return static_cast<float>(fTotalSamples) / static_cast<float>(fHeader.fNumSamplesPerSec);
#else
    return 0.f;
#endif
}

bool plMP3Codec::SetPosition(uint32_t numBytes)
{
#ifdef USE_MPG123
    if (!fValid) return false;
    int64_t sample = numBytes / fHeader.fBlockAlign;
    bool ok = mpg123_seek(fHandle, sample, SEEK_SET) >= 0;
    if (ok)
        fEOF = false;
    return ok;
#else
    return false;
#endif
}

bool plMP3Codec::Read(uint32_t numBytes, void* buffer)
{
#ifdef USE_MPG123
    if (!fValid) return false;
    if (fEOF) {
        memset(buffer, 0, numBytes);
        return false;
    }

    uint8_t* out = static_cast<uint8_t*>(buffer);
    size_t remaining = numBytes;
    size_t totalDone = 0;

    while (remaining > 0) {
        size_t done = 0;
        int ret = mpg123_read(fHandle, out, remaining, &done);
        if (done > 0) {
            out += done;
            remaining -= done;
            totalDone += done;
        }
        if (ret == MPG123_DONE) {
            fEOF = true;
            memset(out, 0, remaining);
            remaining = 0;
            break;
        }
        if (ret == MPG123_NEW_FORMAT) {
            continue;
        }
        if (ret != MPG123_OK) {
            plStatusLog::AddLineSF("audio.log", "plMP3Codec::Read error ret={}", ret);
            break;
        }
    }

    return remaining == 0;
#else
    return false;
#endif
}

uint32_t plMP3Codec::NumBytesLeft()
{
#ifdef USE_MPG123
    if (!fValid || fEOF)
        return 0;
    if (fTotalSamples > 0) {
        int64_t pos = mpg123_tell(fHandle);
        return (pos < 0 || pos >= fTotalSamples)
               ? 0
               : static_cast<uint32_t>(fTotalSamples - pos) * fHeader.fBlockAlign;
    }
    return UINT32_MAX;
#else
    return 0;
#endif
}
