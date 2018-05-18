/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Adobe Systems Incorporated. All rights reserved.
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
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedNumber.h"
#include "SVGExternalResourcesRequired.h"
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

protected:
    SVGGeometryElement(const QualifiedName&, Document&);

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGGeometryElement)
        DECLARE_ANIMATED_NUMBER(PathLength, pathLength)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore
