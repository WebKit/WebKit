/*
    Copyright (C) 2006 Nikolas Zimmermann <wildfox@kde.org>
    Copyright (C) 2008 Holger Hans Peter Freyther

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

#include "GraphicsContext.h"
#include "RenderPath.h"

#include <QPainter>

namespace WebCore {

bool SVGPaintServerSolid::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    QPainter* painter(context ? context->platformContext() : 0);
    Q_ASSERT(painter);

    const SVGRenderStyle* svgStyle = object->style()->svgStyle();
    RenderStyle* style = object ? object->style() : 0;
    // TODO? painter->setOpacity(renderStyle->opacity());

    QColor c = color();

    if ((type & ApplyToFillTargetType) && (!style || svgStyle->hasFill())) {
        if (style)
            c.setAlphaF(svgStyle->fillOpacity());

        QBrush brush(c);
        painter->setBrush(brush);

        if (style)
            context->setFillRule(svgStyle->fillRule());

        /* if(isPaintingText()) ... */
    }

    if ((type & ApplyToStrokeTargetType) && (!style || svgStyle->hasStroke())) {
        if (style)
            c.setAlphaF(svgStyle->strokeOpacity());

        QPen pen(c);
        painter->setPen(pen);
        if (style)
            applyStrokeStyleToContext(context, style, object);

        /* if(isPaintingText()) ... */
    }

    return true;
}

} // namespace WebCore

#endif

// vim:ts=4:noet
