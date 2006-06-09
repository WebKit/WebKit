/*
 * This file is part of the CSS implementation for KDE.
 *
 * Copyright (C) 2005 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef MEDIAFEATURENAMES_H
#define MEDIAFEATURENAMES_H

#include "AtomicString.h"

namespace WebCore { namespace MediaFeatureNames {

#define CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(macro) \
    macro(color, "color") \
    macro(grid, "grid") \
    macro(monochrome, "monochrome") \
    macro(height, "height") \
    macro(width, "width") \
    macro(device_aspect_ratio, "device-aspect-ratio") \
    macro(device_height, "device-height") \
    macro(device_width, "device-width") \
    macro(max_color, "max-color") \
    macro(max_device_aspect_ratio, "max-device-aspect-ratio") \
    macro(max_device_height, "max-device-height") \
    macro(max_device_width, "max-device-width") \
    macro(max_height, "max-height") \
    macro(max_monochrome, "max-monochrome") \
    macro(max_width, "max-width") \
    macro(min_color, "min-color") \
    macro(min_device_aspect_ratio, "min-device-aspect-ratio") \
    macro(min_device_height, "min-device-height") \
    macro(min_device_width, "min-device-width") \
    macro(min_height, "min-height") \
    macro(min_monochrome, "min-monochrome") \
    macro(min_width, "min-width") \
// end of macro

#if !CSS_MEDIAQUERY_NAMES_HIDE_GLOBALS
    #define CSS_MEDIAQUERY_NAMES_DECLARE(name, str) extern const AtomicString name##MediaFeature;
    CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(CSS_MEDIAQUERY_NAMES_DECLARE)
    #undef CSS_MEDIAQUERY_NAMES_DECLARE
#endif

     void init();
} }

#endif


