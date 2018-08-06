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
#include "SVGExternalResourcesRequired.h"
#include "SVGGeometryElement.h"
#include "SVGNames.h"

namespace WebCore {

class SVGCircleElement final : public SVGGeometryElement, public SVGExternalResourcesRequired {
    WTF_MAKE_ISO_ALLOCATED(SVGCircleElement);
public:
    static Ref<SVGCircleElement> create(const QualifiedName&, Document&);

    const SVGLengthValue& cx() const { return m_cx.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& cy() const { return m_cy.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& r() const { return m_r.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> cxAnimated() { return m_cx.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> cyAnimated() { return m_cy.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> rAnimated() { return m_r.animatedProperty(attributeOwnerProxy()); }

private:
    SVGCircleElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGCircleElement, SVGGeometryElement, SVGExternalResourcesRequired>;
    static auto& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool selfHasRelativeLengths() const final { return true; }

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) final;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthAttribute m_cx { LengthModeWidth };
    SVGAnimatedLengthAttribute m_cy { LengthModeHeight };
    SVGAnimatedLengthAttribute m_r { LengthModeOther };
};

} // namespace WebCore
