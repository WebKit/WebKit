/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGInlineTextBox.h"

#if ENABLE(SVG)
#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "InlineFlowBox.h"
#include "RenderBlock.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGResource.h"
#include "SVGRootInlineBox.h"

using namespace std;

namespace WebCore {

SVGInlineTextBox::SVGInlineTextBox(RenderObject* object)
    : InlineTextBox(object)
    , m_logicalHeight(0)
    , m_paintingResourceMode(ApplyToDefaultMode)
    , m_startsNewTextChunk(false)
    , m_paintingResource(0)
{
}

int SVGInlineTextBox::offsetForPosition(int, bool) const
{
    // SVG doesn't use the standard offset <-> position selection system, as it's not suitable for SVGs complex needs.
    // vertical text selection, inline boxes spanning multiple lines (contrary to HTML, etc.)
    ASSERT_NOT_REACHED();
    return 0;
}

int SVGInlineTextBox::offsetForPositionInFragment(const SVGTextFragment& fragment, float position, bool includePartialGlyphs) const
{
    RenderText* textRenderer = this->textRenderer();
    ASSERT(textRenderer);

    RenderStyle* style = textRenderer->style();
    ASSERT(style);

    TextRun textRun(constructTextRun(style, fragment));

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    // FIXME: Handle vertical text.
    if (!fragment.transform.isIdentity())
        textRun.setHorizontalGlyphStretch(narrowPrecisionToFloat(fragment.transform.xScale()));

    return fragment.positionListOffset - start() + style->font().offsetForPosition(textRun, position, includePartialGlyphs);
}

int SVGInlineTextBox::positionForOffset(int) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

FloatRect SVGInlineTextBox::selectionRectForTextFragment(const SVGTextFragment& fragment, int startPosition, int endPosition, RenderStyle* style)
{
    ASSERT(startPosition < endPosition);

    const Font& font = style->font();
    FloatPoint textOrigin(fragment.x, fragment.y - font.ascent());
    return font.selectionRectForText(constructTextRun(style, fragment), textOrigin, fragment.height, startPosition, endPosition);
}

IntRect SVGInlineTextBox::selectionRect(int, int, int startPosition, int endPosition)
{
    int boxStart = start();
    startPosition = max(startPosition - boxStart, 0);
    endPosition = min(endPosition - boxStart, static_cast<int>(len()));
    if (startPosition >= endPosition)
        return IntRect();

    RenderText* text = textRenderer();
    ASSERT(text);

    RenderStyle* style = text->style();
    ASSERT(style);

    FloatRect selectionRect;
    int fragmentStartPosition = 0;
    int fragmentEndPosition = 0;

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        FloatRect fragmentRect = selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style);
        if (!fragment.transform.isIdentity())
            fragmentRect = fragment.transform.mapRect(fragmentRect);

        selectionRect.unite(fragmentRect);
    }

    return enclosingIntRect(selectionRect);
}

void SVGInlineTextBox::paintSelectionBackground(PaintInfo& paintInfo)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(renderer()));
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(truncation() == cNoTruncation);

    if (renderer()->style()->visibility() != VISIBLE)
        return;

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);
    ASSERT(!parentRenderer->document()->printing());

    // Determine whether or not we're selected.
    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
    bool hasSelection = selectionState() != RenderObject::SelectionNone;
    if (!hasSelection || paintSelectedTextOnly)
        return;

    Color backgroundColor = renderer()->selectionBackgroundColor();
    if (!backgroundColor.isValid() || !backgroundColor.alpha())
        return;

    RenderStyle* style = parentRenderer->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    bool hasFill = svgStyle->hasFill();
    bool hasStroke = svgStyle->hasStroke();

    RenderStyle* selectionStyle = style;
    if (hasSelection) {
        selectionStyle = parentRenderer->getCachedPseudoStyle(SELECTION);
        if (selectionStyle) {
            const SVGRenderStyle* svgSelectionStyle = selectionStyle->svgStyle();
            ASSERT(svgSelectionStyle);

            if (!hasFill)
                hasFill = svgSelectionStyle->hasFill();
            if (!hasStroke)
                hasStroke = svgSelectionStyle->hasStroke();
        } else
            selectionStyle = style;
    }

    int startPosition, endPosition;
    selectionStartEnd(startPosition, endPosition);

    int fragmentStartPosition = 0;
    int fragmentEndPosition = 0;
    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_textFragments.at(i);
        ASSERT(!m_paintingResource);

        fragmentStartPosition = startPosition;
        fragmentEndPosition = endPosition;
        if (!mapStartEndPositionsIntoFragmentCoordinates(fragment, fragmentStartPosition, fragmentEndPosition))
            continue;

        paintInfo.context->save();

        if (!fragment.transform.isIdentity())
            paintInfo.context->concatCTM(fragment.transform);

        paintInfo.context->setFillColor(backgroundColor, style->colorSpace());
        paintInfo.context->fillRect(selectionRectForTextFragment(fragment, fragmentStartPosition, fragmentEndPosition, style), backgroundColor, style->colorSpace());

        m_paintingResourceMode = ApplyToDefaultMode;
        paintInfo.context->restore();
    }

    ASSERT(!m_paintingResource);
}

