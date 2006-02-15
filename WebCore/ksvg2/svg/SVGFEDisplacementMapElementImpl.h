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
#if SVG_SUPPORT

#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedNumberImpl;
    class SVGAnimatedStringImpl;
    class SVGAnimatedEnumerationImpl;
    
    class SVGFEDisplacementMapElementImpl : public SVGFilterPrimitiveStandardAttributesImpl
    {
public:
        SVGFEDisplacementMapElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFEDisplacementMapElementImpl();
        
        // 'SVGFEDisplacementMapElement' functions
        SVGAnimatedStringImpl* in1() const;
        SVGAnimatedStringImpl* in2() const;
        SVGAnimatedEnumerationImpl* xChannelSelector() const;
        SVGAnimatedEnumerationImpl* yChannelSelector() const;
        SVGAnimatedNumberImpl* scale() const;
        
        static KCChannelSelectorType stringToChannel(KDOM::DOMString &key);
        
        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);
        
        virtual KCanvasFEDisplacementMap* filterEffect() const;
        
private:
        mutable RefPtr<SVGAnimatedStringImpl> m_in1;
        mutable RefPtr<SVGAnimatedStringImpl> m_in2;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_xChannelSelector;
        mutable RefPtr<SVGAnimatedEnumerationImpl> m_yChannelSelector;
        mutable RefPtr<SVGAnimatedNumberImpl> m_scale;
        mutable KCanvasFEDisplacementMap *m_filterEffect;
    };
};

#endif // SVG_SUPPORT
#endif // KSVG_SVGFEDisplacementMapElementImpl_H

