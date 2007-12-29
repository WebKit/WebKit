/**
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG_FONTS)
#include "Font.h"

#include "FontData.h"
#include "GraphicsContext.h"
#include "RenderObject.h"
#include "SVGGlyphElement.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGPaintServer.h"

namespace WebCore {

static inline bool isVerticalWritingMode(const SVGRenderStyle* style)
{
    return style->writingMode() == WM_TBRL || style->writingMode() == WM_TB; 
}

void Font::drawGlyphsWithSVGFont(GraphicsContext* context, RenderObject* renderObject,
                                 const FontData* fontData, const GlyphBuffer& glyphBuffer,
                                 int from, int to, const FloatPoint& point) const
{
    ASSERT(renderObject);

    ASSERT(!fontData->isCustomFont());
    ASSERT(fontData->isSVGFont());

    SVGFontData* svgFontData = fontData->svgFontData();
    ASSERT(svgFontData);

    SVGFontFaceElement* fontFace = svgFontData->fontFaceElement.get();
    ASSERT(fontFace);

    RenderStyle* style = renderObject->style();
    ASSERT(style);

    // TODO: This is ridiculous. Glyphs shouldn't be collected when painted. The problem is
    // that we need to cache 'Glyph' instead of 'String' in the GlyphHashMap, to obtain
    // a set of 'Glyph's from a 'String' we need the local function glyphDataForCharacter.
    // Somehow the design needs to be changed. Not yet sure how, needs discussion.
    if (fontFace->parentNode() && fontFace->parentNode()->hasTagName(SVGNames::fontTag))
        static_cast<SVGFontElement*>(fontFace->parentNode())->collectGlyphs(*this);

    float fontSize = size();
    unsigned unitsPerEm = fontFace->unitsPerEm();
    bool isVerticalText = isVerticalWritingMode(style->svgStyle());
    float scale = SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, 1.0f);

    SVGPaintServer* fillPaintServer = SVGPaintServer::fillPaintServer(style, renderObject);
    SVGPaintServer* strokePaintServer = SVGPaintServer::strokePaintServer(style, renderObject);

    FloatPoint startPoint = point;

    FloatPoint glyphOrigin;
    if (!isVerticalText) {
        glyphOrigin.setX(SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, svgFontData->horizontalOriginX));
        glyphOrigin.setY(SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, svgFontData->horizontalOriginY));
    }

    for (int i = from; i < to; ++i) {
        SVGGlyphIdentifier identifier = fontFace->glyphIdentifierForGlyphCode(glyphBuffer.glyphAt(i));

        // TODO: Support arbitary SVG content as glyph (currently limited to <glyph d="..."> situations)
        if (!identifier.pathData.isEmpty()) {
            if (isVerticalText) {
                glyphOrigin.setX(SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, identifier.verticalOriginX));
                glyphOrigin.setY(SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, identifier.verticalOriginY));
            }

            context->save();
            context->translate(startPoint.x() + glyphOrigin.x(), startPoint.y() + glyphOrigin.y());
            context->scale(FloatSize(scale, -scale));

            context->beginPath();

            if (fillPaintServer) {
                context->addPath(identifier.pathData);
                fillPaintServer->draw(context, renderObject, ApplyToFillTargetType);
            }

            if (strokePaintServer) {
                context->addPath(identifier.pathData);
                strokePaintServer->draw(context, renderObject, ApplyToStrokeTargetType);
            }

            context->restore();

            if (isVerticalText)
                startPoint.move(0, SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, identifier.verticalAdvanceY));
            else
                startPoint.move(SVGFontData::convertEmUnitToPixel(fontSize, unitsPerEm, identifier.horizontalAdvanceX), 0);
        }
    }
}

}

#endif
