#ifndef _TCUPIXELFORMAT_HPP
#define _TCUPIXELFORMAT_HPP
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
 * \brief Pixel format descriptor.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuRGBA.hpp"

namespace tcu
{

/*--------------------------------------------------------------------*//*!
 * \brief Fixed-point render target pixel format
 *//*--------------------------------------------------------------------*/
struct PixelFormat
{
    int redBits;
    int greenBits;
    int blueBits;
    int alphaBits;

    PixelFormat(int red, int green, int blue, int alpha)
        : redBits(red)
        , greenBits(green)
        , blueBits(blue)
        , alphaBits(alpha)
    {
    }

    PixelFormat(void) : redBits(0), greenBits(0), blueBits(0), alphaBits(0)
    {
    }

    static inline int channelThreshold(int bits)
    {
        if (bits <= 8)
        {
            // Threshold is 2^(8 - bits)
            return 1 << (8 - bits);
        }
        else
        {
            // Threshold is bound by the 8-bit buffer value
            return 1;
        }
    }

    /*--------------------------------------------------------------------*//*!
     * \brief Get default threshold for per-pixel comparison for this format
     *
     * Per-channel threshold is 2^(8-bits). If alpha channel bits are zero,
     * threshold for that channel is 0.
     *//*--------------------------------------------------------------------*/
    inline RGBA getColorThreshold(void) const
    {
        return RGBA(channelThreshold(redBits), channelThreshold(greenBits), channelThreshold(blueBits),
                    alphaBits ? channelThreshold(alphaBits) : 0);
    }

    static inline int convertChannel(int val, int bits)
    {
        if (bits == 0)
        {
            return 0;
        }
        else if (bits == 1)
        {
            return (val & 0x80) ? 0xff : 0;
        }
        else if (bits < 8)
        {
            // Emulate precision reduction by replicating the upper bits as the fractional component
            int intComp   = val >> (8 - bits);
            int fractComp = (intComp << (24 - bits)) | (intComp << (24 - 2 * bits)) | (intComp << (24 - 3 * bits));
            return (intComp << (8 - bits)) | (fractComp >> (bits + 16));
        }
        else
        {
            // Bits greater than or equal to 8 will have full precision, so no reduction
            return val;
        }
    }

    /*--------------------------------------------------------------------*//*!
     * \brief Emulate reduced bit depth
     *
     * The color value bit depth is reduced and converted back. The lowest
     * bits are filled by replicating the upper bits.
     *//*--------------------------------------------------------------------*/
    inline RGBA convertColor(const RGBA &col) const
    {
        return RGBA(convertChannel(col.getRed(), redBits), convertChannel(col.getGreen(), greenBits),
                    convertChannel(col.getBlue(), blueBits),
                    alphaBits ? convertChannel(col.getAlpha(), alphaBits) : 0xff);
    }

    inline bool operator==(const PixelFormat &other) const
    {
        return redBits == other.redBits && greenBits == other.greenBits && blueBits == other.blueBits &&
               alphaBits == other.alphaBits;
    }

    inline bool operator!=(const PixelFormat &other) const
    {
        return !(*this == other);
    }
} DE_WARN_UNUSED_TYPE;

} // namespace tcu

#endif // _TCUPIXELFORMAT_HPP
