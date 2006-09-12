/*
 Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 
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
 the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 Boston, MA 02111-1307, USA.
 */

#ifndef KSVG_SVGFEDisplacementMapElementImpl_H
#define KSVG_SVGFEDisplacementMapElementImpl_H
#ifdef SVG_SUPPORT

#include "SVGFilterPrimitiveStandardAttributes.h"
#include "KCanvasFilters.h"

namespace WebCore {
    
    class SVGFEDisplacementMapElement : public SVGFilterPrimitiveStandardAttributes {
    public:
        enum SVGChannelSelectors {
            SVG_CHANNEL_UNKNOWN = 0,
            SVG_CHANNEL_R       = 1,
            SVG_CHANNEL_G       = 2,
            SVG_CHANNEL_B       = 3,
            SVG_CHANNEL_A       = 4
        };

        SVGFEDisplacementMapElement(const QualifiedName& tagName, Document*);
        virtual ~SVGFEDisplacementMapElement();
        
        // 'SVGFEDisplacementMapElement' functions
        static KCChannelSelectorType stringToChannel(const String&);
        
        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute*);
        
        virtual KCanvasFEDisplacementMap* filterEffect() const;
        
    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, String, String, In2, in2)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, int, int, XChannelSelector, xChannelSelector)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, int, int, YChannelSelector, yChannelSelector)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEDisplacementMapElement, double, double, Scale, scale)
        mutable KCanvasFEDisplacementMap* m_filterEffect;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KSVG_SVGFEDisplacementMapElementImpl_H
