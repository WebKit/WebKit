/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2021-2023 Apple Inc.  All rights reserved.
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

#pragma once

#include "FilterEffect.h"
#include <wtf/EnumeratedArray.h>
#include <wtf/Vector.h>

namespace WebCore {

enum ComponentTransferType {
    FECOMPONENTTRANSFER_TYPE_UNKNOWN  = 0,
    FECOMPONENTTRANSFER_TYPE_IDENTITY = 1,
    FECOMPONENTTRANSFER_TYPE_TABLE    = 2,
    FECOMPONENTTRANSFER_TYPE_DISCRETE = 3,
    FECOMPONENTTRANSFER_TYPE_LINEAR   = 4,
    FECOMPONENTTRANSFER_TYPE_GAMMA    = 5
};

struct ComponentTransferFunction {
    ComponentTransferType type { FECOMPONENTTRANSFER_TYPE_UNKNOWN };

    float slope { 0 };
    float intercept { 0 };
    float amplitude { 0 };
    float exponent { 0 };
    float offset { 0 };

    Vector<float> tableValues;
};

enum class ComponentTransferChannel : uint8_t { Red, Green, Blue, Alpha };

using ComponentTransferFunctions = EnumeratedArray<ComponentTransferChannel, ComponentTransferFunction, ComponentTransferChannel::Alpha>;

class FEComponentTransfer : public FilterEffect {
public:
    WEBCORE_EXPORT static Ref<FEComponentTransfer> create(const ComponentTransferFunction& redFunc, const ComponentTransferFunction& greenFunc, const ComponentTransferFunction& blueFunc, const ComponentTransferFunction& alphaFunc);
    static Ref<FEComponentTransfer> create(ComponentTransferFunctions&&);

    ComponentTransferFunction redFunction() const { return m_functions[ComponentTransferChannel::Red]; }
    ComponentTransferFunction greenFunction() const { return m_functions[ComponentTransferChannel::Green]; }
    ComponentTransferFunction blueFunction() const { return m_functions[ComponentTransferChannel::Blue]; }
    ComponentTransferFunction alphaFunction() const { return m_functions[ComponentTransferChannel::Alpha]; }

    bool setType(ComponentTransferChannel, ComponentTransferType);
    bool setSlope(ComponentTransferChannel, float);
    bool setIntercept(ComponentTransferChannel, float);
    bool setAmplitude(ComponentTransferChannel, float);
    bool setExponent(ComponentTransferChannel, float);
    bool setOffset(ComponentTransferChannel, float);
    bool setTableValues(ComponentTransferChannel, Vector<float>&&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<FEComponentTransfer>> decode(Decoder&);

private:
    FEComponentTransfer(const ComponentTransferFunction& redFunc, const ComponentTransferFunction& greenFunc, const ComponentTransferFunction& blueFunc, const ComponentTransferFunction& alphaFunc);
    FEComponentTransfer(ComponentTransferFunctions&&);

    OptionSet<FilterRenderingMode> supportedFilterRenderingModes() const override;
    std::unique_ptr<FilterEffectApplier> createAcceleratedApplier() const override;
    std::unique_ptr<FilterEffectApplier> createSoftwareApplier() const override;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const override;

    ComponentTransferFunctions m_functions;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ComponentTransferType> {
    using values = EnumValues<
        WebCore::ComponentTransferType,

        WebCore::FECOMPONENTTRANSFER_TYPE_UNKNOWN,
        WebCore::FECOMPONENTTRANSFER_TYPE_IDENTITY,
        WebCore::FECOMPONENTTRANSFER_TYPE_TABLE,
        WebCore::FECOMPONENTTRANSFER_TYPE_DISCRETE,
        WebCore::FECOMPONENTTRANSFER_TYPE_LINEAR,
        WebCore::FECOMPONENTTRANSFER_TYPE_GAMMA
    >;
};

} // namespace WTF

SPECIALIZE_TYPE_TRAITS_FILTER_EFFECT(FEComponentTransfer)
