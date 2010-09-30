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
#include "RenderSVGResource.h"
#include "SVGRootInlineBox.h"
#include "SVGTextLayoutUtilities.h"

#include <float.h>

using namespace std;

namespace WebCore {

SVGInlineTextBox::SVGInlineTextBox(RenderObject* object)
    : InlineTextBox(object)
    , m_logicalHeight(0)
    , m_paintingResource(0)
    , m_paintingResourceMode(ApplyToDefaultMode)
{
}

SVGRootInlineBox* SVGInlineTextBox::svgRootInlineBox() const
{
    // Find associated root inline box
    InlineFlowBox* parentBox = parent();

    while (parentBox && !parentBox->isRootInlineBox())
        parentBox = parentBox->parent();

    ASSERT(parentBox);
    ASSERT(parentBox->isRootInlineBox());

    if (!parentBox->isSVGRootInlineBox())
        return 0;

    return static_cast<SVGRootInlineBox*>(parentBox);
}

void SVGInlineTextBox::measureCharacter(RenderStyle* style, int position, int& charsConsumed, String& glyphName, String& unicodeString, float& glyphWidth, float& glyphHeight) const
{
    ASSERT(style);

    int offset = !isLeftToRightDirection() ? end() - position : start() + position;
    int extraCharsAvailable = len() - position - 1;
    const UChar* characters = textRenderer()->characters();

    const Font& font = style->font();
    glyphWidth = font.floatWidth(svgTextRunForInlineTextBox(characters + offset, 1, style, this), extraCharsAvailable, charsConsumed, glyphName);
    glyphHeight = font.height();

    // The unicodeString / glyphName pair is needed for kerning calculations.
    unicodeString = String(characters + offset, charsConsumed);
}

int SVGInlineTextBox::offsetForPosition(int xCoordinate, bool includePartialGlyphs) const
{
    ASSERT(!m_currentChunkPart.isValid());
    float x = xCoordinate;

    RenderText* textRenderer = this->textRenderer();
    ASSERT(textRenderer);

    RenderStyle* style = textRenderer->style();
    ASSERT(style);

    RenderBlock* containingBlock = textRenderer->containingBlock();
    ASSERT(containingBlock);

    // Move incoming relative x position to absolute position, as the character origins stored in the chunk parts use absolute coordinates
    x += containingBlock->x();

    // Figure out which text chunk part is hit
    SVGTextChunkPart hitPart;

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it) {
        const SVGTextChunkPart& part = *it;

        // Check whether we're past the hit part.
        if (x < part.firstCharacter->x)
            break;

        hitPart = part;
    }

    // If we did not hit anything, just exit.
    if (!hitPart.isValid())
        return 0;

    // Before calling Font::offsetForPosition(), subtract the start position of the first character
    // in the hit text chunk part, to pass in coordinates, which are relative to the text chunk part, as
    // constructTextRun() only builds a TextRun for the current chunk part, not the whole inline text box.
    x -= hitPart.firstCharacter->x;

    m_currentChunkPart = hitPart;
    TextRun textRun(constructTextRun(style));
    m_currentChunkPart = SVGTextChunkPart();

    // Eventually handle lengthAdjust="spacingAndGlyphs".
    // FIXME: Need to revisit the whole offsetForPosition concept for vertical text selection.
    if (!m_chunkTransformation.isIdentity())
        textRun.setHorizontalGlyphStretch(narrowPrecisionToFloat(m_chunkTransformation.a()));

    return hitPart.offset + style->font().offsetForPosition(textRun, x, includePartialGlyphs);
}

int SVGInlineTextBox::positionForOffset(int) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

