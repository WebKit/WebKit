#ifndef _TCUSURFACEACCESS_HPP
#define _TCUSURFACEACCESS_HPP
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
 * \brief Surface access class.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuPixelFormat.hpp"
#include "tcuSurface.hpp"

namespace tcu
{

inline uint8_t getColorMask(const tcu::PixelFormat &format)
{
    return (uint8_t)((format.redBits ? tcu::RGBA::RED_MASK : 0) | (format.greenBits ? tcu::RGBA::GREEN_MASK : 0) |
                     (format.blueBits ? tcu::RGBA::BLUE_MASK : 0) | (format.alphaBits ? tcu::RGBA::ALPHA_MASK : 0));
}

inline tcu::RGBA toRGBAMasked(const tcu::Vec4 &v, uint8_t mask)
{
    return tcu::RGBA((mask & tcu::RGBA::RED_MASK) ? tcu::floatToU8(v.x()) : 0,
                     (mask & tcu::RGBA::GREEN_MASK) ? tcu::floatToU8(v.y()) : 0,
                     (mask & tcu::RGBA::BLUE_MASK) ? tcu::floatToU8(v.z()) : 0,
                     (mask & tcu::RGBA::ALPHA_MASK) ?
                         tcu::floatToU8(v.w()) :
                         0xFF); //!< \note Alpha defaults to full saturation when reading masked format
}

class SurfaceAccess
{
public:
    SurfaceAccess(tcu::Surface &surface, const tcu::PixelFormat &colorFmt);
    SurfaceAccess(tcu::Surface &surface, const tcu::PixelFormat &colorFmt, int x, int y, int width, int height);
    SurfaceAccess(const SurfaceAccess &parent, int x, int y, int width, int height);

    int getWidth(void) const
    {
        return m_width;
    }
    int getHeight(void) const
    {
        return m_height;
    }

    void setPixel(const tcu::Vec4 &color, int x, int y) const;

private:
    mutable tcu::Surface *m_surface;
    uint8_t m_colorMask;
    int m_x;
    int m_y;
    int m_width;
    int m_height;
};

inline void SurfaceAccess::setPixel(const tcu::Vec4 &color, int x, int y) const
{
    DE_ASSERT(de::inBounds(x, 0, m_width) && de::inBounds(y, 0, m_height));
    m_surface->setPixel(m_x + x, m_y + y, toRGBAMasked(color, m_colorMask));
}

} // namespace tcu

#endif // _TCUSURFACEACCESS_HPP
