/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>

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

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServerSolid.h"

#include "Color.h"
#include "CgSupport.h"
#include "GraphicsContext.h"
#include "RenderObject.h"

namespace WebCore {

bool SVGPaintServerSolid::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    // FIXME: This function does not use any CG-specific calls, however it's not yet
    // cross platform, because CG handles fill rule different from other graphics
    // platforms.  CG makes you use two separate fill calls, other platforms set
    // the fill rule state on the context and then call a generic "fillPath"

    RenderStyle* style = object ? object->style() : 0;

    if ((type & ApplyToFillTargetType) && (!style || style->svgStyle()->hasFill())) {
        RGBA32 rgba = color().rgb();
        ASSERT(!color().hasAlpha());
        if (style)
            rgba = colorWithOverrideAlpha(rgba, style->svgStyle()->fillOpacity());

        context->setFillColor(rgba);

        if (isPaintingText)
            context->setTextDrawingMode(cTextFill);
    }

    if ((type & ApplyToStrokeTargetType) && (!style || style->svgStyle()->hasStroke())) {
        RGBA32 rgba = color().rgb();
        ASSERT(!color().hasAlpha());
        if (style)
            rgba = colorWithOverrideAlpha(rgba, style->svgStyle()->strokeOpacity());

        context->setStrokeColor(rgba);

        if (style)
            applyStrokeStyleToContext(context, style, object);

        if (isPaintingText)
            context->setTextDrawingMode(cTextStroke);
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
