/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGAnimatedEnumeration.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<BlendMode> {
    static unsigned highestEnumValue() { return static_cast<unsigned>(BlendMode::Luminosity); }

    static String toString(BlendMode type)
    {
        if (type < BlendMode::PlusDarker)
            return blendModeName(type);

        return emptyString();
    }
};

class SVGFEBlendElement final : public SVGFilterPrimitiveStandardAttributes {
    WTF_MAKE_ISO_ALLOCATED(SVGFEBlendElement);
public:
    static Ref<SVGFEBlendElement> create(const QualifiedName&, Document&);

    String in1() const { return m_in1.currentValue(attributeOwnerProxy()); }
    String in2() const { return m_in2.currentValue(attributeOwnerProxy()); }
    BlendMode mode() const { return m_mode.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedString> in1Animated() { return m_in1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedString> in2Animated() { return m_in2.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> modeAnimated() { return m_mode.animatedProperty(attributeOwnerProxy()); }

private:
    SVGFEBlendElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGFEBlendElement, SVGFilterPrimitiveStandardAttributes>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RefPtr<FilterEffect> build(SVGFilterBuilder*, Filter&) override;
    bool setFilterEffectAttribute(FilterEffect*, const QualifiedName& attrName) override;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedStringAttribute m_in1;
    SVGAnimatedStringAttribute m_in2;
    SVGAnimatedEnumerationAttribute<BlendMode> m_mode { BlendMode::Normal };
};

} // namespace WebCore
