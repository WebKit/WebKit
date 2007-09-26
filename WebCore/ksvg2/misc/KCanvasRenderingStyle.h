/*
    Copyright (C) 2004, 2005, 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KCanvasRenderingStyle_h
#define KCanvasRenderingStyle_h

#if ENABLE(SVG)

#include <wtf/Vector.h>

#if PLATFORM(CG)
#include "CgSupport.h"
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

#endif // ENABLE(SVG)
#endif // KCanvasRenderingStyle_h

// vim:ts=4:noet