void SVGInlineTextBox::paint(PaintInfo& paintInfo, int, int)
{
    ASSERT(paintInfo.shouldPaintWithinRoot(renderer()));
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(truncation() == cNoTruncation);

    if (renderer()->style()->visibility() != VISIBLE)
        return;

    // Note: We're explicitely not supporting composition & custom underlines and custom highlighters - unlike InlineTextBox.
    // If we ever need that for SVG, it's very easy to refactor and reuse the code.

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);

    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;
    bool hasSelection = !parentRenderer->document()->printing() && selectionState() != RenderObject::SelectionNone;
    if (!hasSelection && paintSelectedTextOnly)
        return;

    RenderStyle* style = parentRenderer->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    bool hasFill = svgStyle->hasFill();
    bool hasStroke = svgStyle->hasStroke();

    RenderStyle* selectionStyle = style;
    if (hasSelection) {
        selectionStyle = parentRenderer->getCachedPseudoStyle(SELECTION);
        if (selectionStyle) {
            const SVGRenderStyle* svgSelectionStyle = selectionStyle->svgStyle();
            ASSERT(svgSelectionStyle);

            if (!hasFill)
                hasFill = svgSelectionStyle->hasFill();
            if (!hasStroke)
                hasStroke = svgSelectionStyle->hasStroke();
        } else
            selectionStyle = style;
    }

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        SVGTextFragment& fragment = m_textFragments.at(i);
        ASSERT(!m_paintingResource);

        paintInfo.context->save();

        if (!fragment.transform.isIdentity())
            paintInfo.context->concatCTM(fragment.transform);

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        int decorations = style->textDecorationsInEffect();
        if (decorations & UNDERLINE)
            paintDecoration(paintInfo.context, UNDERLINE, fragment);
        if (decorations & OVERLINE)
            paintDecoration(paintInfo.context, OVERLINE, fragment);

        // Fill text
        if (hasFill) {
            m_paintingResourceMode = ApplyToFillMode | ApplyToTextMode;
            paintText(paintInfo.context, style, selectionStyle, fragment, hasSelection, paintSelectedTextOnly);
        }

        // Stroke text
        if (hasStroke) {
            m_paintingResourceMode = ApplyToStrokeMode | ApplyToTextMode;
            paintText(paintInfo.context, style, selectionStyle, fragment, hasSelection, paintSelectedTextOnly);
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & LINE_THROUGH)
            paintDecoration(paintInfo.context, LINE_THROUGH, fragment);

        m_paintingResourceMode = ApplyToDefaultMode;
        paintInfo.context->restore();
    }

    ASSERT(!m_paintingResource);
}

bool SVGInlineTextBox::acquirePaintingResource(GraphicsContext*& context, RenderObject* renderer, RenderStyle* style)
{
    ASSERT(renderer);
    ASSERT(style);
    ASSERT(m_paintingResourceMode != ApplyToDefaultMode);

    if (m_paintingResourceMode & ApplyToFillMode)
        m_paintingResource = RenderSVGResource::fillPaintingResource(renderer, style);
    else if (m_paintingResourceMode & ApplyToStrokeMode)
        m_paintingResource = RenderSVGResource::strokePaintingResource(renderer, style);
    else {
        // We're either called for stroking or filling.
        ASSERT_NOT_REACHED();
    }

    if (!m_paintingResource)
        return false;

    m_paintingResource->applyResource(renderer, style, context, m_paintingResourceMode);
    return true;
}

void SVGInlineTextBox::releasePaintingResource(GraphicsContext*& context)
{
    ASSERT(m_paintingResource);

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);

    m_paintingResource->postApplyResource(parentRenderer, context, m_paintingResourceMode);
    m_paintingResource = 0;
}

