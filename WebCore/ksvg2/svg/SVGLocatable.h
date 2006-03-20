/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#ifndef KSVG_SVGLocatableImpl_H
#define KSVG_SVGLocatableImpl_H
#if SVG_SUPPORT

namespace WebCore
{
    class SVGRect;
    class SVGMatrix;
    class SVGElement;
    class SVGStyledElement;
    class SVGLocatable
    {
    public:
        SVGLocatable();
        virtual ~SVGLocatable();

        // 'SVGLocatable' functions
        virtual SVGElement *nearestViewportElement() const = 0;
        virtual SVGElement *farthestViewportElement() const = 0;

        virtual SVGRect *getBBox() const = 0;
        virtual SVGMatrix *getCTM() const = 0;
        virtual SVGMatrix *getScreenCTM() const = 0;
        virtual SVGMatrix *getTransformToElement(SVGElement *element) const = 0;

    protected:
        static SVGElement *nearestViewportElement(const SVGStyledElement *element);
        static SVGElement *farthestViewportElement(const SVGStyledElement *element);
        static SVGRect *getBBox(const SVGStyledElement *element);
        static SVGMatrix *getCTM(const SVGElement *element);
        static SVGMatrix *getScreenCTM(const SVGElement *element);
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
