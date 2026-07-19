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

#ifndef plLayerMovieMsg_inc
#define plLayerMovieMsg_inc

#include "pnMessage/plMessage.h"
#include "plFileSystem.h"

// Directed (not broadcast) message controlling a single plLayerMovie/plLayerWebM
// instance already living in the scene, identified by sending straight to its plKey.
// Unlike plMovieMsg (which is for the fullscreen ptMoviePlayer overlay and is handled
// centrally by plClient), this message needs no client-side registry: the movie layer
// object already persists as part of the loaded material.
class plLayerMovieMsg : public plMessage
{
public:
    enum
    {
        kSetMovieName   = 0x1, // Call SetFileName() first
        kPlay           = 0x2,
        kPause          = 0x4,
        kResume         = 0x8,
        kStop           = 0x10,
        kAddCallback    = 0x20, // Call SetCallback() first
        kSetFalloff     = 0x40, // Call SetFalloff() first
        kSeek           = 0x80, // Call SetSeekTime() first
    };

protected:
    plFileName  fMovieName;
    plMessage*  fCallback;
    int         fFalloffMin;
    int         fFalloffMax;
    float       fSeekTime;
    uint16_t    fCmd;

public:
    plLayerMovieMsg(const plKey& receiver, uint16_t cmd)
        : plMessage(nullptr, receiver, nullptr), fCallback(), fFalloffMin(), fFalloffMax(), fSeekTime(), fCmd(cmd)
    { }

    plLayerMovieMsg() : fCallback(), fFalloffMin(), fFalloffMax(), fSeekTime(), fCmd()
    { }

    ~plLayerMovieMsg()
    {
        hsRefCnt_SafeUnRef(fCallback);
    }

    CLASSNAME_REGISTER(plLayerMovieMsg);
    GETINTERFACE_ANY(plLayerMovieMsg, plMessage);

    uint16_t GetCmd() const { return fCmd; }
    plLayerMovieMsg& SetCmd(uint16_t c) { fCmd = c; return *this; }

    const plFileName& GetFileName() const { return fMovieName; }
    plLayerMovieMsg& SetFileName(const plFileName& name) { fMovieName = name; return *this; }

    // Only meaningful with kAddCallback. Message is ref'd/kept and Send() when the
    // movie's plAnimTimeConvert reaches its kStop callback point.
    plMessage* GetCallback() const { return fCallback; }
    plLayerMovieMsg& SetCallback(plMessage* msg)
    {
        hsRefCnt_SafeUnRef(fCallback);
        fCallback = msg;
        hsRefCnt_SafeRef(fCallback);
        return *this;
    }

    // Only meaningful with kSetFalloff. Distances, same units as plSound::SetMin()/SetMax().
    int GetFalloffMin() const { return fFalloffMin; }
    int GetFalloffMax() const { return fFalloffMax; }
    plLayerMovieMsg& SetFalloff(int minDist, int maxDist) { fFalloffMin = minDist; fFalloffMax = maxDist; return *this; }

    // With kSeek: seconds from the start of the movie to jump to, set before Send().
    float GetSeekTime() const { return fSeekTime; }
    plLayerMovieMsg& SetSeekTime(float t) { fSeekTime = t; return *this; }

    void Read(hsStream* s, hsResMgr* mgr) override { hsAssert(false, "Not for I/O"); plMessage::IMsgRead(s, mgr); }
    void Write(hsStream* s, hsResMgr* mgr) override { hsAssert(false, "Not for I/O"); plMessage::IMsgWrite(s, mgr); }
};

#endif // plLayerMovieMsg_inc
