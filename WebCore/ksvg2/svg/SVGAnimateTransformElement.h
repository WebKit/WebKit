/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>
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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef SVGAnimateTransformElement_H
#define SVGAnimateTransformElement_H
#ifdef SVG_SUPPORT

#include "SVGAnimationElement.h"
#include "SVGTransform.h"

namespace WebCore {

    class AffineTransform;

    class SVGAnimateTransformElement : public SVGAnimationElement {
    public:
        SVGAnimateTransformElement(const QualifiedName&, Document*);
        virtual ~SVGAnimateTransformElement();
        
        virtual bool hasValidTarget() const;

        virtual void parseMappedAttribute(MappedAttribute*);

        void applyAnimationToValue(SVGTransformList*);

        // Helpers
        RefPtr<SVGTransform> parseTransformValue(const String&) const;
        void calculateRotationFromMatrix(const AffineTransform&, double& angle, double& cx, double& cy) const;

        AffineTransform currentTransform() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }
        void storeInitialValue();
        virtual void resetValues();
        
        virtual bool updateCurrentValue(double timePercentage);
        virtual bool handleStartCondition();
        virtual void updateLastValueWithCurrent();

    private:
        int m_currentItem;
        SVGTransform::SVGTransformType m_type;

        RefPtr<SVGTransform> m_toTransform;
        RefPtr<SVGTransform> m_fromTransform;
        RefPtr<SVGTransform> m_initialTransform;

        AffineTransform m_lastTransform;
        AffineTransform m_currentTransform;

        mutable bool m_rotateSpecialCase : 1;
        bool m_toRotateSpecialCase : 1;
        bool m_fromRotateSpecialCase : 1;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // SVGAnimateTransformElement_H

// vim:ts=4:noet
