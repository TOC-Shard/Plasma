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

#include "plWebMComponent.h"
#include "plComponentReg.h"
#include "plSoftVolumeComponent.h"
#include "MaxMain/MaxAPI.h"
#include "MaxMain/MaxCompat.h"
#include "MaxMain/plMaxNode.h"

#include "resource.h"

#include <shlwapi.h>

void DummyCodeIncludeFuncWebM() {}

plFileName plWebMComponent::GetFileName() const
{
    const TCHAR* fn = fCompPB->GetStr((ParamID)kFileName);
    return (fn && *fn) ? plFileName(M2ST(fn)) : plFileName();
}

namespace
{
    void IUpdateWebMButton(plWebMComponent* comp, HWND hDlg, int dlgBtnItemToSet)
    {
        ICustButton* custButton = GetICustButton(GetDlgItem(hDlg, dlgBtnItemToSet));
        if (custButton != nullptr)
        {
            plFileName fileName = comp->GetFileName();
            if (fileName.IsValid())
                custButton->SetText(const_cast<TCHAR*>(ST2T(fileName.GetFileName())));
            else
                custButton->SetText(_M("<None>"));
            ReleaseICustButton(custButton);
        }
    }

    void ISelectWebMFile(plWebMComponent* comp, HWND hDlg, int dlgBtnItemToSet)
    {
        TCHAR fileName[MAX_PATH], dirName[MAX_PATH];
        plFileName origName = comp->GetFileName();

        if (origName.IsValid())
            _tcsncpy(fileName, ST2T(origName.AsString()), std::size(fileName));
        else
            *fileName = _T('\0');

        _tcsncpy(dirName, fileName, std::size(dirName));
        ::PathRemoveFileSpec(dirName);

        OPENFILENAME ofn = { 0 };
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = GetCOREInterface()->GetMAXHWnd();
        ofn.lpstrFilter = _T("WebM Files (*.webm)\0*.webm\0");
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = std::size(fileName);
        ofn.lpstrInitialDir = dirName;
        ofn.lpstrTitle = _T("Choose a WebM video file");
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = _T("webm");

        if (GetOpenFileName(&ofn))
        {
            comp->SetFileName(fileName);
            IUpdateWebMButton(comp, hDlg, dlgBtnItemToSet);
        }
    }

    // Subclasses plSingleCompSelProc (the reusable "Pick" button DlgProc used
    // throughout plSoftVolumeComponent.cpp for soft-region node selection) so this
    // one dialog can host both our own file-picker button and a soft-region picker
    // button without needing a second P_MULTIMAP rollout -- same pattern as
    // plVisRegionComponent's DlgProc in plSoftVolumeComponent.cpp, which handles its
    // own WM_COMMAND cases and falls through to the base class for everything else.
    class plWebMCompProc : public plSingleCompSelProc
    {
    public:
        plWebMCompProc()
            : plSingleCompSelProc((ParamID)plWebMComponent::kSoftRegion, IDC_COMP_WEBM_SOFTREGION_BTN,
                                   _T("Select a soft region for the WebM video's audio"))
        { }

        INT_PTR DlgProc(TimeValue t, IParamMap2* map, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override
        {
            plWebMComponent* comp = (plWebMComponent*)map->GetParamBlock()->GetOwner();

            switch (msg)
            {
            case WM_INITDIALOG:
                IUpdateWebMButton(comp, hWnd, IDC_COMP_WEBM_FILENAME_BTN);
                break;

            case WM_COMMAND:
                if (LOWORD(wParam) == IDC_COMP_WEBM_FILENAME_BTN)
                {
                    ISelectWebMFile(comp, hWnd, IDC_COMP_WEBM_FILENAME_BTN);
                    return TRUE;
                }
                if (LOWORD(wParam) == IDC_COMP_WEBM_FILENAME_CLEAR)
                {
                    comp->SetFileName(_T(""));
                    IUpdateWebMButton(comp, hWnd, IDC_COMP_WEBM_FILENAME_BTN);
                    return TRUE;
                }
                break;
            }

            return plSingleCompSelProc::DlgProc(t, map, hWnd, msg, wParam, lParam);
        }
    };

