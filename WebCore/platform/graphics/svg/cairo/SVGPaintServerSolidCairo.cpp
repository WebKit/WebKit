/*
 * Copyright (C) 2007 Alp Toker <alp.toker@collabora.co.uk>
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
#include "SVGPaintServerSolid.h"

#include "GraphicsContext.h"
#include "KCanvasRenderingStyle.h"
#include "RenderPath.h"

namespace WebCore {

bool SVGPaintServerSolid::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    // TODO: share this code with other PaintServers

    cairo_t* cr = context->platformContext();
    const SVGRenderStyle* style = object->style()->svgStyle();

    float red, green, blue, alpha;
    color().getRGBA(red, green, blue, alpha);

    if ((type & ApplyToFillTargetType) && style->hasFill()) {
        alpha = style->fillOpacity();

        cairo_set_fill_rule(cr, style->fillRule() == RULE_EVENODD ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
    }

    if ((type & ApplyToStrokeTargetType) && style->hasStroke()) {
        alpha = style->strokeOpacity();

        cairo_set_line_width(cr, KSVGPainterFactory::cssPrimitiveToLength(object, style->strokeWidth(), 1.0));
        context->setLineCap(style->capStyle());
        context->setLineJoin(style->joinStyle());
        if (style->joinStyle() == MiterJoin)
            context->setMiterLimit(style->strokeMiterLimit());

        const KCDashArray& dashes = KSVGPainterFactory::dashArrayFromRenderingStyle(object->style());
        double* dsh = new double[dashes.size()];
        for (int i = 0 ; i < dashes.size() ; i++)
            dsh[i] = dashes[i];
        double dashOffset = KSVGPainterFactory::cssPrimitiveToLength(object, style->strokeDashOffset(), 0.0);
        cairo_set_dash(cr, dsh, dashes.size(), dashOffset);
        delete[] dsh;
    }

    cairo_set_source_rgba(cr, red, green, blue, alpha);

    return true;
}

} // namespace WebCore

#endif
