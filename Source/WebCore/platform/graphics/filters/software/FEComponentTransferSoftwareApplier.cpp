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

namespace WebCore {

void FEComponentTransferSoftwareApplier::computeIdentityTable(LookupTable&, const ComponentTransferFunction&)
{
}

void FEComponentTransferSoftwareApplier::computeTabularTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
{
    const Vector<float>& tableValues = transferFunction.tableValues;
    unsigned n = tableValues.size();
    if (n < 1)
        return;
    for (unsigned i = 0; i < values.size(); ++i) {
        double c = i / 255.0;
        unsigned k = static_cast<unsigned>(c * (n - 1));
        double v1 = tableValues[k];
        double v2 = tableValues[std::min((k + 1), (n - 1))];
        double val = 255.0 * (v1 + (c * (n - 1) - k) * (v2 - v1));
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<uint8_t>(val);
    }
}

void FEComponentTransferSoftwareApplier::computeDiscreteTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
{
    const Vector<float>& tableValues = transferFunction.tableValues;
    unsigned n = tableValues.size();
    if (n < 1)
        return;
    for (unsigned i = 0; i < values.size(); ++i) {
        unsigned k = static_cast<unsigned>((i * n) / 255.0);
        k = std::min(k, n - 1);
        double val = 255 * tableValues[k];
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<uint8_t>(val);
    }
}

void FEComponentTransferSoftwareApplier::computeLinearTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
{
    for (unsigned i = 0; i < values.size(); ++i) {
        double val = transferFunction.slope * i + 255 * transferFunction.intercept;
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<uint8_t>(val);
    }
}

void FEComponentTransferSoftwareApplier::computeGammaTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
{
    for (unsigned i = 0; i < values.size(); ++i) {
        double exponent = transferFunction.exponent; // RCVT doesn't like passing a double and a float to pow, so promote this to double
        double val = 255.0 * (transferFunction.amplitude * pow((i / 255.0), exponent) + transferFunction.offset);
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<uint8_t>(val);
    }
}

FEComponentTransferSoftwareApplier::LookupTable FEComponentTransferSoftwareApplier::computeLookupTable(const ComponentTransferFunction& function)
{
    LookupTable table;

    for (unsigned i = 0; i < table.size(); ++i)
        table[i] = i;

    using TransferType = void (*)(LookupTable&, const ComponentTransferFunction&);
    TransferType callEffect[] = {
        computeIdentityTable,   // FECOMPONENTTRANSFER_TYPE_UNKNOWN
        computeIdentityTable,   // FECOMPONENTTRANSFER_TYPE_IDENTITY
        computeTabularTable,    // FECOMPONENTTRANSFER_TYPE_TABLE
        computeDiscreteTable,   // FECOMPONENTTRANSFER_TYPE_DISCRETE
        computeLinearTable,     // FECOMPONENTTRANSFER_TYPE_LINEAR
        computeGammaTable       // FECOMPONENTTRANSFER_TYPE_GAMMA
    };

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(static_cast<size_t>(function.type) < std::size(callEffect));
    callEffect[function.type](table, function);

    return table;
}

void FEComponentTransferSoftwareApplier::applyPlatform(PixelBuffer& pixelBuffer) const
{
    auto* data = pixelBuffer.bytes();
    auto pixelByteLength = pixelBuffer.sizeInBytes();

    auto redTable   = computeLookupTable(m_effect.redFunction());
    auto greenTable = computeLookupTable(m_effect.greenFunction());
    auto blueTable  = computeLookupTable(m_effect.blueFunction());
    auto alphaTable = computeLookupTable(m_effect.alphaFunction());

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