FloatRect SVGInlineTextBox::selectionRectForTextChunkPart(const SVGTextChunkPart& part, int partStartPos, int partEndPos, RenderStyle* style)
{
    // Map startPos/endPos positions into chunk part
    mapStartEndPositionsIntoChunkPartCoordinates(partStartPos, partEndPos, part);

    if (partStartPos >= partEndPos)
        return FloatRect();

    // Set current chunk part, so constructTextRun() works properly.
    m_currentChunkPart = part;

    const Font& font = style->font();
    Vector<SVGChar>::const_iterator character = part.firstCharacter;
    FloatPoint textOrigin(character->x, character->y - font.ascent());

    FloatRect partRect(font.selectionRectForText(constructTextRun(style), textOrigin, part.height, partStartPos, partEndPos));
    m_currentChunkPart = SVGTextChunkPart();

    return character->characterTransform().mapRect(partRect);
}

IntRect SVGInlineTextBox::selectionRect(int, int, int startPos, int endPos)
{
    ASSERT(!m_currentChunkPart.isValid());

    int boxStart = start();
    startPos = max(startPos - boxStart, 0);
    endPos = min(endPos - boxStart, static_cast<int>(len()));

    if (startPos >= endPos)
        return IntRect();

    RenderText* text = textRenderer();
    ASSERT(text);

    RenderStyle* style = text->style();
    ASSERT(style);

    FloatRect selectionRect;

    // Figure out which text chunk part is hit
    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it)
        selectionRect.unite(selectionRectForTextChunkPart(*it, startPos, endPos, style));

    // Resepect possible chunk transformation
    if (m_chunkTransformation.isIdentity())
        return enclosingIntRect(selectionRect);

    return enclosingIntRect(m_chunkTransformation.mapRect(selectionRect));
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

    RenderStyle* style = parentRenderer->style();
    ASSERT(style);

    const SVGRenderStyle* svgStyle = style->svgStyle();
    ASSERT(svgStyle);

    bool hasFill = svgStyle->hasFill();
    bool hasStroke = svgStyle->hasStroke();
    bool paintSelectedTextOnly = paintInfo.phase == PaintPhaseSelection;

    // Determine whether or not we're selected.
    bool isPrinting = parentRenderer->document()->printing();
    bool hasSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;
    if (!hasSelection && paintSelectedTextOnly)
        return;

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

    // Compute text match marker rects. It needs to traverse all text chunk parts to figure
    // out the union selection rect of all text chunk parts that contribute to the selection.
    computeTextMatchMarkerRect(style);
    ASSERT(!m_currentChunkPart.isValid());

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it) {
        ASSERT(!m_paintingResource);

        // constructTextRun() uses the current chunk part to figure out what text to render.
        m_currentChunkPart = *it;
        paintInfo.context->save();

        // Prepare context and draw text
        if (!m_chunkTransformation.isIdentity())
            paintInfo.context->concatCTM(m_chunkTransformation);

        Vector<SVGChar>::const_iterator firstCharacter = m_currentChunkPart.firstCharacter;
        AffineTransform characterTransform = firstCharacter->characterTransform();
        if (!characterTransform.isIdentity())
            paintInfo.context->concatCTM(characterTransform);

        FloatPoint textOrigin(firstCharacter->x, firstCharacter->y);

        // Draw background once (not in both fill/stroke phases)
        if (!isPrinting && !paintSelectedTextOnly && hasSelection)
            paintSelection(paintInfo.context, textOrigin, style);

        // Spec: All text decorations except line-through should be drawn before the text is filled and stroked; thus, the text is rendered on top of these decorations.
        int decorations = style->textDecorationsInEffect();
        if (decorations & UNDERLINE)
            paintDecoration(paintInfo.context, textOrigin, UNDERLINE, hasSelection);
        if (decorations & OVERLINE)
            paintDecoration(paintInfo.context, textOrigin, OVERLINE, hasSelection);

        // Fill text
        if (hasFill) {
            m_paintingResourceMode = ApplyToFillMode | ApplyToTextMode;
            paintText(paintInfo.context, textOrigin, style, selectionStyle, hasSelection, paintSelectedTextOnly);
        }

        // Stroke text
        if (hasStroke) {
            m_paintingResourceMode = ApplyToStrokeMode | ApplyToTextMode;
            paintText(paintInfo.context, textOrigin, style, selectionStyle, hasSelection, paintSelectedTextOnly);
        }

        // Spec: Line-through should be drawn after the text is filled and stroked; thus, the line-through is rendered on top of the text.
        if (decorations & LINE_THROUGH)
            paintDecoration(paintInfo.context, textOrigin, LINE_THROUGH, hasSelection);

        m_paintingResourceMode = ApplyToDefaultMode;
        paintInfo.context->restore();
    }

    m_currentChunkPart = SVGTextChunkPart();
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

