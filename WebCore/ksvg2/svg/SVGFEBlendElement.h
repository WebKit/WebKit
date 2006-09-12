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

#ifndef KSVG_SVGFEBlendElementImpl_H
#define KSVG_SVGFEBlendElementImpl_H
#ifdef SVG_SUPPORT

#include "SVGFilterPrimitiveStandardAttributes.h"
#include "KCanvasFilters.h"

namespace WebCore
{

    class SVGFEBlendElement : public SVGFilterPrimitiveStandardAttributes
    {
    public:
        enum SVGBlendModeType {
            SVG_FEBLEND_MODE_UNKNOWN  = 0,
            SVG_FEBLEND_MODE_NORMAL   = 1,
            SVG_FEBLEND_MODE_MULTIPLY = 2,
            SVG_FEBLEND_MODE_SCREEN   = 3,
            SVG_FEBLEND_MODE_DARKEN   = 4,
            SVG_FEBLEND_MODE_LIGHTEN  = 5
        };

        SVGFEBlendElement(const QualifiedName&, Document*);
        virtual ~SVGFEBlendElement();

        // 'SVGFEBlendElement' functions
        // Derived from: 'Element'
        virtual void parseMappedAttribute(MappedAttribute *attr);

        virtual KCanvasFEBlend *filterEffect() const;

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEBlendElement, String, String, In1, in1)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEBlendElement, String, String, In2, in2)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFEBlendElement, int, int, Mode, mode)
        mutable KCanvasFEBlend *m_filterEffect;
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif

