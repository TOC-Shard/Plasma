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

#include "plLayerMovie.h"

#include "HeadSpin.h"
#include "hsGDeviceRef.h"
#include "hsResMgr.h"
#include "hsStream.h"
#include "hsTimer.h"

#include <string_theory/format>

#include "plMessage/plAnimCmdMsg.h"
#include "plMessage/plLayerMovieMsg.h"
#include "pnMessage/plEventCallbackMsg.h"
#include "plGImage/plMipmap.h"
#include "plStatusLog/plStatusLog.h"

plLayerMovie::plLayerMovie()
:   fCurrentFrame(-1),
    fLength(0),
    fWidth(32),
    fHeight(32),
    fLoggedIdle(false)
{
    fOwnedChannels |= kTexture;
    fTexture = new plBitmap*;
    *fTexture = nullptr;
}

plLayerMovie::~plLayerMovie()
{
    delete *fTexture;
}

bool plLayerMovie::ISetFault(const char* errStr)
{
#ifdef HS_DEBUGGING
    hsStatusMessageF("ERROR {}: {}", fMovieName, errStr);
#endif // HS_DEBUGGING
    fMovieName = "";
    return true;
}

bool plLayerMovie::ISetLength(float secs)
{
    fLength = secs;

    // Mirrors the one-time setup in hsMaterialConverter.cpp's IProcessLayerMovie().
    // That only runs at export time, when a file was actually picked -- for a movie
    // set later at runtime (e.g. via setFilename() with no file picked in Max), the
    // exported fTimeConvert is otherwise stuck with Begin==End==0 forever, so Start()
    // never actually advances anything. Redoing it here whenever the real length
    // becomes known (export or runtime) keeps both paths correct.
    fTimeConvert.SetBegin(0);
    fTimeConvert.SetEnd(secs);
    fTimeConvert.SetLoopPoints(0, secs);

    return false;
}

int plLayerMovie::GetWidth() const
{
    plMipmap    *mip = plMipmap::ConvertNoRef( GetTexture() );
    return mip ? mip->GetWidth() : 0;
}

int plLayerMovie::GetHeight() const
{
    plMipmap    *mip = plMipmap::ConvertNoRef( GetTexture() );
    return mip ? mip->GetHeight() : 0;
}

bool plLayerMovie::ISetSize(int width, int height)
{
    fWidth = width;
    fHeight = height;
    return false;
}

bool plLayerMovie::ISetupBitmap()
{
    // Switching to a different movie (setFilename() mid-session) can mean a different
    // resolution -- ISetSize() already updated fWidth/fHeight by this point (called
    // from IInit(), which always runs before ICheckBitmap()/ISetupBitmap() each tick),
    // but the old bitmap itself is still whatever size the *previous* movie needed.
    // Without this, the decoder writes the new (possibly larger) frame into a buffer
    // sized for the old one -- a heap buffer overflow.
    if( plMipmap* existing = plMipmap::ConvertNoRef(GetTexture()) )
    {
        if( (uint32_t)existing->GetWidth() != fWidth || (uint32_t)existing->GetHeight() != fHeight )
        {
            delete existing;
            *fTexture = nullptr;
        }
    }

    if( !GetTexture() )
    {
        plMipmap* b = new plMipmap( fWidth, fHeight, plMipmap::kARGB32Config, 1 );
        memset(b->GetImage(), 0x10, b->GetHeight() * b->GetRowBytes() );
        b->SetFlags( b->GetFlags() | plMipmap::kDontThrowAwayImage );

        ST::string name = ST::format("{}_BMap", fMovieName);
        hsgResMgr::ResMgr()->NewKey( name, b, plLocation::kGlobalFixedLoc );

        *fTexture = (plBitmap *)b;
    }

    return false;
}

bool plLayerMovie::ICheckBitmap()
{
    // Always call through, not just when there's no texture yet -- ISetupBitmap()
    // itself now also handles the "existing texture is the wrong size" case (e.g.
    // switching to a different movie mid-session via setFilename()), which would
    // never get a chance to run if gated behind !GetTexture() here.
    ISetupBitmap();

    return false;
}

bool plLayerMovie::IMovieIsIdle()
{
    IRelease();

    return false;
}

bool plLayerMovie::ICurrentFrameDirty(double wSecs)
{
    float secs = fTimeConvert.WorldToAnimTime(wSecs);
    uint32_t frame = ISecsToFrame(secs);
    if( frame == fCurrentFrame )
        return false;
    fCurrentFrame = frame;

    return true;
}

