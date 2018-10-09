/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
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

#include "Path.h"
#include "SVGAnimatedNumber.h"
#include "SVGGraphicsElement.h"
#include "SVGNames.h"

namespace WebCore {

struct DOMPointInit;
class SVGPoint;

class SVGGeometryElement : public SVGGraphicsElement {
    WTF_MAKE_ISO_ALLOCATED(SVGGeometryElement);
public:
    
    virtual float getTotalLength() const;
    virtual Ref<SVGPoint> getPointAtLength(float distance) const;

    bool isPointInFill(DOMPointInit&&);
    bool isPointInStroke(DOMPointInit&&);

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGGeometryElement, SVGGraphicsElement>;
    static auto& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }

    auto pathLengthAnimated() { return m_pathLength.animatedProperty(attributeOwnerProxy()); }

protected:
    SVGGeometryElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

private:
    bool isSVGGeometryElement() const override { return true; }
    const SVGAttributeOwnerProxy& attributeOwnerProxy() const override { return m_attributeOwnerProxy; }

    static void registerAttributes();
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedNumberAttribute m_pathLength;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGGeometryElement)
    static bool isType(const WebCore::SVGElement& element) { return element.isSVGGeometryElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::SVGElement>(node) && isType(downcast<WebCore::SVGElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
