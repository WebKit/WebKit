/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
#include "SourceAlphaSoftwareApplier.h"

#include "Color.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "SourceAlpha.h"

namespace WebCore {

bool SourceAlphaSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();

    auto resultImage = result.imageBuffer();
    if (!resultImage)
        return false;
    
    auto inputImage = input.imageBuffer();
    if (!inputImage)
        return false;

    FloatRect imageRect(FloatPoint(), result.absoluteImageRect().size());
    auto& filterContext = resultImage->context();

    filterContext.fillRect(imageRect, Color::black);
    filterContext.drawImageBuffer(*inputImage, IntPoint(), CompositeOperator::DestinationIn);
    return true;
}

} // namespace WebCore
