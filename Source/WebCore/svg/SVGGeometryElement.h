/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGGeometryElement, SVGGraphicsElement>;

    float pathLength() const { return m_pathLength->currentValue(); }
    SVGAnimatedNumber& pathLengthAnimated() { return m_pathLength; }

protected:
    SVGGeometryElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

private:
    bool isSVGGeometryElement() const override { return true; }

    Ref<SVGAnimatedNumber> m_pathLength { SVGAnimatedNumber::create(this) };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGGeometryElement)
    static bool isType(const WebCore::SVGElement& element) { return element.isSVGGeometryElement(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::SVGElement>(node) && isType(downcast<WebCore::SVGElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
