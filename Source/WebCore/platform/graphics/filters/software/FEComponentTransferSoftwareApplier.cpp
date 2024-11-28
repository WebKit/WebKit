/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc.  All rights reserved.
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
#include "FEComponentTransferSoftwareApplier.h"

#include "FEComponentTransfer.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "PixelBuffer.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEComponentTransferSoftwareApplier);

void FEComponentTransferSoftwareApplier::applyPlatform(PixelBuffer& pixelBuffer) const
{
    auto* data = pixelBuffer.bytes().data();
    auto pixelByteLength = pixelBuffer.bytes().size();

    auto redTable   = FEComponentTransfer::computeLookupTable(m_effect.redFunction());
    auto greenTable = FEComponentTransfer::computeLookupTable(m_effect.greenFunction());
    auto blueTable  = FEComponentTransfer::computeLookupTable(m_effect.blueFunction());
    auto alphaTable = FEComponentTransfer::computeLookupTable(m_effect.alphaFunction());

    for (unsigned pixelOffset = 0; pixelOffset < pixelByteLength; pixelOffset += 4) {
        data[pixelOffset]     = redTable[data[pixelOffset]];
        data[pixelOffset + 1] = greenTable[data[pixelOffset + 1]];
        data[pixelOffset + 2] = blueTable[data[pixelOffset + 2]];
        data[pixelOffset + 3] = alphaTable[data[pixelOffset + 3]];
    }
}

bool FEComponentTransferSoftwareApplier::apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const
{
    auto& input = inputs[0].get();
    
    auto destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Unpremultiplied);
    if (!destinationPixelBuffer)
        return false;

    auto drawingRect = result.absoluteImageRectRelativeTo(input);
    input.copyPixelBuffer(*destinationPixelBuffer, drawingRect);

    applyPlatform(*destinationPixelBuffer);
    return true;
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
