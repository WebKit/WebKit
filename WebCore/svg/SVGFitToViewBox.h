/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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
#include "SVGPreserveAspectRatio.h"

namespace WebCore {

    extern char SVGFitToViewBoxIdentifier[];

    class TransformationMatrix;

    class SVGFitToViewBox {
    public:
        SVGFitToViewBox();
        virtual ~SVGFitToViewBox();

        bool parseViewBox(Document*, const UChar*& start, const UChar* end, float& x, float& y, float& w, float& h, bool validate = true);
        static TransformationMatrix viewBoxToViewTransform(const FloatRect& viewBoxRect, SVGPreserveAspectRatio*, float viewWidth, float viewHeight);

        bool parseMappedAttribute(Document*, MappedAttribute*);
        bool isKnownAttribute(const QualifiedName&);

    protected:
        virtual SVGAnimatedTypeValue<FloatRect>::DecoratedType viewBoxBaseValue() const = 0;
        virtual void setViewBoxBaseValue(SVGAnimatedTypeValue<FloatRect>::DecoratedType type) = 0;

        virtual SVGAnimatedTypeValue<SVGPreserveAspectRatio>::DecoratedType preserveAspectRatioBaseValue() const = 0;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGFitToViewBox_h
