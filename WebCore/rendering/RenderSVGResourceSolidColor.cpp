/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#if ENABLE(SVG)
#include "RenderSVGResourceSolidColor.h"

#include "GraphicsContext.h"
#include "SVGRenderSupport.h"

#if PLATFORM(SKIA)
#include "PlatformContextSkia.h"
#endif

namespace WebCore {

RenderSVGResourceType RenderSVGResourceSolidColor::s_resourceType = SolidColorResourceType;

RenderSVGResourceSolidColor::RenderSVGResourceSolidColor()
{
}

RenderSVGResourceSolidColor::~RenderSVGResourceSolidColor()
{
}

bool RenderSVGResourceSolidColor::applyResource(RenderObject* object, RenderStyle* style, GraphicsContext*& context, unsigned short resourceMode)
{
    // We are NOT allowed to ASSERT(object) here, unlike all other resources.
    // RenderSVGResourceSolidColor is the only resource which may be used from HTML, when rendering
    // SVG Fonts for a HTML document. This will be indicated by a null RenderObject pointer.
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    const SVGRenderStyle* svgStyle = style ? style->svgStyle() : 0;
    ColorSpace colorSpace = style ? style->colorSpace() : DeviceColorSpace;

    if (resourceMode & ApplyToFillMode) {
        context->setAlpha(svgStyle ? svgStyle->fillOpacity() : 1.0f);
        context->setFillColor(m_color, colorSpace);
        context->setFillRule(svgStyle ? svgStyle->fillRule() : RULE_NONZERO);

        if (resourceMode & ApplyToTextMode)
            context->setTextDrawingMode(cTextFill);
    } else if (resourceMode & ApplyToStrokeMode) {
        context->setAlpha(svgStyle ? svgStyle->strokeOpacity() : 1.0f);
        context->setStrokeColor(m_color, colorSpace);

        if (style)
            applyStrokeStyleToContext(context, style, object);

        if (resourceMode & ApplyToTextMode)
            context->setTextDrawingMode(cTextStroke);
    }

    return true;
}

void RenderSVGResourceSolidColor::postApplyResource(RenderObject*, GraphicsContext*& context, unsigned short resourceMode)
{
    ASSERT(context);
    ASSERT(resourceMode != ApplyToDefaultMode);

    if (!(resourceMode & ApplyToTextMode)) {
        if (resourceMode & ApplyToFillMode)
            context->fillPath();
        else if (resourceMode & ApplyToStrokeMode)
            context->strokePath();
    }

#if PLATFORM(SKIA)
    // FIXME: Move this into the GraphicsContext
    // WebKit implicitly expects us to reset the path.
    // For example in fillAndStrokePath() of RenderPath.cpp the path is 
    // added back to the context after filling. This is because internally it
    // calls CGContextFillPath() which closes the path.
    context->beginPath();
    context->platformContext()->setFillShader(0);
    context->platformContext()->setStrokeShader(0);
#endif
}

}

#endif
