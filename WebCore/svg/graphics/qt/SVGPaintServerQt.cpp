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
#include "SVGPaintServer.h"

#include "GraphicsContext.h"
#include "SVGRenderStyle.h"
#include "RenderObject.h"

#include <QPainter>
#include <QVector>

namespace WebCore {

void SVGPaintServer::draw(GraphicsContext*& context, const RenderObject* path, SVGPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

void SVGPaintServer::teardown(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const
{
    // no-op
}

void SVGPaintServer::renderPath(GraphicsContext*& context, const RenderObject* path, SVGPaintTargetType type) const
{
    RenderStyle* renderStyle = path ? path->style(): 0;

    QPainter* painter(context ? context->platformContext() : 0);
    Q_ASSERT(painter);

    QPainterPath* painterPath(context ? context->currentPath() : 0);
    Q_ASSERT(painterPath);

    if ((type & ApplyToFillTargetType) && (!renderStyle || renderStyle->svgStyle()->hasFill()))
        painter->fillPath(*painterPath, painter->brush());

    if ((type & ApplyToStrokeTargetType) && (!renderStyle || renderStyle->svgStyle()->hasStroke()))
        painter->strokePath(*painterPath, painter->pen());
}

} // namespace WebCore

#endif

// vim:ts=4:noet
