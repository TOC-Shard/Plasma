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

#ifndef plLayerWebM_inc
#define plLayerWebM_inc

#include "hsGeometry3.h"

#include "plLayerMovie.h"

struct plWebMMovieInfo;
class plSoftVolume;
class plMessage;

// Plays a WebM (VP9 video + Opus audio) file as a material layer's texture on a
// 3D object, instead of full-screen (see pfMoviePlayer/plMoviePlayer for that).
// VP9 decoding via libvpx is an inherently sequential/forward-only bitstream
// decode -- unlike plLayerAVI, which can randomly seek to any frame via the
// legacy VFW API, we can only ever decode forward from wherever we left off.
// So fCurrentFrame (inherited from plLayerMovie) is repurposed here as a pure
// millisecond dirty-check counter for plLayerMovie::ICurrentFrameDirty() --
// the real seconds-to-block mapping happens only inside IGetCurrentFrame().
class plLayerWebM : public plLayerMovie
{
protected:
    plWebMMovieInfo*    fWebMInfo;

    hsPoint3            fSoundPos;
    bool                fHasSoundPos;
    int                 fFalloffMin;
    int                 fFalloffMax;
    float               fVolume;
    plSoftVolume*       fSoftRegion; // non-owning; ref tracked via the resmgr like plSound::fSoftRegion
    float               fLastLoggedStrength; // debug only, not serialized -- see IApplyAudioSettings()
    bool                fRegisteredForTime; // see IInit()/ICloseMovie() -- keeps gain (mute/volume/soft region)
                                             // updating every frame regardless of whether this layer's material
                                             // is actually being Eval()'d (i.e. even while the object is offscreen)

    bool                ICloseMovie();

    int32_t             ISecsToFrame(float secs) override;
    bool                IInit() override;
    bool                IGetCurrentFrame() override;
    bool                IRelease() override;
    void                ISetAudioFalloff(int minDist, int maxDist) override;

    void                IApplyAudioSettings(); // pushes fSoundPos/fFalloffMin/fFalloffMax onto a live sound, if any

public:
    // Matches plSound::Refs' kRefSoftVolume/kSoftRegion (same numeric value, 0) --
    // kept as its own constant since plLayerWebM isn't a plSound and has no
    // dependency on that class' Refs enum.
    enum { kRefSoftRegion = 0 };

    plLayerWebM();
    virtual ~plLayerWebM();

    CLASSNAME_REGISTER(plLayerWebM);
    GETINTERFACE_ANY(plLayerWebM, plLayerMovie);

    void Read(hsStream* s, hsResMgr* mgr) override;
    void Write(hsStream* s, hsResMgr* mgr) override;
    bool MsgReceive(plMessage* msg) override;

    // Called at export time (see hsMaterialConverter::IProcessLayerMovie) so the
    // audio -- created lazily inside IInit() once playback actually starts -- comes
    // up already positioned/attenuated correctly.
    void SetSoundPosition(const hsPoint3& pos) { fSoundPos = pos; fHasSoundPos = true; }
    void SetSoundFalloff(int minDist, int maxDist) { fFalloffMin = minDist; fFalloffMax = maxDist; }
    void SetVolume(float volume) { fVolume = volume; }
};

#endif // plLayerWebM_inc
