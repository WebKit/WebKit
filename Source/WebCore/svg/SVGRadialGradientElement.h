/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "SVGAnimatedLength.h"
#include "SVGGradientElement.h"
#include "SVGNames.h"

namespace WebCore {

struct RadialGradientAttributes;

class SVGRadialGradientElement final : public SVGGradientElement {
    WTF_MAKE_ISO_ALLOCATED(SVGRadialGradientElement);
public:
    static Ref<SVGRadialGradientElement> create(const QualifiedName&, Document&);

    bool collectGradientAttributes(RadialGradientAttributes&);

    const SVGLengthValue& cx() const { return m_cx.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& cy() const { return m_cy.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& r() const { return m_r.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& fx() const { return m_fx.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& fy() const { return m_fy.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& fr() const { return m_fr.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> cxAnimated() { return m_cx.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> cyAnimated() { return m_cy.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> rAnimated() { return m_r.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> fxAnimated() { return m_fx.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> fyAnimated() { return m_fy.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> frAnimated() { return m_fr.animatedProperty(attributeOwnerProxy()); }

private:
    SVGRadialGradientElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGRadialGradientElement, SVGGradientElement>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;

    bool selfHasRelativeLengths() const override;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthAttribute m_cx { LengthModeWidth, "50%" };
    SVGAnimatedLengthAttribute m_cy { LengthModeHeight, "50%" };
    SVGAnimatedLengthAttribute m_r { LengthModeOther, "50%" };
    SVGAnimatedLengthAttribute m_fx { LengthModeWidth };
    SVGAnimatedLengthAttribute m_fy { LengthModeHeight };
    SVGAnimatedLengthAttribute m_fr { LengthModeOther, "0%" };
};

} // namespace WebCore
