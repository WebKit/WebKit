/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "SourceGraphicSoftwareApplier.h"

#include "FilterImage.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SourceGraphicSoftwareApplier);

bool SourceGraphicSoftwareApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    Ref input = inputs[0].get();

    RefPtr resultImage = result.imageBuffer();
    RefPtr sourceImage = input->imageBuffer();
    if (!resultImage || !sourceImage)
        return false;

    // The result buffer's origin is result.absoluteImageRect().location() in absolute
    // coords; the source buffer's origin is input->absoluteImageRect().location().
    // When the source extends beyond the result (e.g. CSS-referenced SVG filters where
    // the source canvas is layer bounds but the result is clipped to filterRegion),
    // we need to draw the source at this offset to keep pixels in the same coord system.
    auto offset = input->absoluteImageRect().location() - result.absoluteImageRect().location();
    resultImage->context().drawImageBuffer(*sourceImage, IntPoint(offset));
    return true;
}

} // namespace WebCore
