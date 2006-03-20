/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2006       Alexander Kellett <lypanov@kde.org>

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
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_KCanvasRenderingStyle_H
#define KSVG_KCanvasRenderingStyle_H
#if SVG_SUPPORT

#include "css_valueimpl.h"
#include <kcanvas/KCanvasMatrix.h>
#include <qvaluelist.h>

namespace WebCore {

// FIXME: these should be removed, use KSVG ones instead
enum KCCapStyle {
    CAP_BUTT = 1,
    CAP_ROUND = 2,
    CAP_SQUARE = 3
};

enum KCJoinStyle {
    JOIN_MITER = 1,
    JOIN_ROUND = 2,
    JOIN_BEVEL = 3
};


// Special types
typedef Q3ValueList<float> KCDashArray;

    class KRenderingFillPainter;
    class KRenderingStrokePainter;
    class KRenderingPaintServer;
    class RenderStyle;
    class RenderObject;
    class KSVGPainterFactory
    {
    public:
        static KRenderingFillPainter fillPainter(const RenderStyle*, const RenderObject*);
        static KRenderingStrokePainter strokePainter(const RenderStyle*, const RenderObject*);

        static bool isStroked(const RenderStyle*);
        static KRenderingPaintServer* strokePaintServer(const RenderStyle*, const RenderObject*);

        static bool isFilled(const RenderStyle*);
        static KRenderingPaintServer* fillPaintServer(const RenderStyle*, const RenderObject*);

        static double cssPrimitiveToLength(const RenderObject*, CSSValue*, double defaultValue = 0.0);
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
