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

#ifndef KSVG_Ecma_H
#define KSVG_Ecma_H
#if SVG_SUPPORT

#include <kdom/ecma/Ecma.h>

namespace WebCore
{
    class Node;
    class Document;
};

namespace WebCore
{
    class Ecma : public Ecma
    {
    public:
        Ecma(Document *doc);
        virtual ~Ecma();

        // We are a KDOM user, so implement the hook to convert svg elements to kjs objects
        virtual KJS::JSObject *inheritedGetDOMNode(KJS::ExecState *exec, Node n);
        virtual KJS::JSObject *inheritedGetDOMEvent(KJS::ExecState *exec, Event e);
        virtual KJS::JSObject *inheritedGetDOMCSSValue(KJS::ExecState *exec, CSSValue c);

    protected:
        virtual void setupDocument(Document *document);
    };

    // Helpers
    class SVGPathSeg;
    KJS::JSValue *getSVGPathSeg(KJS::ExecState *exec, SVGPathSeg s);
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
