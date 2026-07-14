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

#ifndef _plWebMComponent_h_inc_
#define _plWebMComponent_h_inc_

#define WEBM_COMPONENT_ID   Class_ID(0x3a6e15d2, 0x5c1a7f4e)

#include "plComponent.h"
#include "plFileSystem.h"

// Applies a WebM (VP9+Opus) video to the object's existing material as a texture
// layer (see hsMaterialConverter::IProcessLayerMovie, which checks for this
// component instead of requiring a real file in the material's own Bitmap slot --
// the material still needs a Plasma bitmap-texture layer to attach onto, but that
// layer's own file can stay unset). No file assigned = no-op, so this can safely
// sit on any object "just in case".
class plWebMComponent : public plComponent
{
public:
    enum
    {
        kFileName,
        kVolume,
        kMinFalloff,
        kMaxFalloff,
        kAutoStart,
        kLoop,
        kSoftRegionEnable,
        kSoftRegion
    };

public:
    plWebMComponent();

    bool Convert(plMaxNode *node, plErrorMsg *pErrMsg) override { return true; } // all the work happens in IProcessLayerMovie

    plFileName GetFileName() const;
    void       SetFileName(const TCHAR* fn) { fCompPB->SetValue((ParamID)kFileName, 0, const_cast<TCHAR*>(fn)); }
    float      GetVolume() const   { return fCompPB->GetFloat((ParamID)kVolume); }
    int        GetMinFalloff() const { return fCompPB->GetInt((ParamID)kMinFalloff); }
    int        GetMaxFalloff() const { return fCompPB->GetInt((ParamID)kMaxFalloff); }
    bool       GetAutoStart() const { return fCompPB->GetInt((ParamID)kAutoStart) != 0; }
    bool       GetLoop() const     { return fCompPB->GetInt((ParamID)kLoop) != 0; }
    bool       GetSoftRegionEnable() const { return fCompPB->GetInt((ParamID)kSoftRegionEnable) != 0; }
    INode*     GetSoftRegionNode() const   { return fCompPB->GetINode((ParamID)kSoftRegion); }
};

#endif // _plWebMComponent_h_inc_
