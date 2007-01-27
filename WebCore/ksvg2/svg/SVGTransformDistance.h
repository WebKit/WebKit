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
 the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 Boston, MA 02111-1307, USA.
 */

#ifndef SVGTransformDistance_h
#define SVGTransformDistance_h
#ifdef SVG_SUPPORT

#include "SVGTransform.h"

namespace WebCore {
    
    class AffineTransform;
        
    class SVGTransformDistance {
    public:
        SVGTransformDistance();
        SVGTransformDistance(const SVGTransform& fromTransform, const SVGTransform& toTransform);
        
        SVGTransformDistance scaledDistance(float scaleFactor) const;
        SVGTransform addToSVGTransform(const SVGTransform&) const;
        void addSVGTransform(const SVGTransform&, bool absoluteValue = false);
        
        static SVGTransform addSVGTransforms(const SVGTransform&, const SVGTransform&);
        
        bool isZero() const;
        
        float distance() const;
    private:
        SVGTransformDistance(SVGTransform::SVGTransformType, double angle, const AffineTransform&);
            
        SVGTransform::SVGTransformType m_type;
        double m_angle;
        AffineTransform m_transform; // for storing scale, translation or matrix transforms
    };
}

#endif // SVG_SUPPORT
#endif // SVGTransformDistance_h
