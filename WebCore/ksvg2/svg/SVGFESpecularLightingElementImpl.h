/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Oliver Hunt <ojh16@student.canterbury.ac.nz>

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

#ifndef KSVG_SVGFESpecularLightingElementImpl_H
#define KSVG_SVGFESpecularLightingElementImpl_H

#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "KCanvasFilters.h"

namespace KSVG
{
    class SVGAnimatedNumberImpl;
    class SVGAnimatedStringImpl;
    class SVGAnimatedColorImpl;
    
    class SVGFESpecularLightingElementImpl : public SVGFilterPrimitiveStandardAttributesImpl
    {
    public:
        SVGFESpecularLightingElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc);
        virtual ~SVGFESpecularLightingElementImpl();
        
        // 'SVGFEDiffuseLightingElement' functions
        SVGAnimatedStringImpl *in1() const;
        SVGAnimatedNumberImpl *specularConstant() const;
        SVGAnimatedNumberImpl *specularExponent() const;
        SVGAnimatedNumberImpl *surfaceScale() const;
        SVGAnimatedNumberImpl *kernelUnitLengthX() const;
        SVGAnimatedNumberImpl *kernelUnitLengthY() const;
        SVGAnimatedColorImpl  *lightingColor() const;
        
        // Derived from: 'ElementImpl'
        virtual void parseMappedAttribute(KDOM::MappedAttributeImpl *attr);
        
        virtual KCanvasFESpecularLighting *filterEffect() const;
    private:
        mutable RefPtr<SVGAnimatedStringImpl> m_in1;
        mutable RefPtr<SVGAnimatedNumberImpl> m_specularConstant;
        mutable RefPtr<SVGAnimatedNumberImpl> m_specularExponent;
        mutable RefPtr<SVGAnimatedNumberImpl> m_surfaceScale;
        mutable RefPtr<SVGAnimatedColorImpl>  m_lightingColor;
        mutable RefPtr<SVGAnimatedNumberImpl> m_kernelUnitLengthX;
        mutable RefPtr<SVGAnimatedNumberImpl> m_kernelUnitLengthY;
        //need other properties here...
        mutable KCanvasFESpecularLighting *m_filterEffect;
        
        //light management
        void updateLights() const;
    };
};

#endif