TextRun SVGInlineTextBox::constructTextRun(RenderStyle* style) const
{
    ASSERT(m_currentChunkPart.isValid());
    return svgTextRunForInlineTextBox(textRenderer()->text()->characters() + start() + m_currentChunkPart.offset, m_currentChunkPart.length, style, this);
}

void SVGInlineTextBox::mapStartEndPositionsIntoChunkPartCoordinates(int& startPos, int& endPos, const SVGTextChunkPart& part) const
{
    if (startPos >= endPos)
        return;

    // Take <text x="10 50 100">ABC</text> as example. We're called three times from paint(), because all absolute positioned
    // characters are drawn on their own. For each of them we want to find out whehter it's selected. startPos=0, endPos=1
    // could mean A, B or C is hit, depending on which chunk part is processed at the moment. With the offset & length values
    // of each chunk part we can easily find out which one is meant to be selected. Bail out for the other chunk parts.
    // If starPos is behind the current chunk or the endPos ends before this text chunk part, we're not meant to be selected.
    if (startPos >= part.offset + part.length || endPos <= part.offset) {
        startPos = 0;
        endPos = -1;
        return;
    }

    // The current processed chunk part is hit. When painting the selection, constructTextRun() builds
    // a TextRun object whose startPos is 0 and endPos is chunk part length. The code below maps the incoming
    // startPos/endPos range into a [0, part length] coordinate system, relative to the current chunk part.
    if (startPos < part.offset)
        startPos = 0;
    else
        startPos -= part.offset;

    if (endPos > part.offset + part.length)
        endPos = part.length;
    else {
        ASSERT(endPos >= part.offset);
        endPos -= part.offset;
    }

    ASSERT(startPos < endPos);
}

void SVGInlineTextBox::selectionStartEnd(int& startPos, int& endPos)
{
    InlineTextBox::selectionStartEnd(startPos, endPos);

    if (!m_currentChunkPart.isValid())
        return;

    mapStartEndPositionsIntoChunkPartCoordinates(startPos, endPos, m_currentChunkPart);
}

void SVGInlineTextBox::computeTextMatchMarkerRect(RenderStyle* style)
{
    ASSERT(!m_currentChunkPart.isValid());
    Node* node = renderer()->node();
    if (!node || !node->inDocument())
        return;

    Document* document = renderer()->document();
    Vector<DocumentMarker> markers = document->markers()->markersForNode(renderer()->node());

    Vector<DocumentMarker>::iterator markerEnd = markers.end();
    for (Vector<DocumentMarker>::iterator markerIt = markers.begin(); markerIt != markerEnd; ++markerIt) {
        const DocumentMarker& marker = *markerIt;

        // SVG is only interessted in the TextMatch marker, for now.
        if (marker.type != DocumentMarker::TextMatch)
            continue;

        FloatRect markerRect;
        int partStartPos = max(marker.startOffset - start(), static_cast<unsigned>(0));
        int partEndPos = min(marker.endOffset - start(), static_cast<unsigned>(len()));

        // Iterate over all text chunk parts, to see which ones have to be highlighted
        const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
        for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it)
            markerRect.unite(selectionRectForTextChunkPart(*it, partStartPos, partEndPos, style));

        if (!m_chunkTransformation.isIdentity())
            markerRect = m_chunkTransformation.mapRect(markerRect);

        document->markers()->setRenderedRectForMarker(node, marker, renderer()->localToAbsoluteQuad(markerRect).enclosingBoundingBox());
    }
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
 
