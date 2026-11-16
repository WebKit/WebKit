/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "BufferFormatAndroid.h"

#if OS(ANDROID)

#include <android/hardware_buffer.h>
#include <drm/drm_fourcc.h>

namespace WebCore {

std::optional<uint32_t> toAHardwareBufferFormat(const FourCC fourcc)
{
    switch (fourcc.value) {
    case DRM_FORMAT_RGBX8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
    case DRM_FORMAT_RGBA8888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
    case DRM_FORMAT_RGB565:
        return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
    case DRM_FORMAT_RGBA1010102:
        return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
    case DRM_FORMAT_RGB888:
        return AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM;
    default:
        return std::nullopt;
    }
}

} // namespace WebCore

#endif // OS(ANDROID)
