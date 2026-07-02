/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief RGBA8888 color type.
 *//*--------------------------------------------------------------------*/

#include "tcuRGBA.hpp"
#include "tcuVector.hpp"
#include "tcuTextureUtil.hpp"

namespace tcu
{

RGBA::RGBA(const Vec4 &v)
{
    const uint32_t r = (uint32_t)floatToU8(v.x());
    const uint32_t g = (uint32_t)floatToU8(v.y());
    const uint32_t b = (uint32_t)floatToU8(v.z());
    const uint32_t a = (uint32_t)floatToU8(v.w());
    m_value          = (a << ALPHA_SHIFT) | (r << RED_SHIFT) | (g << GREEN_SHIFT) | (b << BLUE_SHIFT);
}

Vec4 RGBA::toVec(void) const
{
    return Vec4(float(getRed()) / 255.0f, float(getGreen()) / 255.0f, float(getBlue()) / 255.0f,
                float(getAlpha()) / 255.0f);
}

IVec4 RGBA::toIVec(void) const
{
    return IVec4(getRed(), getGreen(), getBlue(), getAlpha());
}

RGBA computeAbsDiffMasked(RGBA a, RGBA b, uint32_t cmpMask)
{
    uint32_t aPacked = a.getPacked();
    uint32_t bPacked = b.getPacked();
    uint8_t rDiff    = 0;
    uint8_t gDiff    = 0;
    uint8_t bDiff    = 0;
    uint8_t aDiff    = 0;

    if (cmpMask & RGBA::RED_MASK)
    {
        int ra = (aPacked >> RGBA::RED_SHIFT) & 0xFF;
        int rb = (bPacked >> RGBA::RED_SHIFT) & 0xFF;

        rDiff = (uint8_t)deAbs32(ra - rb);
    }

    if (cmpMask & RGBA::GREEN_MASK)
    {
        int ga = (aPacked >> RGBA::GREEN_SHIFT) & 0xFF;
        int gb = (bPacked >> RGBA::GREEN_SHIFT) & 0xFF;

        gDiff = (uint8_t)deAbs32(ga - gb);
    }

    if (cmpMask & RGBA::BLUE_MASK)
    {
        int ba = (aPacked >> RGBA::BLUE_SHIFT) & 0xFF;
        int bb = (bPacked >> RGBA::BLUE_SHIFT) & 0xFF;

        bDiff = (uint8_t)deAbs32(ba - bb);
    }

    if (cmpMask & RGBA::ALPHA_MASK)
    {
        int aa = (aPacked >> RGBA::ALPHA_SHIFT) & 0xFF;
        int ab = (bPacked >> RGBA::ALPHA_SHIFT) & 0xFF;

        aDiff = (uint8_t)deAbs32(aa - ab);
    }

    return RGBA(rDiff, gDiff, bDiff, aDiff);
}

bool compareThresholdMasked(RGBA a, RGBA b, RGBA threshold, uint32_t cmpMask)
{
    return computeAbsDiffMasked(a, b, cmpMask).isBelowThreshold(threshold);
}

} // namespace tcu
