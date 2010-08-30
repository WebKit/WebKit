/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGFilterPrimitiveStandardAttributes_h
#define SVGFilterPrimitiveStandardAttributes_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "FilterEffect.h"
#include "SVGFilterBuilder.h"
#include "SVGFilterElement.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class SVGFilterPrimitiveStandardAttributes : public SVGStyledElement {
public:
    void setStandardAttributes(bool, FilterEffect*) const;

    virtual PassRefPtr<FilterEffect> build(SVGFilterBuilder*) = 0;

protected:
    SVGFilterPrimitiveStandardAttributes(const QualifiedName&, Document*);

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

private:
    virtual bool isFilterEffect() const { return true; }

    virtual bool rendererIsNeeded(RenderStyle*) { return false; }

    DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::xAttr, SVGLength, X, x)
    DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::yAttr, SVGLength, Y, y)
    DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::widthAttr, SVGLength, Width, width)
    DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::heightAttr, SVGLength, Height, height)
    DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::resultAttr, String, Result, result)
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
