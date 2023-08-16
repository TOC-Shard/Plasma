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

#include "hsMessageBox.h"

bool hsMessageBox_SuppressPrompts = false;

#if !defined(HS_BUILD_FOR_APPLE) && !defined(HS_BUILD_FOR_WIN32)
#include <string_theory/format>

// Need a proper implementation for Linux, but for now let's just print out to the console
hsMessageBoxResult hsMessageBox(const ST::string& message, const ST::string& caption, hsMessageBoxKind kind, hsMessageBoxIcon icon)
{
    if (hsMessageBox_SuppressPrompts)
        return hsMBoxOk;

    hsStatusMessage(ST::format("{}\n{}", message, caption).c_str());
    return hsMBoxCancel;
}
#endif

hsMessageBoxResult hsMessageBox(const char* message, const char* caption, hsMessageBoxKind kind, hsMessageBoxIcon icon)
{
    return hsMessageBox(ST::string::from_latin_1(message), ST::string::from_latin_1(caption), kind, icon);
}

hsMessageBoxResult hsMessageBox(const wchar_t* message, const wchar_t* caption, hsMessageBoxKind kind, hsMessageBoxIcon icon)
{
    return hsMessageBox(ST::string::from_wchar(message), ST::string::from_wchar(caption), kind, icon);
}
