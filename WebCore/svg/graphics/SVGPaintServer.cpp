/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2007 Rob Buis <buis@kde.org>
 *               2008 Dirk Schulze <krit@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServer.h"

#include "GraphicsContext.h"
#include "RenderObject.h"
#include "RenderStyle.h"
#include "SVGPaintServerSolid.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

#if PLATFORM(SKIA)
#include "PlatformContextSkia.h"
#endif

namespace WebCore {

SVGPaintServer::SVGPaintServer()
{
}

SVGPaintServer::~SVGPaintServer()
{
}

TextStream& operator<<(TextStream& ts, const SVGPaintServer& paintServer)
{
    return paintServer.externalRepresentation(ts);
}

SVGPaintServer* getPaintServerById(Document* document, const AtomicString& id)
{
    SVGResource* resource = getResourceById(document, id);
    if (resource && resource->isPaintServer())
        return static_cast<SVGPaintServer*>(resource);

    return 0;
}

SVGPaintServerSolid* SVGPaintServer::sharedSolidPaintServer()
{
    static SVGPaintServerSolid* _sharedSolidPaintServer = SVGPaintServerSolid::create().releaseRef();
    
    return _sharedSolidPaintServer;
}

SVGPaintServer* SVGPaintServer::fillPaintServer(const RenderStyle* style, const RenderObject* item)
{
    if (!style->svgStyle()->hasFill())
        return 0;

    SVGPaint* fill = style->svgStyle()->fillPaint();

    SVGPaintServer* fillPaintServer = 0;
    SVGPaint::SVGPaintType paintType = fill->paintType();
    if (paintType == SVGPaint::SVG_PAINTTYPE_URI ||
        paintType == SVGPaint::SVG_PAINTTYPE_URI_RGBCOLOR) {
        AtomicString id(SVGURIReference::getTarget(fill->uri()));
        fillPaintServer = getPaintServerById(item->document(), id);

        SVGElement* svgElement = static_cast<SVGElement*>(item->node());
        ASSERT(svgElement && svgElement->document() && svgElement->isStyled());

        if (item->isRenderPath() && fillPaintServer)
            fillPaintServer->addClient(static_cast<SVGStyledElement*>(svgElement));
        else if (!fillPaintServer && paintType == SVGPaint::SVG_PAINTTYPE_URI)
            svgElement->document()->accessSVGExtensions()->addPendingResource(id, static_cast<SVGStyledElement*>(svgElement)); 
    }
    if (paintType != SVGPaint::SVG_PAINTTYPE_URI && !fillPaintServer) {
        fillPaintServer = sharedSolidPaintServer();
        SVGPaintServerSolid* fillPaintServerSolid = static_cast<SVGPaintServerSolid*>(fillPaintServer);
        if (paintType == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
            fillPaintServerSolid->setColor(style->color());
        else
            fillPaintServerSolid->setColor(fill->color());
        // FIXME: Ideally invalid colors would never get set on the RenderStyle and this could turn into an ASSERT
        if (!fillPaintServerSolid->color().isValid())
            fillPaintServer = 0;
    }
    if (!fillPaintServer) {
        // default value (black), see bug 11017
        fillPaintServer = sharedSolidPaintServer();
        static_cast<SVGPaintServerSolid*>(fillPaintServer)->setColor(Color::black);
    }
    return fillPaintServer;
}

SVGPaintServer* SVGPaintServer::strokePaintServer(const RenderStyle* style, const RenderObject* item)
{
    if (!style->svgStyle()->hasStroke())
        return 0;

    SVGPaint* stroke = style->svgStyle()->strokePaint();

    SVGPaintServer* strokePaintServer = 0;
    SVGPaint::SVGPaintType paintType = stroke->paintType();
    if (paintType == SVGPaint::SVG_PAINTTYPE_URI ||
        paintType == SVGPaint::SVG_PAINTTYPE_URI_RGBCOLOR) {
        AtomicString id(SVGURIReference::getTarget(stroke->uri()));
        strokePaintServer = getPaintServerById(item->document(), id);

        SVGElement* svgElement = static_cast<SVGElement*>(item->node());
        ASSERT(svgElement && svgElement->document() && svgElement->isStyled());
 
        if (item->isRenderPath() && strokePaintServer)
            strokePaintServer->addClient(static_cast<SVGStyledElement*>(svgElement));
        else if (!strokePaintServer && paintType == SVGPaint::SVG_PAINTTYPE_URI)
            svgElement->document()->accessSVGExtensions()->addPendingResource(id, static_cast<SVGStyledElement*>(svgElement)); 
    }
    if (paintType != SVGPaint::SVG_PAINTTYPE_URI && !strokePaintServer) {
        strokePaintServer = sharedSolidPaintServer();
        SVGPaintServerSolid* strokePaintServerSolid = static_cast<SVGPaintServerSolid*>(strokePaintServer);
        if (paintType == SVGPaint::SVG_PAINTTYPE_CURRENTCOLOR)
            strokePaintServerSolid->setColor(style->color());
        else
            strokePaintServerSolid->setColor(stroke->color());
        // FIXME: Ideally invalid colors would never get set on the RenderStyle and this could turn into an ASSERT
        if (!strokePaintServerSolid->color().isValid())
            strokePaintServer = 0;
    }

    return strokePaintServer;
}

void applyStrokeStyleToContext(GraphicsContext* context, RenderStyle* style, const RenderObject* object)
{
    context->setStrokeThickness(SVGRenderStyle::cssPrimitiveToLength(object, style->svgStyle()->strokeWidth(), 1.0f));
    context->setLineCap(style->svgStyle()->capStyle());
    context->setLineJoin(style->svgStyle()->joinStyle());
    if (style->svgStyle()->joinStyle() == MiterJoin)
        context->setMiterLimit(style->svgStyle()->strokeMiterLimit());

    const DashArray& dashes = dashArrayFromRenderingStyle(object->style());
    float dashOffset = SVGRenderStyle::cssPrimitiveToLength(object, style->svgStyle()->strokeDashOffset(), 0.0f);
    context->setLineDash(dashes, dashOffset);
}

void SVGPaintServer::draw(GraphicsContext*& context, const RenderObject* path, SVGPaintTargetType type) const
{
    if (!setup(context, path, type))
        return;

    renderPath(context, path, type);
    teardown(context, path, type);
}

void SVGPaintServer::renderPath(GraphicsContext*& context, const RenderObject* path, SVGPaintTargetType type) const
{
    const SVGRenderStyle* style = path ? path->style()->svgStyle() : 0;

    if ((type & ApplyToFillTargetType) && (!style || style->hasFill()))
        context->fillPath();

    if ((type & ApplyToStrokeTargetType) && (!style || style->hasStroke()))
        context->strokePath();
}

#if PLATFORM(SKIA)
void SVGPaintServer::teardown(GraphicsContext*& context, const RenderObject*, SVGPaintTargetType, bool) const
{
    // FIXME: Move this into the GraphicsContext
    // WebKit implicitly expects us to reset the path.
    // For example in fillAndStrokePath() of RenderPath.cpp the path is 
    // added back to the context after filling. This is because internally it
    // calls CGContextFillPath() which closes the path.
    context->beginPath();
    context->platformContext()->setGradient(0);
    context->platformContext()->setPattern(0);
}
#else
void SVGPaintServer::teardown(GraphicsContext*&, const RenderObject*, SVGPaintTargetType, bool) const
{
}
#endif

DashArray dashArrayFromRenderingStyle(const RenderStyle* style)
{
    DashArray array;
    
    CSSValueList* dashes = style->svgStyle()->strokeDashArray();
    if (dashes) {
        CSSPrimitiveValue* dash = 0;
        unsigned long len = dashes->length();
        for (unsigned long i = 0; i < len; i++) {
            dash = static_cast<CSSPrimitiveValue*>(dashes->itemWithoutBoundsCheck(i));
            if (!dash)
                continue;

            array.append((float) dash->computeLengthFloat(const_cast<RenderStyle*>(style)));
        }
    }

    return array;
}

} // namespace WebCore

#endif
