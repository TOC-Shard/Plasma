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

#ifndef plLayerWebM_SSE2_inc
#define plLayerWebM_SSE2_inc

#include "hsConfig.h"

#include <cstdint>

// SSE2 fallback for plLayerWebM_Yuv420ToRgbaRow8Wide() (see plLayerWebM_AVX2.h):
// AVX2 support can't be assumed just because HAVE_AVX2 was true on the build
// machine's compiler -- that only means the compiler *can* target AVX2, not
// that every player's CPU actually has it. A player crashed on an AVX2-less
// CPU (illegal instruction) hitting the unconditional AVX2 path. SSE2, unlike
// AVX2, is part of the mandatory x86-64 baseline -- every 64-bit-capable CPU
// has it, so this path needs no runtime capability check at its call site
// (see plLayerWebM.cpp's Yuv420ToRgba(), which does check has_avx2 via
// hsCpuID before preferring the wider AVX2 path).
#ifdef HAVE_SSE2
void plLayerWebM_Yuv420ToRgbaRow4Wide(const uint8_t* yRow, const uint8_t* uRow, const uint8_t* vRow, uint8_t* destRow, uint32_t j);
#endif

#endif // plLayerWebM_SSE2_inc
