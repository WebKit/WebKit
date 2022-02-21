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

namespace WebCore {

Ref<FEComponentTransfer> FEComponentTransfer::create(const ComponentTransferFunction& redFunction, const ComponentTransferFunction& greenFunction, const ComponentTransferFunction& blueFunction, const ComponentTransferFunction& alphaFunction)
{
    return adoptRef(*new FEComponentTransfer(redFunction, greenFunction, blueFunction, alphaFunction));
}

FEComponentTransfer::FEComponentTransfer(const ComponentTransferFunction& redFunction, const ComponentTransferFunction& greenFunction, const ComponentTransferFunction& blueFunction, const ComponentTransferFunction& alphaFunction)
    : FilterEffect(FilterEffect::Type::FEComponentTransfer)
    , m_redFunction(redFunction)
    , m_greenFunction(greenFunction)
    , m_blueFunction(blueFunction)
    , m_alphaFunction(alphaFunction)
{
}

bool FEComponentTransfer::supportsAcceleratedRendering() const
{
#if USE(CORE_IMAGE)
    return FEComponentTransferCoreImageApplier::supportsCoreImageRendering(*this);
#else
    return false;
#endif
}

std::unique_ptr<FilterEffectApplier> FEComponentTransfer::createAcceleratedApplier() const
{
#if USE(CORE_IMAGE)
    return FilterEffectApplier::create<FEComponentTransferCoreImageApplier>(*this);
#else
    return nullptr;
#endif
}

std::unique_ptr<FilterEffectApplier> FEComponentTransfer::createSoftwareApplier() const
{
    return FilterEffectApplier::create<FEComponentTransferSoftwareApplier>(*this);
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

TextStream& FEComponentTransfer::externalRepresentation(TextStream& ts, FilterRepresentation representation) const
{
    ts << indent << "[feComponentTransfer";
    FilterEffect::externalRepresentation(ts, representation);
    ts << "\n";

    {
        TextStream::IndentScope indentScope(ts, 2);
        ts << indent << "{red: " << m_redFunction << "}\n";
        ts << indent << "{green: " << m_greenFunction << "}\n";
        ts << indent << "{blue: " << m_blueFunction << "}\n";
        ts << indent << "{alpha: " << m_alphaFunction << "}";
    }

    ts << "]\n";
    return ts;
}

} // namespace WebCore
