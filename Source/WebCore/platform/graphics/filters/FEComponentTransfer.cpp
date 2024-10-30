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
#include "FEComponentTransfer.h"

#include "FEComponentTransferSoftwareApplier.h"
#include "Filter.h"
#include <wtf/text/TextStream.h>

#if USE(CORE_IMAGE)
#include "FEComponentTransferCoreImageApplier.h"
#endif

#if USE(SKIA)
#include "FEComponentTransferSkiaApplier.h"
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

Ref<FEComponentTransfer> FEComponentTransfer::create(const ComponentTransferFunction& redFunction, const ComponentTransferFunction& greenFunction, const ComponentTransferFunction& blueFunction, const ComponentTransferFunction& alphaFunction, DestinationColorSpace colorSpace)
{
    return adoptRef(*new FEComponentTransfer(redFunction, greenFunction, blueFunction, alphaFunction, colorSpace));
}

Ref<FEComponentTransfer> FEComponentTransfer::create(ComponentTransferFunctions&& functions)
{
    return adoptRef(*new FEComponentTransfer(WTFMove(functions)));
}

FEComponentTransfer::FEComponentTransfer(const ComponentTransferFunction& redFunction, const ComponentTransferFunction& greenFunction, const ComponentTransferFunction& blueFunction, const ComponentTransferFunction& alphaFunction, DestinationColorSpace colorSpace)
    : FilterEffect(FilterEffect::Type::FEComponentTransfer, colorSpace)
    , m_functions(std::array { redFunction, greenFunction, blueFunction, alphaFunction })
{
}

FEComponentTransfer::FEComponentTransfer(ComponentTransferFunctions&& functions)
    : FilterEffect(FilterEffect::Type::FEComponentTransfer)
    , m_functions(WTFMove(functions))
{
}

bool FEComponentTransfer::operator==(const FEComponentTransfer& other) const
{
    return FilterEffect::operator==(other) && m_functions == other.m_functions;
}

OptionSet<FilterRenderingMode> FEComponentTransfer::supportedFilterRenderingModes() const
{
    OptionSet<FilterRenderingMode> modes = FilterRenderingMode::Software;
#if USE(SKIA)
    modes.add(FilterRenderingMode::Accelerated);
#endif
#if USE(CORE_IMAGE)
    if (FEComponentTransferCoreImageApplier::supportsCoreImageRendering(*this))
        modes.add(FilterRenderingMode::Accelerated);
#endif
    return modes;
}

std::unique_ptr<FilterEffectApplier> FEComponentTransfer::createAcceleratedApplier() const
{
#if USE(CORE_IMAGE)
    return FilterEffectApplier::create<FEComponentTransferCoreImageApplier>(*this);
#elif USE(SKIA)
    return FilterEffectApplier::create<FEComponentTransferSkiaApplier>(*this);
#else
    return nullptr;
#endif
}

std::unique_ptr<FilterEffectApplier> FEComponentTransfer::createSoftwareApplier() const
{
#if USE(SKIA)
    return FilterEffectApplier::create<FEComponentTransferSkiaApplier>(*this);
#else
    return FilterEffectApplier::create<FEComponentTransferSoftwareApplier>(*this);
#endif
}

bool FEComponentTransfer::setType(ComponentTransferChannel channel, ComponentTransferType type)
{
    if (m_functions[channel].type == type)
        return false;

    m_functions[channel].type = type;
    return true;
}

bool FEComponentTransfer::setSlope(ComponentTransferChannel channel, float value)
{
    if (m_functions[channel].slope == value)
        return false;

    m_functions[channel].slope = value;
    return true;
}

bool FEComponentTransfer::setIntercept(ComponentTransferChannel channel, float value)
{
    if (m_functions[channel].intercept == value)
        return false;

    m_functions[channel].intercept = value;
    return true;
}

bool FEComponentTransfer::setAmplitude(ComponentTransferChannel channel, float value)
{
    if (m_functions[channel].amplitude == value)
        return false;

    m_functions[channel].amplitude = value;
    return true;
}

bool FEComponentTransfer::setExponent(ComponentTransferChannel channel, float value)
{
    if (m_functions[channel].exponent == value)
        return false;

    m_functions[channel].exponent = value;
    return true;
}