uint32_t plLayerMovie::Eval(double wSecs, uint32_t frame, uint32_t ignore)
{
    uint32_t dirty = plLayerAnimation::Eval(wSecs, frame, ignore);

    if( !IGetFault() && !(ignore & kTexture) )
    {
        if( ICurrentFrameDirty(wSecs) )
        {
            fLoggedIdle = false;

            if( IGetCurrentFrame() )
                ISetFault("Getting current frame");

            if( GetTexture() )
            {
                hsGDeviceRef* ref = GetTexture()->GetDeviceRef();
                if( ref )
                    ref->SetDirty(true);
            }
        }
        else
        if( IsStopped() && !fLoggedIdle )
        {
            plStatusLog::AddLineSF("movie.log", "{}: idling (IsStopped()==true) at frame {}", fMovieName, fCurrentFrame);
            IMovieIsIdle();
            fLoggedIdle = true;
        }
        
        dirty |= kTexture;
    }
    return dirty;
}

void plLayerMovie::Read(hsStream* s, hsResMgr* mgr)
{
    plLayerAnimation::Read(s, mgr);

    int len = s->ReadLE32();
    if( len )
    {
        ST::char_buffer movieName;
        movieName.allocate(len);
        s->Read(len, movieName.data());
        fMovieName = ST::string(movieName);
    }
    else
    {
        // Valid and expected for a WebM layer exported with no file picked yet (see
        // hsMaterialConverter.cpp's IProcessLayerMovie) -- the movie gets set later
        // at runtime via setFilename().
        fMovieName = "";
    }
}

void plLayerMovie::Write(hsStream* s, hsResMgr* mgr)
{
    plLayerAnimation::Write(s, mgr);

    s->WriteLE32((uint32_t)fMovieName.GetSize());
    s->Write(fMovieName.GetSize(), fMovieName.AsString().c_str());
}

void plLayerMovie::SetMovieName(const plFileName& n)
{
    if (n == fMovieName)
        return;

    // Release whatever was previously open (no-op if nothing was ever opened) and
    // reset our dirty-check state so the next Eval() reinitializes from the new file.
    IRelease();
    fMovieName = n;
    fCurrentFrame = -1;
    fLength = 0;
}

void plLayerMovie::ISetAudioFalloff(int minDist, int maxDist)
{
    // No audio in the base class (e.g. plLayerAVI). Subclasses with an audio
    // track (plLayerWebM) override this.
}

bool plLayerMovie::MsgReceive(plMessage* msg)
{
    if (plLayerMovieMsg* movieMsg = plLayerMovieMsg::ConvertNoRef(msg))
    {
        uint16_t cmd = movieMsg->GetCmd();

        if (cmd & plLayerMovieMsg::kSetMovieName)
            SetMovieName(movieMsg->GetFileName());

        if (cmd & plLayerMovieMsg::kPlay)
        {
            fTimeConvert.SetCurrentAnimTime(fTimeConvert.GetBegin(), true);
            fTimeConvert.Start();
        }

        if (cmd & plLayerMovieMsg::kPause)
            fTimeConvert.Stop(true);

        if (cmd & plLayerMovieMsg::kResume)
            fTimeConvert.Start();

        if (cmd & plLayerMovieMsg::kStop)
        {
            fTimeConvert.Stop(true);
            fTimeConvert.SetCurrentAnimTime(fTimeConvert.GetBegin(), true);
        }

        if (cmd & plLayerMovieMsg::kAddCallback)
        {
            // AddCallback() refs the message itself, so no extra ref needed here.
            if (plEventCallbackMsg* cb = plEventCallbackMsg::ConvertNoRef(movieMsg->GetCallback()))
                fTimeConvert.AddCallback(cb);
        }

        if (cmd & plLayerMovieMsg::kSetFalloff)
            ISetAudioFalloff(movieMsg->GetFalloffMin(), movieMsg->GetFalloffMax());

        if (cmd & plLayerMovieMsg::kSeek)
        {
            // Doesn't touch play/pause state -- jumps the clock only. Whatever
            // catch-up/rewind machinery IGetCurrentFrame() already has (see
            // plLayerWebM) picks this up naturally on the next tick, forward or
            // backward.
            fTimeConvert.SetCurrentAnimTime(movieMsg->GetSeekTime(), true);
        }

        if (cmd & plLayerMovieMsg::kGetCurrentTime)
        {
            // WorldToAnimTimeNoUpdate() computes purely from the recorded Start()/
            // Stop() state history (fStartWorldTime/fStartAnimTime) and the wall
            // time given here -- unlike CurrentAnimTime(), it's NOT a stale cached
            // field, so this is correct even if this layer hasn't been Eval()'d
            // (e.g. offscreen) recently. Since this message is always sent+received
            // synchronously within a single client (never over the network), the
            // sender can read GetSeekTime() back immediately after Send() returns.
            movieMsg->SetSeekTime(fTimeConvert.WorldToAnimTimeNoUpdate(hsTimer::GetSysSeconds()));
        }

        return true;
    }

    return plLayerAnimation::MsgReceive(msg);
}

void plLayerMovie::DefaultMovie()
{
}
