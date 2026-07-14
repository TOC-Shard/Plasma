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

#include "plLayerWebM.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#ifdef USE_WEBM
#   include <mkvparser/mkvreader.h>
#   include <mkvparser/mkvparser.h>

#   define VPX_CODEC_DISABLE_COMPAT 1
#   include <vpx/vpx_decoder.h>
#   include <vpx/vp8dx.h>
#   define iface (vpx_codec_vp9_dx())
#   include <opus.h>

#   define WEBM_CODECID_VP9 "V_VP9"
#   define WEBM_CODECID_OPUS "A_OPUS"

#   include <AL/al.h>
#endif

#include "HeadSpin.h"
#include "hsResMgr.h"
#include "hsTimer.h"

#include "plAudio/plAudioSystem.h"
#include "plGImage/plMipmap.h"
#include "plStatusLog/plStatusLog.h"

#define SAFE_OP(x, err) \
{ \
    int64_t ret = 0; \
    ret = x; \
    if (ret < 0) { \
        hsAssert(false, "failed to " err); \
        return ISetFault(err); \
    } \
}

// =====================================================
// The following is duplicated (rather than shared) from pfMoviePlayer/plMoviePlayer.cpp
// and plPlanarImage.cpp -- see the plLayerWebM implementation plan for why this
// module intentionally doesn't take a new dependency on pfMoviePlayer.
// =====================================================

#ifdef USE_WEBM

namespace
{
    uint8_t Clip(int32_t val)
    {
        if (val < 0)
            return 0;
        if (val > 255)
            return 255;
        return static_cast<uint8_t>(val);
    }

    // Same I420->RGBA math as plPlanarImage::Yuv420ToRgba().
    void Yuv420ToRgba(uint32_t w, uint32_t h, const int32_t* stride, uint8_t** planes, uint8_t* const dest)
    {
        const uint8_t* y_src = planes[0];
        const uint8_t* u_src = planes[1];
        const uint8_t* v_src = planes[2];

        constexpr int32_t YG = 74;
        constexpr int32_t UB = 127, UG = -25, UR = 0;
        constexpr int32_t VB = 0, VG = -52, VR = 102;
        constexpr int32_t BB = UB * 128 + VB * 128;
        constexpr int32_t BG = UG * 128 + VG * 128;
        constexpr int32_t BR = UR * 128 + VR * 128;

        for (uint32_t i = 0; i < h; ++i)
        {
            for (uint32_t j = 0; j < w; ++j)
            {
                size_t y_idx = stride[0] * i + j;
                size_t u_idx = stride[1] * (i / 2) + (j / 2);
                size_t v_idx = stride[2] * (i / 2) + (j / 2);
                size_t dest_idx = w * i + j;

                int32_t y = static_cast<int32_t>(y_src[y_idx]);
                int32_t u = static_cast<int32_t>(u_src[u_idx]);
                int32_t v = static_cast<int32_t>(v_src[v_idx]);
                int32_t y1 = (y - 16) * YG;

                dest[dest_idx * 4 + 0] = Clip(((u * UB + v * VB) - (BB)+y1) >> 6);
                dest[dest_idx * 4 + 1] = Clip(((u * UG + v * VG) - (BG)+y1) >> 6);
                dest[dest_idx * 4 + 2] = Clip(((u * UR + v * VR) - (BR)+y1) >> 6);
                dest[dest_idx * 4 + 3] = 0xff;
            }
        }
    }

// Thin RAII wrapper matching plMoviePlayer's plVPXMovieFrame ownership semantics
// exactly (vpx_img_free() on the decoded frame), rather than assuming we know
// libvpx's internal buffer-ownership rules better than the already-proven code.
// (Kept in this same anonymous namespace, along with VPX/TrackMgr below, so a
// UNITY_BUILD of pfSurface can never collide with a same-named type elsewhere.)
struct VpxFrame
{
    vpx_image_t* fImage;
    VpxFrame(vpx_image_t* img) : fImage(img) { }
    ~VpxFrame() { if (fImage) vpx_img_free(fImage); }
};

class VPX
{
public:
    vpx_codec_ctx_t codec;