static inline RenderObject* findRenderObjectDefininingTextDecoration(InlineFlowBox* parentBox, ETextDecoration decoration)
{
    // Lookup render object which has text-decoration set.
    RenderObject* renderer = 0;
    while (parentBox) {
        renderer = parentBox->renderer();

        // Explicitely check textDecoration() not textDecorationsInEffect(), which is inherited to
        // children, as we want to lookup the render object whose style defined the text-decoration.
        if (renderer->style() && renderer->style()->textDecoration() & decoration)
            break;

        parentBox = parentBox->parent();
    }

    ASSERT(renderer);
    return renderer;
}

void SVGInlineTextBox::paintDecoration(GraphicsContext* context, const FloatPoint& textOrigin, ETextDecoration decoration, bool hasSelection)
{
    // Find out which render style defined the text-decoration, as its fill/stroke properties have to be used for drawing instead of ours.
    RenderObject* decorationRenderer = findRenderObjectDefininingTextDecoration(parent(), decoration);
    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    const SVGRenderStyle* svgDecorationStyle = decorationStyle->svgStyle();
    ASSERT(svgDecorationStyle);

    bool hasDecorationFill = svgDecorationStyle->hasFill();
    bool hasDecorationStroke = svgDecorationStyle->hasStroke();

    if (hasSelection) {
        if (RenderStyle* pseudoStyle = decorationRenderer->getCachedPseudoStyle(SELECTION)) {
            decorationStyle = pseudoStyle;

            svgDecorationStyle = decorationStyle->svgStyle();
            ASSERT(svgDecorationStyle);

            if (!hasDecorationFill)
                hasDecorationFill = svgDecorationStyle->hasFill();
            if (!hasDecorationStroke)
                hasDecorationStroke = svgDecorationStyle->hasStroke();
        }
    }

    if (decorationStyle->visibility() == HIDDEN)
        return;

    if (hasDecorationFill) {
        m_paintingResourceMode = ApplyToFillMode;
        paintDecorationWithStyle(context, textOrigin, decorationRenderer, decoration);
    }

    if (hasDecorationStroke) {
        m_paintingResourceMode = ApplyToStrokeMode;
        paintDecorationWithStyle(context, textOrigin, decorationRenderer, decoration);
    }
}

void SVGInlineTextBox::paintDecorationWithStyle(GraphicsContext* context, const FloatPoint& textOrigin, RenderObject* decorationRenderer, ETextDecoration decoration)
{
    ASSERT(!m_paintingResource);
    ASSERT(m_paintingResourceMode != ApplyToDefaultMode);
    ASSERT(m_currentChunkPart.isValid());

    RenderStyle* decorationStyle = decorationRenderer->style();
    ASSERT(decorationStyle);

    const Font& font = decorationStyle->font();

    // The initial y value refers to overline position.
    float thickness = thicknessForDecoration(decoration, font);
    float x = textOrigin.x();
    float y = textOrigin.y() - font.ascent() + positionOffsetForDecoration(decoration, font, thickness);

    context->save();
    context->beginPath();
    context->addPath(Path::createRectangle(FloatRect(x, y, m_currentChunkPart.width, thickness)));

    if (acquirePaintingResource(context, decorationRenderer, decorationStyle))
        releasePaintingResource(context);

    context->restore();
}

void SVGInlineTextBox::paintSelection(GraphicsContext* context, const FloatPoint& textOrigin, RenderStyle* style)
{
    // See if we have a selection to paint at all.
    int startPos, endPos;
    selectionStartEnd(startPos, endPos);
    if (startPos >= endPos)
        return;

    Color backgroundColor = renderer()->selectionBackgroundColor();
    if (!backgroundColor.isValid() || !backgroundColor.alpha())
        return;

    const Font& font = style->font();

    FloatPoint selectionOrigin = textOrigin;
    selectionOrigin.move(0, -font.ascent());

    context->save();
    context->setFillColor(backgroundColor, style->colorSpace());
    context->fillRect(font.selectionRectForText(constructTextRun(style), selectionOrigin, m_currentChunkPart.height, startPos, endPos), backgroundColor, style->colorSpace());
    context->restore();
}

void SVGInlineTextBox::paintTextWithShadows(GraphicsContext* context, const FloatPoint& textOrigin, RenderStyle* style, TextRun& textRun, int startPos, int endPos)
{
    const Font& font = style->font();
    const ShadowData* shadow = style->textShadow();

    FloatRect shadowRect(FloatPoint(textOrigin.x(), textOrigin.y() - font.ascent()), FloatSize(m_currentChunkPart.width, m_currentChunkPart.height));
    do {
        if (!prepareGraphicsContextForTextPainting(context, textRun, style))
            break;

        FloatSize extraOffset;
        if (shadow)
            extraOffset = applyShadowToGraphicsContext(context, shadow, shadowRect, false /* stroked */, true /* opaque */);

        font.drawText(context, textRun, textOrigin + extraOffset, startPos, endPos);
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

void SVGInlineTextBox::paintText(GraphicsContext* context, const FloatPoint& textOrigin, RenderStyle* style, RenderStyle* selectionStyle, bool hasSelection, bool paintSelectedTextOnly)
{
    ASSERT(style);
    ASSERT(selectionStyle);
    ASSERT(m_currentChunkPart.isValid());

    int startPos = 0;
    int endPos = 0;
    selectionStartEnd(startPos, endPos);

    // Fast path if there is no selection, just draw the whole chunk part using the regular style
    TextRun textRun(constructTextRun(style));
    if (!hasSelection || startPos >= endPos) {
        paintTextWithShadows(context, textOrigin, style, textRun, 0, m_currentChunkPart.length);
        return;
    }

    // Eventually draw text using regular style until the start position of the selection
    if (startPos > 0 && !paintSelectedTextOnly)
        paintTextWithShadows(context, textOrigin, style, textRun, 0, startPos);

    // Draw text using selection style from the start to the end position of the selection
    if (style != selectionStyle)
        SVGResourcesCache::clientStyleChanged(parent()->renderer(), StyleDifferenceRepaint, selectionStyle);

    TextRun selectionTextRun(constructTextRun(selectionStyle));
    paintTextWithShadows(context, textOrigin, selectionStyle, textRun, startPos, endPos);

    if (style != selectionStyle)
        SVGResourcesCache::clientStyleChanged(parent()->renderer(), StyleDifferenceRepaint, style);

    // Eventually draw text using regular style from the end position of the selection to the end of the current chunk part
    if (endPos < m_currentChunkPart.length && !paintSelectedTextOnly)
        paintTextWithShadows(context, textOrigin, style, textRun, endPos, m_currentChunkPart.length);
}

void SVGInlineTextBox::buildLayoutInformation(SVGCharacterLayoutInfo& info, SVGLastGlyphInfo& lastGlyph)
{
    RenderText* textRenderer = this->textRenderer();
    ASSERT(textRenderer);

    RenderStyle* style = textRenderer->style();
    ASSERT(style);

    RenderObject* parentRenderer = parent()->renderer();
    ASSERT(parentRenderer);
    ASSERT(parentRenderer->node());
    ASSERT(parentRenderer->node()->isSVGElement());
    SVGElement* lengthContext = static_cast<SVGElement*>(parentRenderer->node());

    const Font& font = style->font();
    const UChar* characters = textRenderer->characters();

    unsigned startPosition = start();
    unsigned endPosition = end();
    unsigned length = len();

    const SVGRenderStyle* svgStyle = style->svgStyle();
    bool isVerticalText = isVerticalWritingMode(svgStyle);

    int charsConsumed = 0;
    for (unsigned i = 0; i < length; i += charsConsumed) {
        SVGChar svgChar;

        if (info.inPathLayout())
            svgChar.pathData = SVGCharOnPath::create();

        float glyphWidth = 0.0f;
        float glyphHeight = 0.0f;
        String glyphName;
        String unicodeString;
        measureCharacter(style, i, charsConsumed, glyphName, unicodeString, glyphWidth, glyphHeight);

        bool assignedX = false;
        bool assignedY = false;

        if (info.xValueAvailable() && (!info.inPathLayout() || (info.inPathLayout() && !isVerticalText))) {
            if (!isVerticalText)
                svgChar.newTextChunk = true;

            assignedX = true;
            svgChar.drawnSeperated = true;
            info.curx = info.xValueNext();
        }

        if (info.yValueAvailable() && (!info.inPathLayout() || (info.inPathLayout() && isVerticalText))) {
            if (isVerticalText)
                svgChar.newTextChunk = true;

            assignedY = true;
            svgChar.drawnSeperated = true;
            info.cury = info.yValueNext();
        }

        float dx = 0.0f;
        float dy = 0.0f;

        // Apply x-axis shift
        if (info.dxValueAvailable()) {
            svgChar.drawnSeperated = true;

            dx = info.dxValueNext();
            info.dx += dx;

            if (!info.inPathLayout())
                info.curx += dx;
        }

        // Apply y-axis shift
        if (info.dyValueAvailable()) {
            svgChar.drawnSeperated = true;

            dy = info.dyValueNext();
            info.dy += dy;

            if (!info.inPathLayout())
                info.cury += dy;
        }

        // Take letter & word spacing and kerning into account
        float spacing = font.letterSpacing() + calculateCSSKerning(lengthContext, style);

        const UChar* currentCharacter = characters + (!isLeftToRightDirection() ? endPosition - i : startPosition + i);
        const UChar* lastCharacter = 0;

        if (!isLeftToRightDirection()) {
            if (i < endPosition)
                lastCharacter = characters + endPosition - i +  1;
        } else {
            if (i > 0)
                lastCharacter = characters + startPosition + i - 1;
        }

        // FIXME: SVG Kerning doesn't get applied on texts on path.
        bool appliedSVGKerning = applySVGKerning(info, style, lastGlyph, unicodeString, glyphName, isVerticalText);
        if (info.nextDrawnSeperated || spacing != 0.0f || appliedSVGKerning) {
            info.nextDrawnSeperated = false;
            svgChar.drawnSeperated = true;
        }

        if (currentCharacter && Font::treatAsSpace(*currentCharacter) && lastCharacter && !Font::treatAsSpace(*lastCharacter)) {
            spacing += font.wordSpacing();

            if (spacing != 0.0f && !info.inPathLayout())
                info.nextDrawnSeperated = true;
        }

        float orientationAngle = glyphOrientationToAngle(svgStyle, isVerticalText, *currentCharacter);

        float xOrientationShift = 0.0f;
        float yOrientationShift = 0.0f;
        float glyphAdvance = applyGlyphAdvanceAndShiftRespectingOrientation(isVerticalText, orientationAngle, glyphWidth, glyphHeight,
                                                                            font, svgChar, xOrientationShift, yOrientationShift);

        // Handle textPath layout mode
        if (info.inPathLayout()) {
            float extraAdvance = isVerticalText ? dy : dx;
            float newOffset = FLT_MIN;

            if (assignedX && !isVerticalText)
                newOffset = info.curx;
            else if (assignedY && isVerticalText)
                newOffset = info.cury;

            float correctedGlyphAdvance = glyphAdvance;

            // Handle lengthAdjust="spacingAndGlyphs" by specifying per-character scale operations
            if (info.pathTextLength && info.pathChunkLength) { 
                if (isVerticalText) {
                    svgChar.pathData->yScale = info.pathChunkLength / info.pathTextLength;
                    spacing *= svgChar.pathData->yScale;
                    correctedGlyphAdvance *= svgChar.pathData->yScale;
                } else {
                    svgChar.pathData->xScale = info.pathChunkLength / info.pathTextLength;
                    spacing *= svgChar.pathData->xScale;
                    correctedGlyphAdvance *= svgChar.pathData->xScale;
                }
            }

            // Handle letter & word spacing on text path
            float pathExtraAdvance = info.pathExtraAdvance;
            info.pathExtraAdvance += spacing;

            svgChar.pathData->hidden = !info.nextPathLayoutPointAndAngle(correctedGlyphAdvance, extraAdvance, newOffset);
            svgChar.drawnSeperated = true;

            info.pathExtraAdvance = pathExtraAdvance;
        }

        // Apply rotation
        if (info.angleValueAvailable())
            info.angle = info.angleValueNext();

        // Apply baseline-shift
        if (info.baselineShiftValueAvailable()) {
            svgChar.drawnSeperated = true;
            float shift = info.baselineShiftValueNext();

            if (isVerticalText)
                info.shiftx += shift;
            else
                info.shifty -= shift;
        }

        // Take dominant-baseline / alignment-baseline into account
        yOrientationShift += alignmentBaselineToShift(isVerticalText, textRenderer, font);

        svgChar.x = info.curx;
        svgChar.y = info.cury;
        svgChar.angle = info.angle;

        // For text paths any shift (dx/dy/baseline-shift) has to be applied after the rotation
        if (!info.inPathLayout()) {
            svgChar.x += info.shiftx + xOrientationShift;
            svgChar.y += info.shifty + yOrientationShift;

            if (orientationAngle != 0.0f)
                svgChar.angle += orientationAngle;

            if (svgChar.angle != 0.0f)
                svgChar.drawnSeperated = true;
        } else {
            svgChar.pathData->orientationAngle = orientationAngle;

            if (isVerticalText)
                svgChar.angle -= 90.0f;

            svgChar.pathData->xShift = info.shiftx + xOrientationShift;
            svgChar.pathData->yShift = info.shifty + yOrientationShift;

            // Translate to glyph midpoint
            if (isVerticalText) {
                svgChar.pathData->xShift += info.dx;
                svgChar.pathData->yShift -= glyphAdvance / 2.0f;
            } else {
                svgChar.pathData->xShift -= glyphAdvance / 2.0f;
                svgChar.pathData->yShift += info.dy;
            }
        }

        // Advance to new position
        if (isVerticalText) {
            svgChar.drawnSeperated = true;
            info.cury += glyphAdvance + spacing;
        } else
            info.curx += glyphAdvance + spacing;

        // Advance to next character group
        for (int k = 0; k < charsConsumed; ++k) {
            info.svgChars.append(svgChar);
            info.processedSingleCharacter();
            svgChar.drawnSeperated = false;
            svgChar.newTextChunk = false;
        }
    }
}

FloatRect SVGInlineTextBox::calculateGlyphBoundaries(RenderStyle* style, int position, const SVGChar& character) const
{
    int charsConsumed = 0;
    String glyphName;
    String unicodeString;
    float glyphWidth = 0.0f;
    float glyphHeight = 0.0f;
    measureCharacter(style, position, charsConsumed, glyphName, unicodeString, glyphWidth, glyphHeight);

    FloatRect glyphRect(character.x, character.y - style->font().ascent(), glyphWidth, glyphHeight);
    glyphRect = character.characterTransform().mapRect(glyphRect);
    if (m_chunkTransformation.isIdentity())
        return glyphRect;

    return m_chunkTransformation.mapRect(glyphRect);
}

IntRect SVGInlineTextBox::calculateBoundaries() const
{
    FloatRect textRect;
    int baseline = baselinePosition(true);

    const Vector<SVGTextChunkPart>::const_iterator end = m_svgTextChunkParts.end();
    for (Vector<SVGTextChunkPart>::const_iterator it = m_svgTextChunkParts.begin(); it != end; ++it)
        textRect.unite(it->firstCharacter->characterTransform().mapRect(FloatRect(it->firstCharacter->x, it->firstCharacter->y - baseline, it->width, it->height)));

    if (m_chunkTransformation.isIdentity())
        return enclosingIntRect(textRect);

    return enclosingIntRect(m_chunkTransformation.mapRect(textRect));
}

} // namespace WebCore

#endif
