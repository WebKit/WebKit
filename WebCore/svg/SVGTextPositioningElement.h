/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
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

#ifndef SVGTextPositioningElement_h
#define SVGTextPositioningElement_h

#if ENABLE(SVG)
#include "SVGTextContentElement.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"

namespace WebCore {

    class SVGTextPositioningElement : public SVGTextContentElement {
    public:
        static SVGTextPositioningElement* elementFromRenderer(RenderObject*);

    protected:
        SVGTextPositioningElement(const QualifiedName&, Document*);

        virtual void parseMappedAttribute(Attribute*);
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);

        virtual bool selfHasRelativeLengths() const;

        DECLARE_ANIMATED_LIST_PROPERTY_NEW(SVGTextPositioningElement, SVGNames::xAttr, SVGLengthList, X, x)
        DECLARE_ANIMATED_LIST_PROPERTY_NEW(SVGTextPositioningElement, SVGNames::yAttr, SVGLengthList, Y, y)
        DECLARE_ANIMATED_LIST_PROPERTY_NEW(SVGTextPositioningElement, SVGNames::dxAttr, SVGLengthList, Dx, dx)
        DECLARE_ANIMATED_LIST_PROPERTY_NEW(SVGTextPositioningElement, SVGNames::dyAttr, SVGLengthList, Dy, dy)
        DECLARE_ANIMATED_LIST_PROPERTY_NEW(SVGTextPositioningElement, SVGNames::rotateAttr, SVGNumberList, Rotate, rotate)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
