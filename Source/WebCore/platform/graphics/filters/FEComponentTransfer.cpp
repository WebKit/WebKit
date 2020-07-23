/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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
#include "FEComponentTransfer.h"

#include "Filter.h"
#include "GraphicsContext.h"
#include "ImageData.h"
#include <wtf/MathExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

FEComponentTransfer::FEComponentTransfer(Filter& filter, const ComponentTransferFunction& redFunction,
    const ComponentTransferFunction& greenFunction, const ComponentTransferFunction& blueFunction, const ComponentTransferFunction& alphaFunction)
    : FilterEffect(filter, Type::ComponentTransfer)
    , m_redFunction(redFunction)
    , m_greenFunction(greenFunction)
    , m_blueFunction(blueFunction)
    , m_alphaFunction(alphaFunction)
{
}

Ref<FEComponentTransfer> FEComponentTransfer::create(Filter& filter, const ComponentTransferFunction& redFunction,
    const ComponentTransferFunction& greenFunction, const ComponentTransferFunction& blueFunction, const ComponentTransferFunction& alphaFunction)
{
    return adoptRef(*new FEComponentTransfer(filter, redFunction, greenFunction, blueFunction, alphaFunction));
}

void FEComponentTransfer::computeIdentityTable(LookupTable&, const ComponentTransferFunction&)
{
}

void FEComponentTransfer::computeTabularTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
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

void FEComponentTransfer::computeDiscreteTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
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

void FEComponentTransfer::computeLinearTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
{
    for (unsigned i = 0; i < values.size(); ++i) {
        double val = transferFunction.slope * i + 255 * transferFunction.intercept;
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<uint8_t>(val);
    }
}

void FEComponentTransfer::computeGammaTable(LookupTable& values, const ComponentTransferFunction& transferFunction)
{
    for (unsigned i = 0; i < values.size(); ++i) {
        double exponent = transferFunction.exponent; // RCVT doesn't like passing a double and a float to pow, so promote this to double
        double val = 255.0 * (transferFunction.amplitude * pow((i / 255.0), exponent) + transferFunction.offset);
        val = std::max(0.0, std::min(255.0, val));
        values[i] = static_cast<uint8_t>(val);
    }
}

void FEComponentTransfer::platformApplySoftware()
{
    FilterEffect* in = inputEffect(0);

    auto* imageResult = createUnmultipliedImageResult();
    auto* pixelArray = imageResult ? imageResult->data() : nullptr;
    if (!pixelArray)
        return;

    LookupTable redTable;
    LookupTable greenTable;
    LookupTable blueTable;
    LookupTable alphaTable;
    computeLookupTables(redTable, greenTable, blueTable, alphaTable);

    IntRect drawingRect = requestedRegionOfInputImageData(in->absolutePaintRect());
    in->copyUnmultipliedResult(*pixelArray, drawingRect, operatingColorSpace());
    unsigned pixelArrayLength = pixelArray->length();
    uint8_t* data = pixelArray->data();
    for (unsigned pixelOffset = 0; pixelOffset < pixelArrayLength; pixelOffset += 4) {
        data[pixelOffset] = redTable[data[pixelOffset]];
        data[pixelOffset + 1] = greenTable[data[pixelOffset + 1]];
        data[pixelOffset + 2] = blueTable[data[pixelOffset + 2]];
        data[pixelOffset + 3] = alphaTable[data[pixelOffset + 3]];
    }
}

void FEComponentTransfer::computeLookupTables(LookupTable& redTable, LookupTable& greenTable, LookupTable& blueTable, LookupTable& alphaTable)
{
    for (unsigned i = 0; i < redTable.size(); ++i)
        redTable[i] = greenTable[i] = blueTable[i] = alphaTable[i] = i;

    using TransferType = void (*)(LookupTable&, const ComponentTransferFunction&);
    TransferType callEffect[] = {
        computeIdentityTable,   // FECOMPONENTTRANSFER_TYPE_UNKNOWN
        computeIdentityTable,   // FECOMPONENTTRANSFER_TYPE_IDENTITY
        computeTabularTable,    // FECOMPONENTTRANSFER_TYPE_TABLE
        computeDiscreteTable,   // FECOMPONENTTRANSFER_TYPE_DISCRETE
        computeLinearTable,     // FECOMPONENTTRANSFER_TYPE_LINEAR
        computeGammaTable       // FECOMPONENTTRANSFER_TYPE_GAMMA
    };

    ASSERT_WITH_SECURITY_IMPLICATION(static_cast<size_t>(m_redFunction.type) < WTF_ARRAY_LENGTH(callEffect));
    ASSERT_WITH_SECURITY_IMPLICATION(static_cast<size_t>(m_greenFunction.type) < WTF_ARRAY_LENGTH(callEffect));
    ASSERT_WITH_SECURITY_IMPLICATION(static_cast<size_t>(m_blueFunction.type) < WTF_ARRAY_LENGTH(callEffect));
    ASSERT_WITH_SECURITY_IMPLICATION(static_cast<size_t>(m_alphaFunction.type) < WTF_ARRAY_LENGTH(callEffect));

    callEffect[m_redFunction.type](redTable, m_redFunction);
    callEffect[m_greenFunction.type](greenTable, m_greenFunction);
    callEffect[m_blueFunction.type](blueTable, m_blueFunction);
    callEffect[m_alphaFunction.type](alphaTable, m_alphaFunction);
}

static TextStream& operator<<(TextStream& ts, ComponentTransferType type)
{
    switch (type) {
    case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case FECOMPONENTTRANSFER_TYPE_IDENTITY:
        ts << "IDENTITY";
        break;
    case FECOMPONENTTRANSFER_TYPE_TABLE:
        ts << "TABLE";
        break;
    case FECOMPONENTTRANSFER_TYPE_DISCRETE:
        ts << "DISCRETE";
        break;
    case FECOMPONENTTRANSFER_TYPE_LINEAR:
        ts << "LINEAR";
        break;
    case FECOMPONENTTRANSFER_TYPE_GAMMA:
        ts << "GAMMA";
        break;
    }
    return ts;
}

static TextStream& operator<<(TextStream& ts, const ComponentTransferFunction& function)
{
    ts << "type=\"" << function.type;

    switch (function.type) {
    case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        break;
    case FECOMPONENTTRANSFER_TYPE_IDENTITY:
        break;
    case FECOMPONENTTRANSFER_TYPE_TABLE:
        ts << " " << function.tableValues;
        break;
    case FECOMPONENTTRANSFER_TYPE_DISCRETE:
        ts << " " << function.tableValues;
        break;
    case FECOMPONENTTRANSFER_TYPE_LINEAR:
        ts << "\" slope=\"" << function.slope << "\" intercept=\"" << function.intercept << "\"";
        break;
    case FECOMPONENTTRANSFER_TYPE_GAMMA:
        ts << "\" amplitude=\"" << function.amplitude << "\" exponent=\"" << function.exponent << "\" offset=\"" << function.offset << "\"";
        break;
    }

    return ts;
}

TextStream& FEComponentTransfer::externalRepresentation(TextStream& ts, RepresentationType representation) const
{
    ts << indent << "[feComponentTransfer";
    FilterEffect::externalRepresentation(ts, representation);
    ts << "\n";
    {
        TextStream::IndentScope indentScope(ts, 2);
        ts << indent << "{red: " << m_redFunction << "}\n";
        ts << indent << "{green: " << m_greenFunction << "}\n";
        ts << indent << "{blue: " << m_blueFunction << "}\n";
        ts << indent << "{alpha: " << m_alphaFunction << "}]\n";
    }

    TextStream::IndentScope indentScope(ts);
    inputEffect(0)->externalRepresentation(ts, representation);
    return ts;
}

} // namespace WebCore
