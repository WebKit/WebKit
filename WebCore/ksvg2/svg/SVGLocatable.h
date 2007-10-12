/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGLocatable_h
#define SVGLocatable_h

#if ENABLE(SVG)

#include "ExceptionCode.h"

namespace WebCore {

    class AffineTransform;
    class FloatRect;
    class SVGElement;
    class SVGStyledElement;

    class SVGLocatable {
    public:
        SVGLocatable();
        virtual ~SVGLocatable();

        // 'SVGLocatable' functions
        virtual SVGElement* nearestViewportElement() const = 0;
        virtual SVGElement* farthestViewportElement() const = 0;

        virtual FloatRect getBBox() const = 0;
        virtual AffineTransform getCTM() const = 0;
        virtual AffineTransform getScreenCTM() const = 0;
        AffineTransform getTransformToElement(SVGElement*, ExceptionCode&) const;

        static SVGElement* nearestViewportElement(const SVGStyledElement*);
        static SVGElement* farthestViewportElement(const SVGStyledElement*);

    protected:
        static FloatRect getBBox(const SVGStyledElement*);
        static AffineTransform getCTM(const SVGElement*);
        static AffineTransform getScreenCTM(const SVGElement*);
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGLocatable_h

// vim:ts=4:noet
