/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGFitToViewBox_h
#define SVGFitToViewBox_h
#if ENABLE(SVG)

#include "SVGElement.h"

namespace WebCore {
    class AffineTransform;
    class SVGPreserveAspectRatio;

    class SVGFitToViewBox {
    public:
        SVGFitToViewBox();
        virtual ~SVGFitToViewBox();

        // 'SVGFitToViewBox' functions
        void parseViewBox(const String&);
        AffineTransform viewBoxToViewTransform(float viewWidth, float viewHeight) const;

        bool parseMappedAttribute(MappedAttribute*);

    protected:
        virtual const SVGElement* contextElement() const = 0;

    private:
        ANIMATED_PROPERTY_DECLARATIONS_WITH_CONTEXT(SVGFitToViewBox, FloatRect, FloatRect, ViewBox, viewBox)
        ANIMATED_PROPERTY_DECLARATIONS_WITH_CONTEXT(SVGFitToViewBox, SVGPreserveAspectRatio*, RefPtr<SVGPreserveAspectRatio>, PreserveAspectRatio, preserveAspectRatio)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGFitToViewBox_h

// vim:ts=4:noet
