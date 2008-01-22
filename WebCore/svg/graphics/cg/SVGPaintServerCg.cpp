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
#include "SVGPaintServer.h"

#include "GraphicsContext.h"
#include "RenderObject.h"

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
    RenderStyle* style = path ? path->style() : 0;
    CGContextRef contextRef = context->platformContext();

    if ((type & ApplyToFillTargetType) && (!style || style->svgStyle()->hasFill()))
        fillPath(contextRef, path);

    if ((type & ApplyToStrokeTargetType) && (!style || style->svgStyle()->hasStroke()))
        strokePath(contextRef, path);
}

void SVGPaintServer::strokePath(CGContextRef context, const RenderObject*) const
{
    CGContextStrokePath(context);
}

void SVGPaintServer::clipToStrokePath(CGContextRef context, const RenderObject*) const
{
    CGContextReplacePathWithStrokedPath(context);
    CGContextClip(context);
}

void SVGPaintServer::fillPath(CGContextRef context, const RenderObject* path) const
{
    if (!path || path->style()->svgStyle()->fillRule() == RULE_EVENODD)
        CGContextEOFillPath(context);
    else
        CGContextFillPath(context);
}

void SVGPaintServer::clipToFillPath(CGContextRef context, const RenderObject* path) const
{
    if (!path || path->style()->svgStyle()->fillRule() == RULE_EVENODD)
        CGContextEOClip(context);
    else
        CGContextClip(context);
}

} // namespace WebCore

#endif

// vim:ts=4:noet
