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
//////////////////////////////////////////////////////////////////////
//
// pyLayerMovie   - a wrapper class for the plLayerMovie/plLayerWebM functions
//
//////////////////////////////////////////////////////////////////////

#include "pyLayerMovie.h"

#include "plMessage/plLayerMovieMsg.h"

#include "pyKey.h"

pyLayerMovie::pyLayerMovie(pyKey& layerKey)
    : fLayerKey(layerKey.getKey())
{
}

void pyLayerMovie::SetLayerKey(pyKey& layerKey)
{
    fLayerKey = layerKey.getKey();
}

void pyLayerMovie::SetFilename(const ST::string& filename)
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kSetMovieName);
    mov->SetFileName(filename);
    mov->Send();
}

void pyLayerMovie::SetFalloff(int minDist, int maxDist)
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kSetFalloff);
    mov->SetFalloff(minDist, maxDist);
    mov->Send();
}

void pyLayerMovie::SeekTo(float seconds)
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kSeek);
    mov->SetSeekTime(seconds);
    mov->Send();
}

float pyLayerMovie::GetPlaybackTime()
{
    if (!fLayerKey)
        return 0.f;

    // plMessage::Send() dispatches synchronously to a local (non-networked)
    // receiver, so GetSeekTime() already holds the answer once Send() returns --
    // SendAndKeep() (instead of plain Send()) keeps our own ref alive so we can
    // still read it back afterward.
    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kGetCurrentTime);
    mov->SendAndKeep();
    float result = mov->GetSeekTime();
    hsRefCnt_SafeUnRef(mov);
    return result;
}

void pyLayerMovie::Play()
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kPlay);
    mov->Send();
}

void pyLayerMovie::Pause()
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kPause);
    mov->Send();
}

void pyLayerMovie::Resume()
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kResume);
    mov->Send();
}

void pyLayerMovie::Stop()
{
    if (!fLayerKey)
        return;

    plLayerMovieMsg* mov = new plLayerMovieMsg(fLayerKey, plLayerMovieMsg::kStop);
    mov->Send();
}
