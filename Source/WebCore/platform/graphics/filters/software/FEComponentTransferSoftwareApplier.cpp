/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#if USE(ACCELERATE)
#include <Accelerate/Accelerate.h>
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FEComponentTransferSoftwareApplier);

void FEComponentTransferSoftwareApplier::applyPlatform(PixelBuffer& pixelBuffer) const
{
    auto redTable   = FEComponentTransfer::computeLookupTable(m_effect->redFunction());
    auto greenTable = FEComponentTransfer::computeLookupTable(m_effect->greenFunction());
    auto blueTable  = FEComponentTransfer::computeLookupTable(m_effect->blueFunction());
    auto alphaTable = FEComponentTransfer::computeLookupTable(m_effect->alphaFunction());

#if USE(ACCELERATE)
    auto* pixelBytes = pixelBuffer.bytes().data();
    auto bufferSize = pixelBuffer.size();

    vImage_Buffer src;
    src.width = bufferSize.width();
    src.height = bufferSize.height();
    src.rowBytes = bufferSize.width() * 4;
    src.data = pixelBytes;

    vImage_Buffer dest;
    dest.width = bufferSize.width();
    dest.height = bufferSize.height();
    dest.rowBytes = bufferSize.width() * 4;
    dest.data = pixelBytes;

    // The pixel buffer is in RGBA8 memory order, so the four table arguments map to bytes 0-3 as R, G, B, A
    // (the vImage parameter names follow an ARGB convention but are applied in memory order).
    vImageTableLookUp_ARGB8888(&src, &dest, redTable.data(), greenTable.data(), blueTable.data(), alphaTable.data(), kvImageNoFlags);
#else
    auto data = pixelBuffer.bytes();
    auto pixelByteLength = pixelBuffer.bytes().size();

    for (unsigned pixelOffset = 0; pixelOffset < pixelByteLength; pixelOffset += 4) {
        data[pixelOffset]     = redTable[data[pixelOffset]];
        data[pixelOffset + 1] = greenTable[data[pixelOffset + 1]];
        data[pixelOffset + 2] = blueTable[data[pixelOffset + 2]];
        data[pixelOffset + 3] = alphaTable[data[pixelOffset + 3]];
    }
#endif
}

bool FEComponentTransferSoftwareApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    Ref input = inputs[0];

    RefPtr destinationPixelBuffer = result.pixelBuffer(AlphaPremultiplication::Unpremultiplied);
    if (!destinationPixelBuffer)
        return false;

    auto drawingRect = result.absoluteImageRectRelativeTo(input.get());
    input->copyPixelBuffer(*destinationPixelBuffer, drawingRect);

    applyPlatform(*destinationPixelBuffer);
    return true;
}

} // namespace WebCore
