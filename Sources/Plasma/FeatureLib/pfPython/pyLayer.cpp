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

#include <Python.h>
#include "pyKey.h"
#include "hsResMgr.h"

#include "pyLayer.h"
#include "pyImage.h"

#include "plMessage/plLayRefMsg.h"
#include "plResMgr/plKeyFinder.h"

void pyLayer::setKey(pyKey& layerKey) // only for python glue, do NOT call
{
    if (fLayer && fLayerKey)
        fLayerKey->UnRefObject();

    fLayer = nullptr;
    fLayerKey = layerKey.getKey();
}

plLayer* pyLayer::GetLayer() const
{
    if (fLayer)
        return fLayer;
    return plLayer::ConvertNoRef(fLayerKey->ObjectIsLoaded());
}

void pyLayer::SetTexture(const plKey& image)
{
    plLayer* layer = GetLayer();

    if (image) {
        plLayRefMsg* refMsg = new plLayRefMsg(fLayerKey, plRefMsg::kOnReplace, 0, plLayRefMsg::kTexture);
        hsgResMgr::ResMgr()->AddViaNotify(image, refMsg, plRefFlags::kActiveRef);
    }
}

PyObject* pyLayer::GetTexture() const
{
    plLayer* layer = GetLayer();

    plMipmap* mm = plMipmap::ConvertNoRef(layer->GetTexture());
    if (mm)
        return pyImage::New(mm);

    PYTHON_RETURN_NONE;
}

PyObject* pyLayer::Find(const ST::string& name, const ST::string& age, const ST::string& page)
{
    plKey foundKey = plKeyFinder::Instance().StupidSearch(age, page, plLayer::Index(), name);
    if (foundKey)
        return pyLayer::New(std::move(foundKey));
    PYTHON_RETURN_NONE;
}