bool SVGInlineTextBox::prepareGraphicsContextForTextPainting(GraphicsContext*& context, TextRun& textRun, RenderStyle* style)
{
    bool acquiredResource = acquirePaintingResource(context, parent()->renderer(), style);

#if ENABLE(SVG_FONTS)
    // SVG Fonts need access to the painting resource used to draw the current text chunk.
    if (acquiredResource)
        textRun.setActivePaintingResource(m_paintingResource);
#endif

    return acquiredResource;
}

void SVGInlineTextBox::restoreGraphicsContextAfterTextPainting(GraphicsContext*& context, TextRun& textRun)
{
    releasePaintingResource(context);

#if ENABLE(SVG_FONTS)
    textRun.setActivePaintingResource(0);
#endif
}

TextRun SVGInlineTextBox::constructTextRun(RenderStyle* style, const SVGTextFragment& fragment) const
{
    ASSERT(style);
    ASSERT(textRenderer());

    RenderText* text = textRenderer();
    ASSERT(text);

    TextRun run(text->characters() + fragment.positionListOffset
                , fragment.length
                , false /* allowTabs */
                , 0 /* xPos, only relevant with allowTabs=true */
                , 0 /* padding, only relevant for justified text, not relevant for SVG */
                , direction() == RTL
                , m_dirOverride || style->visuallyOrdered() /* directionalOverride */);

#if ENABLE(SVG_FONTS)
    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);

    run.setReferencingRenderObject(parentRenderer);
#endif

    // Disable any word/character rounding.
    run.disableRoundingHacks();

    // We handle letter & word spacing ourselves.
    run.disableSpacing();
    return run;
}

bool SVGInlineTextBox::mapStartEndPositionsIntoFragmentCoordinates(const SVGTextFragment& fragment, int& startPosition, int& endPosition) const
{
    if (startPosition >= endPosition)
        return false;

    int offset = static_cast<int>(fragment.positionListOffset) - start();
    int length = static_cast<int>(fragment.length);

    if (startPosition >= offset + length || endPosition <= offset)
        return false;

    if (startPosition < offset)
        startPosition = 0;
    else
        startPosition -= offset;

    if (endPosition > offset + length)
        endPosition = length;
    else {
        ASSERT(endPosition >= offset);
        endPosition -= offset;
    }

    ASSERT(startPosition < endPosition);
    return true;
}

static inline float positionOffsetForDecoration(ETextDecoration decoration, const Font& font, float thickness)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera.
    if (decoration == UNDERLINE)
        return font.ascent() + thickness * 1.5f;
    if (decoration == OVERLINE)
        return thickness;
    if (decoration == LINE_THROUGH)
        return font.ascent() * 5.0f / 8.0f;

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float thicknessForDecoration(ETextDecoration, const Font& font)
{
    // FIXME: For SVG Fonts we need to use the attributes defined in the <font-face> if specified.
    // Compatible with Batik/Opera
    return font.size() / 20.0f;
}

