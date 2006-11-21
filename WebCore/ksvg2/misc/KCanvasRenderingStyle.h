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

#ifndef KCanvasRenderingStyle_H
#define KCanvasRenderingStyle_H
#ifdef SVG_SUPPORT

#include <wtf/Vector.h>

#if PLATFORM(CG)
#include "QuartzSupport.h"
#endif

namespace WebCore {

// Special types
#if PLATFORM(CG)
typedef Vector<CGFloat> KCDashArray;
#else
typedef Vector<float> KCDashArray;
#endif

    class CSSValue;
    class SVGPaintServer;
    class RenderStyle;
    class RenderObject;

    class KSVGPainterFactory {
    public:
        static SVGPaintServer* strokePaintServer(const RenderStyle*, const RenderObject*);
        static SVGPaintServer* fillPaintServer(const RenderStyle*, const RenderObject*);

        // Helpers
        static double cssPrimitiveToLength(const RenderObject*, CSSValue*, double defaultValue = 0.0);
        static KCDashArray dashArrayFromRenderingStyle(const RenderStyle*);
    };

} // namespace WebCore

#endif // SVG_SUPPORT
#endif // KCanvasRenderingStyle_H

// vim:ts=4:noet