    plWebMCompProc gWebMCompProc;
}

CLASS_DESC(plWebMComponent, gWebMComponentDesc, "WebM Video", "WebMVideo", COMP_TYPE_GRAPHICS, WEBM_COMPONENT_ID)

ParamBlockDesc2 gWebMBk
(
    plComponent::kBlkComp, _T("WebM Video"), 0, &gWebMComponentDesc, P_AUTO_CONSTRUCT + P_AUTO_UI, plComponent::kRefComp,

    IDD_COMP_WEBM, IDS_COMP_WEBM, 0, 0, &gWebMCompProc,

    plWebMComponent::kFileName, _T("fileName"), TYPE_STRING, 0, 0,
        p_end,

    plWebMComponent::kVolume, _T("volume"), TYPE_FLOAT, 0, 0,
        p_range, 0.f, 4.f,
        p_default, 1.f,
        p_ui, TYPE_SPINNER, EDITTYPE_FLOAT,
        IDC_COMP_WEBM_VOLUME_EDIT, IDC_COMP_WEBM_VOLUME_SPIN, 0.1f,
        p_end,

    plWebMComponent::kMinFalloff, _T("minFalloff"), TYPE_INT, 0, 0,
        p_range, 1, 1000000000,
        p_default, 1,
        p_ui, TYPE_SPINNER, EDITTYPE_POS_INT,
        IDC_COMP_WEBM_MINFALLOFF_EDIT, IDC_COMP_WEBM_MINFALLOFF_SPIN, SPIN_AUTOSCALE,
        p_end,

    plWebMComponent::kMaxFalloff, _T("maxFalloff"), TYPE_INT, 0, 0,
        p_range, 1, 1000000000,
        p_default, 1000000000,
        p_ui, TYPE_SPINNER, EDITTYPE_POS_INT,
        IDC_COMP_WEBM_MAXFALLOFF_EDIT, IDC_COMP_WEBM_MAXFALLOFF_SPIN, SPIN_AUTOSCALE,
        p_end,

    plWebMComponent::kAutoStart, _T("autoStart"), TYPE_BOOL, 0, 0,
        p_default, FALSE,
        p_ui, TYPE_SINGLECHEKBOX, IDC_COMP_WEBM_AUTOSTART_CKBX,
        p_end,

    plWebMComponent::kLoop, _T("loop"), TYPE_BOOL, 0, 0,
        p_default, FALSE,
        p_ui, TYPE_SINGLECHEKBOX, IDC_COMP_WEBM_LOOP_CKBX,
        p_end,

    plWebMComponent::kSoftRegionEnable, _T("enableSoftRegion"), TYPE_BOOL, 0, 0,
        p_default, FALSE,
        p_ui, TYPE_SINGLECHEKBOX, IDC_COMP_WEBM_SOFTENABLE_CKBX,
        p_end,

    plWebMComponent::kSoftRegion, _T("softRegion"), TYPE_INODE, 0, 0,
        p_prompt, IDS_COMP_WEBM_SOFTSELECT,
        p_end,

    p_end
);

plWebMComponent::plWebMComponent()
{
    fClassDesc = &gWebMComponentDesc;
    fClassDesc->MakeAutoParamBlocks(this);
}

plKey plWebMComponent::GetMovieLayerKey(INode* node)
{
    if (!node)
        return nullptr;

    plComponentBase* comp = ((plMaxNodeBase*)node)->ConvertToComponent();
    if (!comp)
        return nullptr;

    if (comp->ClassID() != WEBM_COMPONENT_ID)
        return nullptr;

    return ((plWebMComponent*)comp)->fMovieLayerKey;
}