static inline RenderObject* findRenderObjectDefininingTextDecoration(InlineFlowBox* parentBox)
{
    // Lookup first render object in parent hierarchy which has text-decoration set.
    RenderObject* renderer = 0;
    while (parentBox) {
        renderer = parentBox->renderer();

        if (renderer->style() && renderer->style()->textDecoration() != TDNONE)
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(renderer);
    return renderer;
}

void SVGInlineTextBox::paintDecoration(GraphicsContext* context, ETextDecoration decoration, const SVGTextFragment& fragment)
{
    if (textRenderer()->style()->textDecorationsInEffect() == TDNONE)
        return;

    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    RenderObject* decorationRenderer = findRenderObjectDefininingTextDecoration(parent());
    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    if (decorationStyle->visibility() == HIDDEN)
        return;

    const SVGRenderStyle* svgDecorationStyle = decorationStyle->svgStyle();
    ASSERT(svgDecorationStyle);

    bool hasDecorationFill = svgDecorationStyle->hasFill();
    bool hasDecorationStroke = svgDecorationStyle->hasStroke();

    if (hasDecorationFill) {
        m_paintingResourceMode = ApplyToFillMode;
        paintDecorationWithStyle(context, decoration, fragment, decorationRenderer);
    }

    if (hasDecorationStroke) {
        m_paintingResourceMode = ApplyToStrokeMode;
        paintDecorationWithStyle(context, decoration, fragment, decorationRenderer);
    }
}

void SVGInlineTextBox::paintDecorationWithStyle(GraphicsContext* context, ETextDecoration decoration, const SVGTextFragment& fragment, RenderObject* decorationRenderer)
{
    ASSERT(!m_paintingResource);
    ASSERT(m_paintingResourceMode != ApplyToDefaultMode);

    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    const Font& font = decorationStyle->font();

    // The initial y value refers to overline position.
    float thickness = thicknessForDecoration(decoration, font);
    float y = fragment.y - font.ascent() + positionOffsetForDecoration(decoration, font, thickness);

    context->save();
    context->beginPath();
    context->addPath(Path::createRectangle(FloatRect(fragment.x, y, fragment.width, thickness)));

    if (acquirePaintingResource(context, decorationRenderer, decorationStyle))
        releasePaintingResource(context);

    context->restore();
}

void SVGInlineTextBox::paintTextWithShadows(GraphicsContext* context, RenderStyle* style, TextRun& textRun, const SVGTextFragment& fragment, int startPosition, int endPosition)
{
    const Font& font = style->font();
    const ShadowData* shadow = style->textShadow();

    FloatPoint textOrigin(fragment.x, fragment.y);
    FloatRect shadowRect(FloatPoint(textOrigin.x(), textOrigin.y() - font.ascent()), FloatSize(fragment.width, fragment.height));

    do {
        if (!prepareGraphicsContextForTextPainting(context, textRun, style))
            break;

        FloatSize extraOffset;
        if (shadow)
            extraOffset = applyShadowToGraphicsContext(context, shadow, shadowRect, false /* stroked */, true /* opaque */);

        font.drawText(context, textRun, textOrigin + extraOffset, startPosition, endPosition);
        restoreGraphicsContextAfterTextPainting(context, textRun);

        if (!shadow)
            break;

        if (shadow->next())
            context->restore();
        else
            context->clearShadow();

        shadow = shadow->next();
    } while (shadow);
}

void SVGInlineTextBox::paintText(GraphicsContext* context, RenderStyle* style, RenderStyle* selectionStyle, const SVGTextFragment& fragment, bool hasSelection, bool paintSelectedTextOnly)
{
    ASSERT(style);
    ASSERT(selectionStyle);

    int startPosition = 0;
    int endPosition = 0;
    if (hasSelection) {
        selectionStartEnd(startPosition, endPosition);
        hasSelection = mapStartEndPositionsIntoFragmentCoordinates(fragment, startPosition, endPosition);
    }

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    TextRun textRun(constructTextRun(style, fragment));
    if (!hasSelection || startPosition >= endPosition) {
        paintTextWithShadows(context, style, textRun, fragment, 0, fragment.length);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    if (startPosition > 0 && !paintSelectedTextOnly)
        paintTextWithShadows(context, style, textRun, fragment, 0, startPosition);

    // Draw text using selection style from the start to the end position of the selection
    if (style != selectionStyle)
        SVGResourcesCache::clientStyleChanged(parent()->renderer(), StyleDifferenceRepaint, selectionStyle);

    TextRun selectionTextRun(constructTextRun(selectionStyle, fragment));
    paintTextWithShadows(context, selectionStyle, textRun, fragment, startPosition, endPosition);

    if (style != selectionStyle)
        SVGResourcesCache::clientStyleChanged(parent()->renderer(), StyleDifferenceRepaint, style);

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPosition < static_cast<int>(fragment.length) && !paintSelectedTextOnly)
        paintTextWithShadows(context, style, textRun, fragment, endPosition, fragment.length);
}

IntRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;

    RenderText* textRenderer = this->textRenderer();
    ASSERT(textRenderer);

    RenderStyle* style = textRenderer->style();
    ASSERT(style);

    int baseline = baselinePosition(true);
    int heightDifference = baseline - style->font().ascent();

    unsigned textFragmentsSize = m_textFragments.size();
    for (unsigned i = 0; i < textFragmentsSize; ++i) {
        const SVGTextFragment& fragment = m_textFragments.at(i);
        FloatRect fragmentRect(fragment.x, fragment.y - baseline, fragment.width, fragment.height + heightDifference);

        if (!fragment.transform.isIdentity())
            fragmentRect = fragment.transform.mapRect(fragmentRect);

        textRect.unite(fragmentRect);
    }

    return enclosingIntRect(textRect);
}

} // namespace WebCore

#endif
