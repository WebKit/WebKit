/*
 * Copyright (C) 2005, 2013 Apple Inc. All rights reserved.
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
 *
 */

#pragma once

#include <wtf/text/AtomicString.h>

#if ENABLE(VIEW_MODE_CSS_MEDIA)
#define CSS_MEDIAQUERY_VIEW_MODE(macro) macro(viewMode, "-webkit-view-mode")
#else
#define CSS_MEDIAQUERY_VIEW_MODE(macro)
#endif

#define CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(macro) \
    macro(animation, "-webkit-animation") \
    macro(anyHover, "any-hover") \
    macro(anyPointer, "any-pointer") \
    macro(aspectRatio, "aspect-ratio") \
    macro(color, "color") \
    macro(colorGamut, "color-gamut") \
    macro(colorIndex, "color-index") \
    macro(deviceAspectRatio, "device-aspect-ratio") \
    macro(deviceHeight, "device-height") \
    macro(devicePixelRatio, "-webkit-device-pixel-ratio") \
    macro(deviceWidth, "device-width") \
    macro(grid, "grid") \
    macro(height, "height") \
    macro(hover, "hover") \
    macro(invertedColors, "inverted-colors") \
    macro(maxAspectRatio, "max-aspect-ratio") \
    macro(maxColor, "max-color") \
    macro(maxColorIndex, "max-color-index") \
    macro(maxDeviceAspectRatio, "max-device-aspect-ratio") \
    macro(maxDeviceHeight, "max-device-height") \
    macro(maxDevicePixelRatio, "-webkit-max-device-pixel-ratio") \
    macro(maxDeviceWidth, "max-device-width") \
    macro(maxHeight, "max-height") \
    macro(maxMonochrome, "max-monochrome") \
    macro(maxResolution, "max-resolution") \
    macro(maxWidth, "max-width") \
    macro(minAspectRatio, "min-aspect-ratio") \
    macro(minColor, "min-color") \
    macro(minColorIndex, "min-color-index") \
    macro(minDeviceAspectRatio, "min-device-aspect-ratio") \
    macro(minDeviceHeight, "min-device-height") \
    macro(minDevicePixelRatio, "-webkit-min-device-pixel-ratio") \
    macro(minDeviceWidth, "min-device-width") \
    macro(minHeight, "min-height") \
    macro(minMonochrome, "min-monochrome") \
    macro(minResolution, "min-resolution") \
    macro(minWidth, "min-width") \
    macro(monochrome, "monochrome") \
    macro(orientation, "orientation") \
    macro(pointer, "pointer") \
    macro(resolution, "resolution") \
    macro(transform2d, "-webkit-transform-2d") \
    macro(transform3d, "-webkit-transform-3d") \
    macro(transition, "-webkit-transition") \
    macro(videoPlayableInline, "-webkit-video-playable-inline") \
    macro(width, "width") \
    CSS_MEDIAQUERY_VIEW_MODE(macro)

// end of macro

namespace WebCore {
namespace MediaFeatureNames {

#ifndef CSS_MEDIAQUERY_NAMES_HIDE_GLOBALS
#define CSS_MEDIAQUERY_NAMES_DECLARE(name, string) extern const AtomicString name;
    CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(CSS_MEDIAQUERY_NAMES_DECLARE)
#undef CSS_MEDIAQUERY_NAMES_DECLARE
#endif

    void init();

} // namespace MediaFeatureNames
} // namespace WebCore
