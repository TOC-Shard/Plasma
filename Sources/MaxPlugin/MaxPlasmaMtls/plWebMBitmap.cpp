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

#include "HeadSpin.h"

#include "MaxMain/MaxAPI.h"

// Registers the ".webm" extension with 3ds Max's Bitmap Manager purely so it shows
// up in the standard "Select Bitmap Image File" filter dropdown (used by the
// material editor's Bitmap picker) instead of requiring "All Files". The actual
// decoding is done at runtime by plLayerWebM, not by this plugin -- Max only needs
// the filename, which it records regardless of whether GetImageInfo()/Load() (used
// for in-Max thumbnail previews) succeed. Modeled on plBinkBitmap.cpp.
class plWebMBitmapIO : public BitmapIO
{
public:
    int ExtCount() override           { return 1; }
    const MCHAR* Ext(int n) override  { return _M("webm"); }

    const MCHAR* LongDesc() override  { return _M("WebM Video File"); }
    const MCHAR* ShortDesc() override { return _M("WebM"); }

    const MCHAR* AuthorName()       { return _M("TOC-Shard"); }
    const MCHAR* CopyrightMessage() { return _M(""); }
    unsigned int Version()          { return 100; }

    // BMMIO_READER + BMMIO_EXTENSION are what actually make 3ds Max list ".webm" in
    // the "Select Bitmap Image File" format dropdown and accept it as a valid
    // extension at all -- a capability of 0 (as the disabled Bink stub uses on
    // purpose) makes Max silently refuse the extension entirely, which is not what
    // we want here.
    int Capability()                { return BMMIO_READER | BMMIO_EXTENSION; }
    DWORD EvaluateConfigure()       { return 0; }
    BOOL LoadConfigure(void* ptr BITMAP_LOAD_CONFIGURE_DATASIZE) { return FALSE; }
    BOOL SaveConfigure(void* ptr)   { return FALSE; }

    // No real in-Max preview -- Plasma decodes the movie itself at runtime (see
    // plLayerWebM), so this just reports a placeholder size/format so Max accepts
    // the file without erroring; the real dimensions come from the actual export.
    BMMRES GetImageInfo(BitmapInfo* fbi) override
    {
        fbi->SetWidth(320);
        fbi->SetHeight(240);
        fbi->SetType(BMM_TRUE_24);
        fbi->SetAspect(1.0f);
        fbi->SetGamma(1.0f);
        fbi->SetFirstFrame(0);
        fbi->SetLastFrame(0);
        return BMMRES_SUCCESS;
    }

    // Returns a flat placeholder image (not the real movie frame -- that only ever
    // exists at runtime, decoded by plLayerWebM) so 3ds Max's "Open" action actually
    // completes instead of erroring out. Modeled on the 24-bit path of
    // maxsdk/samples/images/bmp/bmp.cpp's BitmapIO_BMP::Load().
    BitmapStorage* Load(BitmapInfo* fbi, Bitmap* map, BMMRES* status) override
    {
        constexpr int w = 320, h = 240;

        BitmapStorage* s = BMMCreateStorage(map->Manager(), BMM_TRUE_32);
        if (!s)
        {
            if (status)
                *status = BMMRES_CANTSTORAGE;
            return nullptr;
        }

        if (!s->Allocate(fbi, map->Manager(), BMM_OPEN_R))
        {
            delete s;
            if (status)
                *status = BMMRES_MEMORYERROR;
            return nullptr;
        }

        BMM_Color_64 row[w];
        for (auto& px : row)
        {
            px.r = px.g = px.b = 0x4000;
            px.a = 0xffff;
        }
        for (int y = 0; y < h; y++)
            s->PutPixels(0, y, w, row);

        s->bi.CopyImageInfo(fbi);

        if (status)
            *status = BMMRES_SUCCESS;
        return s;
    }

    void ShowAbout(HWND hWnd)
    {
        plMaxMessageBox(hWnd, _T("WebM movies are decoded at runtime by Plasma, not previewed in 3ds Max."), _T("WebM"), MB_ICONINFORMATION);
    }
};

class WebMClassDesc : public plMaxClassDesc<ClassDesc2>
{
public:
    int IsPublic() override { return 1; }
    void* Create(BOOL loading=FALSE) override { return static_cast<void*>(new plWebMBitmapIO); }

    const MCHAR* ClassName() override { return _M("WebM"); }
    SClass_ID SuperClassID() override { return BMM_IO_CLASS_ID; }
    Class_ID ClassID() override { return Class_ID(0x3a7e6b52, 0x591c4d18); }
    const MCHAR* Category() override { return _M("Bitmap I/O"); }
};

static WebMClassDesc WebMDesc;
ClassDesc2* GetWebMClassDesc()
{
    return &WebMDesc;
}