    VPX() : codec() { }

    ~VPX()
    {
        if (vpx_codec_destroy(&codec))
            hsAssert(false, vpx_codec_error_detail(&codec));
    }

    static VPX* Create()
    {
        VPX* instance = new VPX;
        if (vpx_codec_dec_init(&instance->codec, iface, nullptr, 0)) {
            hsAssert(false, vpx_codec_error_detail(&instance->codec));
            delete instance;
            return nullptr;
        }
        return instance;
    }

    std::unique_ptr<VpxFrame> Decode(uint8_t* buf, uint32_t size)
    {
        if (vpx_codec_decode(&codec, buf, size, nullptr, 0) != VPX_CODEC_OK) {
            const char* detail = vpx_codec_error_detail(&codec);
            hsAssert(false, detail ? detail : "unspecified decode error");
            return nullptr;
        }

        vpx_codec_iter_t iter = nullptr;
        // ASSUMPTION: only one image per frame, same as plMoviePlayer's VPX wrapper.
        vpx_image_t* img = vpx_codec_get_frame(&codec, &iter);
        if (!img)
            return nullptr;
        if (img->fmt != VPX_IMG_FMT_I420) {
            hsAssert(0, "VPX decoded a frame in an unexpected format");
            vpx_img_free(img);
            return nullptr;
        }
        return std::make_unique<VpxFrame>(img);
    }
};

class TrackMgr
{
protected:
    const mkvparser::Track* fTrack;
    const mkvparser::BlockEntry* fCurrentBlock;
    int32_t fStatus;

public:
    TrackMgr(const mkvparser::Track* track) : fTrack(track), fCurrentBlock(), fStatus() { }

    const mkvparser::Track* GetTrack() { return fTrack; }

    // Collects raw frame buffers for every block up to movieTimeNs, advancing our
    // position forward -- this can never rewind (see the class comment in plLayerWebM.h).
    bool GetFrames(mkvparser::MkvReader* reader, int64_t movieTimeNs, std::vector<std::pair<std::unique_ptr<uint8_t[]>, int32_t>>& frames)
    {
        if (!fCurrentBlock)
            fStatus = int32_t(fTrack->GetFirst(fCurrentBlock));

        while (fCurrentBlock && fStatus == 0) {
            const mkvparser::Block* block = fCurrentBlock->GetBlock();
            int64_t time = block->GetTime(fCurrentBlock->GetCluster()) - fTrack->GetCodecDelay();
            if (time <= movieTimeNs) {
                for (int32_t i = 0; i < block->GetFrameCount(); i++) {
                    const mkvparser::Block::Frame data = block->GetFrame(i);
                    auto buf = std::make_unique<uint8_t[]>(data.len);
                    data.Read(reader, buf.get());
                    frames.emplace_back(std::move(buf), static_cast<int32_t>(data.len));
                }
                fStatus = int32_t(fTrack->GetNext(fCurrentBlock, fCurrentBlock));
            } else {
                return true; // caught up to movieTimeNs, more may come later
            }
        }
        return false; // no more blocks -- track exhausted
    }
};

} // anonymous namespace

#endif // USE_WEBM

struct plWebMMovieInfo
{
#ifdef USE_WEBM
    mkvparser::MkvReader*               fReader = nullptr;
    std::unique_ptr<mkvparser::Segment> fSegment;
    std::unique_ptr<TrackMgr>           fVideoTrack, fAudioTrack;
    std::unique_ptr<VPX>                fVpx;
    // Direct OpenAL playback (matching plNetworkAudioStream) rather than plWin32VideoSound:
    // that class is hardcoded to a 2D "GUI sound" type that glues itself to the listener's
    // position and never actually builds a 3D-capable buffer (see
    // plWin32Sound::SetPosition()'s fType==kGUISound special case and plWin32VideoSound's
    // ctor always passing enable3D=false to plDSoundBuffer), so it can't be reused for a
    // properly positioned movie layer's audio.
    unsigned int                        fALSource = 0;
    unsigned int                        fALBuffer = 0;
    bool                                fALReady = false;
    int64_t                             fMovieTimeNs = 0;
#endif
};

