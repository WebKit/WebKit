/*
 Copyright (C) 2006 Oliver Hunt <oliver@nerget.com>
 
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

#ifndef SVGFEDisplacementMapElement_h
#define SVGFEDisplacementMapElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGFEDisplacementMap.h"
#include "SVGFilterPrimitiveStandardAttributes.h"

namespace WebCore {
    
    class SVGFEDisplacementMapElement : public SVGFilterPrimitiveStandardAttributes {
    public:
        SVGFEDisplacementMapElement(const QualifiedName& tagName, Document*);
        virtual ~SVGFEDisplacementMapElement();
        
        static ChannelSelectorType stringToChannel(const String&);
        
        virtual void parseMappedAttribute(MappedAttribute*);
        virtual bool build(SVGResourceFilter*);
        
    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, SVGNames::feDisplacementMapTagString, SVGNames::inAttrString, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, SVGNames::feDisplacementMapTagString, SVGNames::in2AttrString, String, In2, in2)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, SVGNames::feDisplacementMapTagString, SVGNames::xChannelSelectorAttrString, int, XChannelSelector, xChannelSelector)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, SVGNames::feDisplacementMapTagString, SVGNames::yChannelSelectorAttrString, int, YChannelSelector, yChannelSelector)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, SVGNames::feDisplacementMapTagString, SVGNames::scaleAttrString, float, Scale, scale)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGFEDisplacementMapElement_h
