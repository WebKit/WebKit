/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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

#include "GraphicsTypes.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

template<>
struct SVGPropertyTraits<BlendMode> {
    static unsigned highestEnumValue() { return static_cast<unsigned>(BlendMode::Luminosity); }

    static BlendMode fromString(const String& string)
    {
        BlendMode mode = BlendMode::Normal;
        parseBlendMode(string, mode);
        return mode;
    }

    static String toString(BlendMode type)
    {
        if (type < BlendMode::PlusDarker)
            return blendModeName(type);

        return emptyString();
    }
};

class SVGFEBlendElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(SVGFEBlendElement);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SVGFEBlendElement);
public:
    static Ref<SVGFEBlendElement> create(const QualifiedName&, Document&);

    String in1() const { return m_in1->currentValue(); }
    String in2() const { return m_in2->currentValue(); }
    BlendMode mode() const { return m_mode->currentValue<BlendMode>(); }

    SVGAnimatedString& in1Animated() { return m_in1; }
    SVGAnimatedString& in2Animated() { return m_in2; }
    SVGAnimatedEnumeration& modeAnimated() { return m_mode; }

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGFEBlendElement, SVGFilterPrimitiveStandardAttributes>;

private:
    SVGFEBlendElement(const QualifiedName&, Document&);

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool setFilterEffectAttribute(FilterEffect&, const QualifiedName& attrName) override;
    Vector<AtomString> filterEffectInputsNames() const override { return { AtomString { in1() }, AtomString { in2() } }; }
    RefPtr<FilterEffect> createFilterEffect(const FilterEffectVector&, const GraphicsContext& destinationContext) const override;

    Ref<SVGAnimatedString> m_in1 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedString> m_in2 { SVGAnimatedString::create(this) };
    Ref<SVGAnimatedEnumeration> m_mode { SVGAnimatedEnumeration::create(this, BlendMode::Normal) };
};

} // namespace WebCore