// =====================================================

plLayerWebM::plLayerWebM()
    : fWebMInfo(new plWebMMovieInfo), fHasSoundPos(), fFalloffMin(1), fFalloffMax(1000000000)
{ }

plLayerWebM::~plLayerWebM()
{
    ICloseMovie();
    delete fWebMInfo;
}

int32_t plLayerWebM::ISecsToFrame(float secs)
{
    // Pure millisecond dirty-check tick -- see the class comment in plLayerWebM.h.
    // The real seconds-to-block mapping happens in IGetCurrentFrame().
    return int32_t(secs * 1000.0f + 0.5f);
}

bool plLayerWebM::IInit()
{
#ifdef USE_WEBM
    if (!plFileInfo(fMovieName).Exists())
        return ISetFault("Movie file does not exist");

    fWebMInfo->fReader = new mkvparser::MkvReader;
    SAFE_OP(fWebMInfo->fReader->Open(fMovieName.AsString().c_str()), "open movie");

    long long pos = 0;
    mkvparser::EBMLHeader ebmlHeader;
    SAFE_OP(ebmlHeader.Parse(fWebMInfo->fReader, pos), "read mkv header");
    mkvparser::Segment* seg;
    SAFE_OP(mkvparser::Segment::CreateInstance(fWebMInfo->fReader, pos, seg), "get segment info");
    SAFE_OP(seg->Load(), "load segment from webm");
    fWebMInfo->fSegment.reset(seg);

    const mkvparser::Tracks* tracks = fWebMInfo->fSegment->GetTracks();
    for (uint32_t i = 0; i < tracks->GetTracksCount(); ++i) {
        const mkvparser::Track* track = tracks->GetTrackByIndex(i);
        if (!track)
            continue;
        switch (track->GetType()) {
        case mkvparser::Track::kAudio:
            if (!fWebMInfo->fAudioTrack)
                fWebMInfo->fAudioTrack = std::make_unique<TrackMgr>(track);
            break;
        case mkvparser::Track::kVideo:
            if (!fWebMInfo->fVideoTrack)
                fWebMInfo->fVideoTrack = std::make_unique<TrackMgr>(track);
            break;
        }
    }

    if (!fWebMInfo->fVideoTrack)
        return ISetFault("No video track in movie");

    const mkvparser::VideoTrack* video = static_cast<const mkvparser::VideoTrack*>(fWebMInfo->fVideoTrack->GetTrack());
    if (strncmp(video->GetCodecId(), WEBM_CODECID_VP9, std::size(WEBM_CODECID_VP9)) != 0)
        return ISetFault("Not a VP9 video track");

    VPX* vpx = VPX::Create();
    if (!vpx)
        return ISetFault("Failed to create VPX decoder");
    fWebMInfo->fVpx.reset(vpx);

    // Decode the entire audio track up front and hand it to a static OpenAL buffer --
    // same decode approach as plMoviePlayer::ILoadAudio() (Opus only, see plan: no
    // MP3/Vorbis for v1), but played directly through OpenAL like plNetworkAudioStream
    // rather than through plWin32VideoSound (see the fALSource/fALBuffer comment above).
    // Guarded on plgAudioSys::Active(): this IInit() also runs once during 3ds Max
    // export (via the Eval(0,0,0) priming call in hsMaterialConverter.cpp), where the
    // audio system was never initialized -- calling into OpenAL there crashed the
    // exporter. At actual runtime in the client, Active() is true and this proceeds
    // normally.
    if (fWebMInfo->fAudioTrack && plgAudioSys::Active()) {
        const mkvparser::AudioTrack* audio = static_cast<const mkvparser::AudioTrack*>(fWebMInfo->fAudioTrack->GetTrack());
        if (strncmp(audio->GetCodecId(), WEBM_CODECID_OPUS, std::size(WEBM_CODECID_OPUS)) == 0) {
            int numChannels = (int)audio->GetChannels();

            int error;
            OpusDecoder* opus = opus_decoder_create(48000, numChannels, &error);
            if (error != OPUS_OK) {
                hsAssert(false, "Error occurred initializing opus");
            } else {
                std::vector<std::pair<std::unique_ptr<uint8_t[]>, int32_t>> frames;
                fWebMInfo->fAudioTrack->GetFrames(fWebMInfo->fReader, fWebMInfo->fSegment->GetDuration(), frames);

                constexpr int kMaxFrameSize = 5760; // max packet duration at 48kHz
                std::vector<int16_t> decoded;
                decoded.reserve(frames.size() * (size_t)numChannels * kMaxFrameSize);
                auto frameData = std::make_unique<int16_t[]>((size_t)kMaxFrameSize * numChannels);

                for (const auto& frame : frames) {
                    int samples = opus_decode(opus, frame.first.get(), frame.second, frameData.get(), kMaxFrameSize, 0);
                    if (samples < 0) {
                        hsAssert(false, "opus error");
                        continue;
                    }
                    for (int32_t i = 0; i < samples * numChannels; i++)
                        decoded.emplace_back(frameData[i]);
                }
                opus_decoder_destroy(opus);

                // OpenAL only spatializes mono sources -- a stereo buffer plays back
                // without any directional panning at all (just volume falloff), which
                // is exactly the "audible but not coming from the object" symptom this
                // fixes. Downmix to mono for positioning, same as plNetworkAudioStream
                // does for positional streams.
                if (numChannels == 2) {
                    for (size_t i = 0; i < decoded.size() / 2; i++)
                        decoded[i] = (int16_t)(((int32_t)decoded[i * 2] + decoded[i * 2 + 1]) / 2);
                    decoded.resize(decoded.size() / 2);
                }

                alGenSources(1, &fWebMInfo->fALSource);
                alGenBuffers(1, &fWebMInfo->fALBuffer);

                alBufferData(fWebMInfo->fALBuffer, AL_FORMAT_MONO16, decoded.data(),
                             (ALsizei)(decoded.size() * sizeof(int16_t)), 48000);
                alSourcei(fWebMInfo->fALSource, AL_BUFFER, (ALint)fWebMInfo->fALBuffer);
                alSourcef(fWebMInfo->fALSource, AL_ROLLOFF_FACTOR, 0.3048f); // same as all other Plasma sounds

                fWebMInfo->fALReady = true;
                IApplyAudioSettings();
                alSourcePlay(fWebMInfo->fALSource);
            }
        } else {
            plStatusLog::AddLineSF("movie.log", "{}: Audio track is not Opus, skipping audio", fMovieName);
        }
    }

    ISetSize((int)video->GetWidth(), (int)video->GetHeight());
    ISetLength(float(fWebMInfo->fSegment->GetDuration() / 1'000'000'000.0));
    fWebMInfo->fMovieTimeNs = 0;

    return false; // success
#else
    return ISetFault("WebM support not built");
#endif
}

bool plLayerWebM::IGetCurrentFrame()
{
#ifdef USE_WEBM
    if (!fWebMInfo->fReader)
    {
        if (IInit())
        {
            plStatusLog::AddLineSF("movie.log", "{}: IInit() failed on initial open", fMovieName);
            return true;
        }
    }

    ICheckBitmap();

    // fCurrentFrame is our millisecond dirty-check tick (see ISecsToFrame()); convert
    // back to nanoseconds to know how far to decode forward.
    int64_t targetNs = int64_t(fCurrentFrame) * 1'000'000;

    if (targetNs < fWebMInfo->fMovieTimeNs)
    {
        // Rewind (e.g. a loop restarting) -- VP9 can't seek backward, so start over
        // and play forward again from zero up to the requested time.
        plStatusLog::AddLineSF("movie.log", "{}: rewind detected (targetNs={} < movieTimeNs={}), reinitializing",
                                fMovieName, targetNs, fWebMInfo->fMovieTimeNs);
        IRelease();
        if (IInit())
        {
            plStatusLog::AddLineSF("movie.log", "{}: IInit() failed on loop-restart reinit", fMovieName);
            return true;
        }
    }
    fWebMInfo->fMovieTimeNs = targetNs;

    std::vector<std::pair<std::unique_ptr<uint8_t[]>, int32_t>> frames;
    fWebMInfo->fVideoTrack->GetFrames(fWebMInfo->fReader, targetNs, frames);

    std::unique_ptr<VpxFrame> lastFrame;
    for (const auto& frame : frames)
    {
        // Keep only the most recent decoded frame -- matches plMoviePlayer::IProcessVideoFrame,
        // which only cares about displaying the latest image, not every intermediate one.
        if (auto decoded = fWebMInfo->fVpx->Decode(frame.first.get(), (uint32_t)frame.second))
            lastFrame = std::move(decoded);
    }

    if (lastFrame)
    {
        plMipmap* mip = plMipmap::ConvertNoRef(GetTexture());
        if (mip)
        {
            vpx_image_t* img = lastFrame->fImage;
            Yuv420ToRgba(img->d_w, img->d_h, img->stride, img->planes, reinterpret_cast<uint8_t*>(mip->GetImage()));
        }
    }

    return false;
#else
    return true;
#endif
}

bool plLayerWebM::ICloseMovie()
{
#ifdef USE_WEBM
    if (fWebMInfo->fALReady)
    {
        alSourceStop(fWebMInfo->fALSource);
        alDeleteSources(1, &fWebMInfo->fALSource);
        alDeleteBuffers(1, &fWebMInfo->fALBuffer);
        fWebMInfo->fALSource = 0;
        fWebMInfo->fALBuffer = 0;
        fWebMInfo->fALReady = false;
    }
    fWebMInfo->fVpx.reset();
    fWebMInfo->fVideoTrack.reset();
    fWebMInfo->fAudioTrack.reset();
    fWebMInfo->fSegment.reset();
    if (fWebMInfo->fReader)
    {
        fWebMInfo->fReader->Close();
        delete fWebMInfo->fReader;
        fWebMInfo->fReader = nullptr;
    }
    fWebMInfo->fMovieTimeNs = 0;
#endif
    return false;
}

bool plLayerWebM::IRelease()
{
    return ICloseMovie();
}

void plLayerWebM::IApplyAudioSettings()
{
#ifdef USE_WEBM
    if (!fWebMInfo->fALReady)
        return;

    if (fHasSoundPos) {
        alSourcei(fWebMInfo->fALSource, AL_SOURCE_RELATIVE, AL_FALSE);
        // Plasma(X,Y,Z) -> OpenAL(X,Z,-Y), same convention as plNetworkAudioStream.
        alSource3f(fWebMInfo->fALSource, AL_POSITION, fSoundPos.fX, fSoundPos.fZ, -fSoundPos.fY);
    } else {
        alSourcei(fWebMInfo->fALSource, AL_SOURCE_RELATIVE, AL_TRUE);
        alSource3f(fWebMInfo->fALSource, AL_POSITION, 0.f, 0.f, 0.f);
    }
    alSourcef(fWebMInfo->fALSource, AL_REFERENCE_DISTANCE, (float)fFalloffMin);
    alSourcef(fWebMInfo->fALSource, AL_MAX_DISTANCE, (float)fFalloffMax);
#endif
}

void plLayerWebM::ISetAudioFalloff(int minDist, int maxDist)
{
    fFalloffMin = minDist;
    fFalloffMax = maxDist;
    IApplyAudioSettings(); // no-op if the sound hasn't been created yet; picked up by IInit() later either way
}

void plLayerWebM::Read(hsStream* s, hsResMgr* mgr)
{
    plLayerMovie::Read(s, mgr);

    fHasSoundPos = s->ReadBool();
    if (fHasSoundPos)
        fSoundPos.Read(s);
    fFalloffMin = s->ReadLE32();
    fFalloffMax = s->ReadLE32();
}

void plLayerWebM::Write(hsStream* s, hsResMgr* mgr)
{
    plLayerMovie::Write(s, mgr);

    s->WriteBool(fHasSoundPos);
    if (fHasSoundPos)
        fSoundPos.Write(s);
    s->WriteLE32((uint32_t)fFalloffMin);
    s->WriteLE32((uint32_t)fFalloffMax);
}
