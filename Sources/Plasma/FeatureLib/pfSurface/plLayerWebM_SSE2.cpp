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

#include "plLayerWebM_SSE2.h"

#ifdef HAVE_SSE2

#include "hsSIMD.h"

// Vectorized over 4 columns/pixels at a time using 32-bit lanes (same overflow
// reasoning as the AVX2 path: 16-bit lanes aren't wide enough). Each U/V byte
// covers 2 columns (4:2:0 subsampling), so 2 real samples get duplicated into
// 4 lanes. The caller (plLayerWebM.cpp's Yuv420ToRgba()) falls back to a plain
// scalar loop for whatever's left over after both this and (if the CPU
// actually has it, checked at runtime) the wider AVX2 path have run.
void plLayerWebM_Yuv420ToRgbaRow4Wide(const uint8_t* yRow, const uint8_t* uRow, const uint8_t* vRow, uint8_t* destRow, uint32_t j)
{
    // Same YUV->RGB coefficients as the scalar path in plLayerWebM.cpp's
    // Yuv420ToRgba() -- kept in sync manually since they're trivial constants.
    // Deliberately function-local (not anonymous-namespace) so this stays
    // collision-free with plLayerWebM.cpp's own copy of the same names if
    // MSVC unity-batches both files together (see plLayerWebM_AVX2.cpp's
    // identical reasoning).
    constexpr int32_t YG = 74;
    constexpr int32_t UB = 127, UG = -25, UR = 0;
    constexpr int32_t VB = 0, VG = -52, VR = 102;
    constexpr int32_t BB = UB * 128 + VB * 128;
    constexpr int32_t BG = UG * 128 + VG * 128;
    constexpr int32_t BR = UR * 128 + VR * 128;

    // SSE2 has neither a 32-bit lane multiply (_mm_mullo_epi32 is SSE4.1) nor
    // signed 32-bit min/max (also SSE4.1) -- emulate both here with the
    // classic pre-SSE4.1 tricks. Local lambdas rather than file-scope helpers
    // for the same Unity-batch-collision reason as the constants above.
    auto mullo32 = [](__m128i a, __m128i b) -> __m128i {
        __m128i evenProducts = _mm_mul_epu32(a, b);
        __m128i oddProducts  = _mm_mul_epu32(_mm_srli_si128(a, 4), _mm_srli_si128(b, 4));
        return _mm_unpacklo_epi32(_mm_shuffle_epi32(evenProducts, _MM_SHUFFLE(0, 0, 2, 0)),
                                   _mm_shuffle_epi32(oddProducts, _MM_SHUFFLE(0, 0, 2, 0)));
    };
    auto max32 = [](__m128i a, __m128i b) -> __m128i {
        __m128i gt = _mm_cmpgt_epi32(a, b);
        return _mm_or_si128(_mm_and_si128(gt, a), _mm_andnot_si128(gt, b));
    };
    auto min32 = [](__m128i a, __m128i b) -> __m128i {
        __m128i gt = _mm_cmpgt_epi32(a, b);
        return _mm_or_si128(_mm_and_si128(gt, b), _mm_andnot_si128(gt, a));
    };

    const __m128i yVals = _mm_set_epi32(yRow[j + 3], yRow[j + 2], yRow[j + 1], yRow[j + 0]);

    // Only 2 real U/V bytes needed per 4 output columns -- duplicate them into
    // the 4 lanes with plain reads rather than an intrinsic shuffle, simpler
    // for a duplication this small.
    const uint32_t uvIdx = j / 2;
    const __m128i uVals = _mm_set_epi32(uRow[uvIdx + 1], uRow[uvIdx + 1], uRow[uvIdx + 0], uRow[uvIdx + 0]);
    const __m128i vVals = _mm_set_epi32(vRow[uvIdx + 1], vRow[uvIdx + 1], vRow[uvIdx + 0], vRow[uvIdx + 0]);

    const __m128i vYG = _mm_set1_epi32(YG);
    const __m128i v16 = _mm_set1_epi32(16);
    const __m128i vUB = _mm_set1_epi32(UB), vUG = _mm_set1_epi32(UG), vUR = _mm_set1_epi32(UR);
    const __m128i vVB = _mm_set1_epi32(VB), vVG = _mm_set1_epi32(VG), vVR = _mm_set1_epi32(VR);
    const __m128i vBB = _mm_set1_epi32(BB), vBG = _mm_set1_epi32(BG), vBR = _mm_set1_epi32(BR);
    const __m128i vZero = _mm_setzero_si128();
    const __m128i v255 = _mm_set1_epi32(255);

    __m128i y1 = mullo32(_mm_sub_epi32(yVals, v16), vYG);

    __m128i r = _mm_srai_epi32(_mm_add_epi32(_mm_sub_epi32(_mm_add_epi32(
        mullo32(uVals, vUB), mullo32(vVals, vVB)), vBB), y1), 6);
    __m128i g = _mm_srai_epi32(_mm_add_epi32(_mm_sub_epi32(_mm_add_epi32(
        mullo32(uVals, vUG), mullo32(vVals, vVG)), vBG), y1), 6);
    __m128i b = _mm_srai_epi32(_mm_add_epi32(_mm_sub_epi32(_mm_add_epi32(
        mullo32(uVals, vUR), mullo32(vVals, vVR)), vBR), y1), 6);

    r = min32(max32(r, vZero), v255);
    g = min32(max32(g, vZero), v255);
    b = min32(max32(b, vZero), v255);

    int32_t rArr[4], gArr[4], bArr[4];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(rArr), r);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(gArr), g);
    _mm_storeu_si128(reinterpret_cast<__m128i*>(bArr), b);

    uint8_t* d = destRow + (size_t)j * 4;
    for (int k = 0; k < 4; ++k)
    {
        d[k * 4 + 0] = (uint8_t)rArr[k];
        d[k * 4 + 1] = (uint8_t)gArr[k];
        d[k * 4 + 2] = (uint8_t)bArr[k];
        d[k * 4 + 3] = 0xff;
    }
}

#endif // HAVE_SSE2
