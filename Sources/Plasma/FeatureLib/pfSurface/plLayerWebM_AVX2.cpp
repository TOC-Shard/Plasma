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

#include "plLayerWebM_AVX2.h"

#ifdef HAVE_AVX2

#include "hsSIMD.h"

// Vectorized over 8 columns/pixels at a time using 32-bit lanes (16-bit lanes
// would overflow: e.g. u*UB - BB + y1 can reach ~33800 in magnitude, past
// int16 range). Each U/V byte covers 2 columns (4:2:0 subsampling), so 4 real
// samples get duplicated into 8 lanes via a permute. The caller (plLayerWebM.cpp's
// Yuv420ToRgba()) falls back to a plain scalar loop for the last (w % 8)
// columns of each row.
void plLayerWebM_Yuv420ToRgbaRow8Wide(const uint8_t* yRow, const uint8_t* uRow, const uint8_t* vRow, uint8_t* destRow, uint32_t j)
{
    // Same YUV->RGB coefficients as the scalar path in plLayerWebM.cpp's
    // Yuv420ToRgba() -- kept in sync manually since they're trivial constants.
    // Deliberately function-local (not anonymous-namespace) so this stays
    // collision-free with plLayerWebM.cpp's own copy of the same names when
    // MSVC unity-batches both files together (plasma_target_simd_sources()'s
    // SKIP_UNITY_BUILD_INCLUSION only applies on GCC/Clang, since MSVC doesn't
    // need the -mavx2 workaround that requires it).
    constexpr int32_t YG = 74;
    constexpr int32_t UB = 127, UG = -25, UR = 0;
    constexpr int32_t VB = 0, VG = -52, VR = 102;
    constexpr int32_t BB = UB * 128 + VB * 128;
    constexpr int32_t BG = UG * 128 + VG * 128;
    constexpr int32_t BR = UR * 128 + VR * 128;

    const __m256i dupIdx = _mm256_setr_epi32(0, 0, 1, 1, 2, 2, 3, 3);
    const __m256i vYG = _mm256_set1_epi32(YG);
    const __m256i v16 = _mm256_set1_epi32(16);
    const __m256i vUB = _mm256_set1_epi32(UB), vUG = _mm256_set1_epi32(UG), vUR = _mm256_set1_epi32(UR);
    const __m256i vVB = _mm256_set1_epi32(VB), vVG = _mm256_set1_epi32(VG), vVR = _mm256_set1_epi32(VR);
    const __m256i vBB = _mm256_set1_epi32(BB), vBG = _mm256_set1_epi32(BG), vBR = _mm256_set1_epi32(BR);
    const __m256i vZero = _mm256_setzero_si256();
    const __m256i v255 = _mm256_set1_epi32(255);

    __m128i yBytes = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(yRow + j));
    __m256i yVals = _mm256_cvtepu8_epi32(yBytes);

    // Read exactly 4 bytes (not 8) -- reading a full 8-byte lane here would run
    // past the end of the U/V plane's row on the last iteration of a row.
    __m128i uBytes4 = _mm_cvtsi32_si128(*reinterpret_cast<const int32_t*>(uRow + j / 2));
    __m256i uVals = _mm256_permutevar8x32_epi32(_mm256_cvtepu8_epi32(uBytes4), dupIdx);

    __m128i vBytes4 = _mm_cvtsi32_si128(*reinterpret_cast<const int32_t*>(vRow + j / 2));
    __m256i vVals = _mm256_permutevar8x32_epi32(_mm256_cvtepu8_epi32(vBytes4), dupIdx);

    __m256i y1 = _mm256_mullo_epi32(_mm256_sub_epi32(yVals, v16), vYG);

    __m256i r = _mm256_srai_epi32(_mm256_add_epi32(_mm256_sub_epi32(_mm256_add_epi32(
        _mm256_mullo_epi32(uVals, vUB), _mm256_mullo_epi32(vVals, vVB)), vBB), y1), 6);
    __m256i g = _mm256_srai_epi32(_mm256_add_epi32(_mm256_sub_epi32(_mm256_add_epi32(
        _mm256_mullo_epi32(uVals, vUG), _mm256_mullo_epi32(vVals, vVG)), vBG), y1), 6);
    __m256i b = _mm256_srai_epi32(_mm256_add_epi32(_mm256_sub_epi32(_mm256_add_epi32(
        _mm256_mullo_epi32(uVals, vUR), _mm256_mullo_epi32(vVals, vVR)), vBR), y1), 6);

    r = _mm256_min_epi32(_mm256_max_epi32(r, vZero), v255);
    g = _mm256_min_epi32(_mm256_max_epi32(g, vZero), v255);
    b = _mm256_min_epi32(_mm256_max_epi32(b, vZero), v255);

    int32_t rArr[8], gArr[8], bArr[8];
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(rArr), r);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(gArr), g);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(bArr), b);

    uint8_t* d = destRow + (size_t)j * 4;
    for (int k = 0; k < 8; ++k)
    {
        d[k * 4 + 0] = (uint8_t)rArr[k];
        d[k * 4 + 1] = (uint8_t)gArr[k];
        d[k * 4 + 2] = (uint8_t)bArr[k];
        d[k * 4 + 3] = 0xff;
    }
}

#endif // HAVE_AVX2
