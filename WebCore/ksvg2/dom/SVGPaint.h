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

#ifndef KSVG_SVGPaint_H
#define KSVG_SVGPaint_H

#include <kdom/DOMString.h>

#include <ksvg2/dom/SVGColor.h>
#include <ksvg2/ecma/SVGLookup.h>

class KCanvasItem;

namespace KSVG
{
    class SVGPaintImpl;
    class SVGPaint : public SVGColor
    {
    public:
        SVGPaint();
        explicit SVGPaint(SVGPaintImpl *i);
        SVGPaint(const SVGPaint &other);
        SVGPaint(const KDOM::CSSValue &other);
        virtual ~SVGPaint();

        // Operators
        SVGPaint &operator=(const SVGPaint &other);
        SVGPaint &operator=(const KDOM::CSSValue &other);

        // 'SVGPaint' functions
        unsigned short paintType() const;
        KDOM::DOMString uri() const;

        void setUri(const KDOM::DOMString &uri);
        void setPaint(unsigned short paintType, const KDOM::DOMString &uri, const KDOM::DOMString &rgbPaint, const KDOM::DOMString &iccPaint);

        // Internal
        KSVG_INTERNAL(SVGPaint)

    public: // EcmaScript section
        KDOM_GET
        KDOM_FORWARDPUT

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
    };
};

KSVG_DEFINE_PROTOTYPE(SVGPaintProto)
KSVG_IMPLEMENT_PROTOFUNC(SVGPaintProtoFunc, SVGPaint)

#endif

// vim:ts=4:noet