bool FEComponentTransfer::setOffset(ComponentTransferChannel channel, float value)
{
    if (m_functions[channel].offset == value)
        return false;

    m_functions[channel].offset = value;
    return true;
}

bool FEComponentTransfer::setTableValues(ComponentTransferChannel channel, Vector<float>&& values)
{
    if (m_functions[channel].tableValues == values)
        return false;

    m_functions[channel].tableValues = WTFMove(values);
    return true;
}

static TextStream& operator<<(TextStream& ts, ComponentTransferType type)
{
    switch (type) {
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        ts << "UNKNOWN";
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY:
        ts << "IDENTITY";
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_TABLE:
        ts << "TABLE";
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_DISCRETE:
        ts << "DISCRETE";
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR:
        ts << "LINEAR";
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA:
        ts << "GAMMA";
        break;
    }
    return ts;
}

static TextStream& operator<<(TextStream& ts, const ComponentTransferFunction& function)
{
    ts << "type=\"" << function.type;

    switch (function.type) {
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_UNKNOWN:
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_IDENTITY:
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_TABLE:
        ts << " " << function.tableValues;
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_DISCRETE:
        ts << " " << function.tableValues;
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_LINEAR:
        ts << "\" slope=\"" << function.slope << "\" intercept=\"" << function.intercept << "\"";
        break;
    case ComponentTransferType::FECOMPONENTTRANSFER_TYPE_GAMMA:
        ts << "\" amplitude=\"" << function.amplitude << "\" exponent=\"" << function.exponent << "\" offset=\"" << function.offset << "\"";
        break;
    }

    return ts;
}

TextStream& FEComponentTransfer::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feComponentTransfer";
    FilterEffect::externalRepresentation(ts, representation);
    ts << "\n";

    {
        TextStream::IndentScope indentScope(ts, 2);
        ts << indent << "{red: " << m_functions[ComponentTransferChannel::Red] << "}\n";
        ts << indent << "{green: " << m_functions[ComponentTransferChannel::Green] << "}\n";
        ts << indent << "{blue: " << m_functions[ComponentTransferChannel::Blue] << "}\n";
        ts << indent << "{alpha: " << m_functions[ComponentTransferChannel::Alpha] << "}";
    }

    ts << "]\n";
    return ts;
}

FEComponentTransfer::LookupTable FEComponentTransfer::computeLookupTable(const ComponentTransferFunction& function)
{
    LookupTable values;
    for (unsigned i = 0; i < values.size(); ++i)
        values[i] = i;

    using TransferType = Function<void(const ComponentTransferFunction&)>;
    TransferType callEffect[] = {
        // FECOMPONENTTRANSFER_TYPE_UNKNOWN
        [&](const ComponentTransferFunction&)
        {
        },
        // FECOMPONENTTRANSFER_TYPE_IDENTITY
        [&](const ComponentTransferFunction&)
        {
        },
        // FECOMPONENTTRANSFER_TYPE_TABLE
        [&](const ComponentTransferFunction& transferFunction)
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
        },
        // FECOMPONENTTRANSFER_TYPE_DISCRETE
        [&](const ComponentTransferFunction& transferFunction)
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
        },
        // FECOMPONENTTRANSFER_TYPE_LINEAR
        [&](const ComponentTransferFunction& transferFunction)
        {
            for (unsigned i = 0; i < values.size(); ++i) {
                double val = transferFunction.slope * i + 255 * transferFunction.intercept;
                val = std::max(0.0, std::min(255.0, val));
                values[i] = static_cast<uint8_t>(val);
            }
        },
        // FECOMPONENTTRANSFER_TYPE_GAMMA
        [&](const ComponentTransferFunction& transferFunction)
        {
            for (unsigned i = 0; i < values.size(); ++i) {
                double exponent = transferFunction.exponent; // RCVT doesn't like passing a double and a float to pow, so promote this to double
                double val = 255.0 * (transferFunction.amplitude * pow((i / 255.0), exponent) + transferFunction.offset);
                val = std::max(0.0, std::min(255.0, val));
                values[i] = static_cast<uint8_t>(val);
            }
        }
    };

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(static_cast<size_t>(function.type) < std::size(callEffect));
    callEffect[static_cast<size_t>(function.type)](function);

    return values;
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
