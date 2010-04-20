/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGFilterPrimitiveStandardAttributes_h
#define SVGFilterPrimitiveStandardAttributes_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFilterBuilder.h"
#include "SVGNames.h"
#include "SVGResourceFilter.h"
#include "SVGStyledElement.h"

namespace WebCore {

    class SVGResourceFilter;

    class SVGFilterPrimitiveStandardAttributes : public SVGStyledElement {
    public:
        SVGFilterPrimitiveStandardAttributes(const QualifiedName&, Document*);
        virtual ~SVGFilterPrimitiveStandardAttributes();
        
        virtual bool isFilterEffect() const { return true; }

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void synchronizeProperty(const QualifiedName&);
        virtual bool build(SVGResourceFilter*) = 0;

        virtual bool rendererIsNeeded(RenderStyle*) { return false; }

    protected:
        friend class SVGResourceFilter;
        void setStandardAttributes(SVGResourceFilter*, FilterEffect*) const;

    private:
        DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::xAttr, SVGLength, X, x)
        DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::yAttr, SVGLength, Y, y)
        DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::widthAttr, SVGLength, Width, width)
        DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::heightAttr, SVGLength, Height, height)
        DECLARE_ANIMATED_PROPERTY(SVGFilterPrimitiveStandardAttributes, SVGNames::resultAttr, String, Result, result)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
