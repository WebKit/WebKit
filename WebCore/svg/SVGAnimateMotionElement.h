/*
 Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 
 This file is part of the WebKit project
 
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

#ifndef SVGAnimateMotionElement_h
#define SVGAnimateMotionElement_h
#if ENABLE(SVG) && ENABLE(SVG_ANIMATION)

#include "SVGAnimationElement.h"
#include "AffineTransform.h"
#include "Path.h"

namespace WebCore {
            
    class SVGAnimateMotionElement : public SVGAnimationElement {
    public:
        SVGAnimateMotionElement(const QualifiedName&, Document*);
        virtual ~SVGAnimateMotionElement();
        
        virtual bool hasValidTarget() const;
        
        virtual bool updateAnimationBaseValueFromElement();
        virtual void applyAnimatedValueToElement();
        
        virtual void parseMappedAttribute(MappedAttribute*);
                
        Path animationPath();
        
    protected:
        virtual const SVGElement* contextElement() const { return this; }
        
        virtual bool updateAnimatedValue(EAnimationMode, float timePercentage, unsigned valueIndex, float percentagePast);
        virtual bool calculateFromAndToValues(EAnimationMode, unsigned valueIndex);
        
    private:
        FloatPoint m_basePoint;
        FloatSize m_animatedTranslation;
        float m_animatedAngle;
        
        // Note: we do not support percentage values for to/from coords as the spec implies we should (opera doesn't either)
        FloatPoint m_fromPoint;
        float m_fromAngle;
        FloatPoint m_toPoint;
        float m_toAngle;
        
        FloatSize m_pointDiff;
        float m_angleDiff;
        
        Path m_path;
        Vector<float> m_keyPoints;
        enum RotateMode {
            AngleMode,
            AutoMode,
            AutoReverseMode
        };
        RotateMode m_rotateMode;
        float m_angle;
    };
    
} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGAnimateMotionElement_h

// vim:ts=4:noet
