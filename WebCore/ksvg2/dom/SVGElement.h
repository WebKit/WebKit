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

#ifndef KSVG_SVGElement_H
#define KSVG_SVGElement_H

#include <kdom/Element.h>
#include <ksvg2/ecma/SVGLookup.h>

namespace KSVG
{
    class SVGSVGElement;
    class SVGElementImpl;
    class SVGElement : public KDOM::Element
    {
    public:
        SVGElement();
        explicit SVGElement(SVGElementImpl *i);
        SVGElement(const SVGElement &other);
        SVGElement(const KDOM::Node &other);
        virtual ~SVGElement();

        // Operators
        SVGElement &operator=(const SVGElement &other);
        SVGElement &operator=(const KDOM::Node &other);

        // 'SVGElement' functions
        KDOM::DOMString id() const;
        KDOM::DOMString xmlbase() const;

        SVGSVGElement ownerSVGElement() const;
        SVGElement viewportElement() const;

        // Internal
        KSVG_INTERNAL(SVGElement)

    public: // EcmaScript section
        KDOM_GET
        KDOM_PUT
        KDOM_CAST

        KJS::ValueImp *getValueProperty(KJS::ExecState *exec, int token) const;
        void putValueProperty(KJS::ExecState *exec, int token, KJS::ValueImp *value, int attr);
    };

    KDOM_DEFINE_CAST(SVGElement)
};

#endif

// vim:ts=4:noet
