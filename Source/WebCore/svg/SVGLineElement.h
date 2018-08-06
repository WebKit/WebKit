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

class SVGLineElement final : public SVGGeometryElement, public SVGExternalResourcesRequired {
    WTF_MAKE_ISO_ALLOCATED(SVGLineElement);
public:
    static Ref<SVGLineElement> create(const QualifiedName&, Document&);

    const SVGLengthValue& x1() const { return m_x1.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y1() const { return m_y1.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& x2() const { return m_x2.currentValue(attributeOwnerProxy()); }
    const SVGLengthValue& y2() const { return m_y2.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> x1Animated() { return m_x1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> y1Animated() { return m_y1.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> x2Animated() { return m_x2.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedLength> y2Animated() { return m_y2.animatedProperty(attributeOwnerProxy()); }

private:
    SVGLineElement(const QualifiedName&, Document&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGLineElement, SVGGeometryElement, SVGExternalResourcesRequired>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    static void registerAttributes();

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const final { return m_attributeOwnerProxy; }
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool isValid() const final { return SVGTests::isValid(); }
    bool supportsMarkers() const final { return true; }
    bool selfHasRelativeLengths() const final;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedLengthAttribute m_x1 { LengthModeWidth };
    SVGAnimatedLengthAttribute m_y1 { LengthModeHeight };
    SVGAnimatedLengthAttribute m_x2 { LengthModeWidth };
    SVGAnimatedLengthAttribute m_y2 { LengthModeHeight };
};

} // namespace WebCore
