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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"

#ifdef SVG_SUPPORT
#include "SVGPaintServer.h"

#include "KRenderingDeviceQuartz.h"
#include "RenderPath.h"

namespace WebCore {

void SVGPaintServer::draw(KRenderingDeviceContext* context, const RenderPath* path, SVGPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

void SVGPaintServer::teardown(KRenderingDeviceContext*, const RenderObject*, SVGPaintTargetType) const
{
    // no-op
}

void SVGPaintServer::renderPath(KRenderingDeviceContext* context, const RenderPath* path, SVGPaintTargetType type) const
{
    RenderStyle* style = path->style();

    KRenderingDeviceContextQuartz* quartzContext = static_cast<KRenderingDeviceContextQuartz*>(context);
    CGContextRef contextRef = quartzContext->cgContext();

    if ((type & APPLY_TO_FILL) && style->svgStyle()->hasFill())
        fillPath(contextRef, path);

    if ((type & APPLY_TO_STROKE) && style->svgStyle()->hasStroke())
        strokePath(contextRef, path);
}

void SVGPaintServer::strokePath(CGContextRef context, const RenderPath*) const
{
    CGContextStrokePath(context);
}

void SVGPaintServer::clipToStrokePath(CGContextRef context, const RenderPath*) const
{
    CGContextReplacePathWithStrokedPath(context);
    CGContextClip(context);
}

void SVGPaintServer::fillPath(CGContextRef context, const RenderPath* path) const
{
    if (path->style()->svgStyle()->fillRule() == RULE_EVENODD)
        CGContextEOFillPath(context);
    else
        CGContextFillPath(context);
}

void SVGPaintServer::clipToFillPath(CGContextRef context, const RenderPath* path) const
{
    if (path->style()->svgStyle()->fillRule() == RULE_EVENODD)
        CGContextEOClip(context);
    else
        CGContextClip(context);
}

} // namespace WebCore

#endif

// vim:ts=4:noet
