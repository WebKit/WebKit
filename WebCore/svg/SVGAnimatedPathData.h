/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#ifndef SVGAnimatedPathData_h
#define SVGAnimatedPathData_h

#if ENABLE(SVG)

namespace WebCore {
    class SVGPathSegList;

    class SVGAnimatedPathData {
    public:
        SVGAnimatedPathData();
        virtual ~SVGAnimatedPathData();

        // 'SVGAnimatedPathData' functions
        virtual SVGPathSegList* pathSegList() const = 0;
        virtual SVGPathSegList* normalizedPathSegList() const = 0;
        virtual SVGPathSegList* animatedPathSegList() const = 0;
        virtual SVGPathSegList* animatedNormalizedPathSegList() const = 0;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
