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

#include "tcuSurfaceAccess.hpp"

using namespace tcu;

SurfaceAccess::SurfaceAccess(tcu::Surface &surface, const tcu::PixelFormat &colorFmt, int x, int y, int width,
                             int height)
    : m_surface(&surface)
    , m_colorMask(getColorMask(colorFmt))
    , m_x(x)
    , m_y(y)
    , m_width(width)
    , m_height(height)
{
}

SurfaceAccess::SurfaceAccess(tcu::Surface &surface, const tcu::PixelFormat &colorFmt)
    : m_surface(&surface)
    , m_colorMask(getColorMask(colorFmt))
    , m_x(0)
    , m_y(0)
    , m_width(surface.getWidth())
    , m_height(surface.getHeight())
{
}

SurfaceAccess::SurfaceAccess(const SurfaceAccess &parent, int x, int y, int width, int height)
    : m_surface(parent.m_surface)
    , m_colorMask(parent.m_colorMask)
    , m_x(parent.m_x + x)
    , m_y(parent.m_y + y)
    , m_width(width)
    , m_height(height)
{
}
