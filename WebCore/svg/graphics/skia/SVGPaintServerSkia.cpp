/*
 * Copyright (c) 2008, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServer.h"

#include "GraphicsContext.h"
#include "RenderPath.h"
#include "PlatformContextSkia.h"
#include "SkiaUtils.h"

namespace WebCore {

void SVGPaintServer::draw(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type) const
{
    if (!setup(context, object, type))
        return;

    renderPath(context, object, type);
    teardown(context, object, type);
}

void SVGPaintServer::teardown(GraphicsContext*& context, const RenderObject*, SVGPaintTargetType, bool isPaintingText) const
{
    // WebKit implicitly expects us to reset the path.
    // For example in fillAndStrokePath() of RenderPath.cpp the path is 
    // added back to the context after filling. This is because internally it
    // calls CGContextFillPath() which closes the path.
    context->beginPath();
    context->platformContext()->setGradient(0);
    context->platformContext()->setPattern(0);
}

void SVGPaintServer::renderPath(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type) const
{
    RenderStyle* renderStyle = object ? object->style() : 0;

    if ((type & ApplyToFillTargetType) && (!renderStyle || renderStyle->svgStyle()->hasFill()))
        context->fillPath();

    if ((type & ApplyToStrokeTargetType) && (!renderStyle || renderStyle->svgStyle()->hasStroke()))
        context->strokePath();
}

} // namespace WebCore

#endif
