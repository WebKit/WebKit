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
        virtual void synchronizeProperty(const QualifiedName&);
        virtual bool build(SVGResourceFilter*);
        
    private:
        DECLARE_ANIMATED_PROPERTY(SVGFEDisplacementMapElement, SVGNames::inAttr, String, In1, in1)
        DECLARE_ANIMATED_PROPERTY(SVGFEDisplacementMapElement, SVGNames::in2Attr, String, In2, in2)
        DECLARE_ANIMATED_PROPERTY(SVGFEDisplacementMapElement, SVGNames::xChannelSelectorAttr, int, XChannelSelector, xChannelSelector)
        DECLARE_ANIMATED_PROPERTY(SVGFEDisplacementMapElement, SVGNames::yChannelSelectorAttr, int, YChannelSelector, yChannelSelector)
        DECLARE_ANIMATED_PROPERTY(SVGFEDisplacementMapElement, SVGNames::scaleAttr, float, Scale, scale)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGFEDisplacementMapElement_h
