/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>
    
    This file is part of the KDE project

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

#ifndef SVGAnimateTransformElement_h
#define SVGAnimateTransformElement_h
#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)

#include "SVGAnimationElement.h"
#include "SVGTransform.h"
#include "SVGTransformDistance.h"

namespace WebCore {

    class AffineTransform;

    class SVGAnimateTransformElement : public SVGAnimationElement {
    public:
        SVGAnimateTransformElement(const QualifiedName&, Document*);
        virtual ~SVGAnimateTransformElement();
        
        virtual bool hasValidTarget() const;

        virtual void parseMappedAttribute(MappedAttribute*);
        
        virtual bool updateAnimationBaseValueFromElement();
        virtual void applyAnimatedValueToElement();

    protected:
        virtual const SVGElement* contextElement() const { return this; }
        
        virtual bool updateAnimatedValue(EAnimationMode, float timePercentage, unsigned valueIndex, float percentagePast);
        virtual bool calculateFromAndToValues(EAnimationMode, unsigned valueIndex);

    private:
        SVGTransform parseTransformValue(const String&) const;
        void calculateRotationFromMatrix(const AffineTransform&, double& angle, double& cx, double& cy) const;
        
        SVGTransform::SVGTransformType m_type;

        SVGTransform m_toTransform;
        SVGTransform m_fromTransform;

        SVGTransform m_baseTransform;
        SVGTransform m_animatedTransform;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimateTransformElement_h

// vim:ts=4:noet
