/**
 * Copyright (C) 2007 Rob Buis <buis@kde.org>
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGInlineTextBox.h"

#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "InlineFlowBox.h"
#include "Range.h"
#include "SVGPaintServer.h"
#include "SVGRootInlineBox.h"
#include "Text.h"

#include <float.h>

namespace WebCore {

SVGInlineTextBox::SVGInlineTextBox(RenderObject* obj)
    : InlineTextBox(obj)
    , m_height(0)
{
}

int SVGInlineTextBox::selectionTop()
{
    return m_y;
}
 
int SVGInlineTextBox::selectionHeight()
{
    return m_height;
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

float SVGInlineTextBox::calculateGlyphWidth(RenderStyle* style, int offset, int extraCharsAvailable, int& charsConsumed, String& glyphName) const
{
    ASSERT(style);
    return style->font().floatWidth(svgTextRunForInlineTextBox(textRenderer()->text()->characters() + offset, 1, style, this, 0), extraCharsAvailable, charsConsumed, glyphName);
}

float SVGInlineTextBox::calculateGlyphHeight(RenderStyle* style, int, int) const
{
    // This is just a guess, and the only purpose of this function is to centralize this hack.
    // In real-life top-top-bottom scripts this won't be enough, I fear.
    return style->font().ascent() + style->font().descent();
}

FloatRect SVGInlineTextBox::calculateGlyphBoundaries(RenderStyle* style, int offset, const SVGChar& svgChar) const
{
    const Font& font = style->font();

    // Take RTL text into account and pick right glyph width/height.
    float glyphWidth = 0.0f;

    // FIXME: account for multi-character glyphs
    int charsConsumed;
    String glyphName;
    if (direction() == LTR)
        glyphWidth = calculateGlyphWidth(style, offset, 0, charsConsumed, glyphName);
    else
        glyphWidth = calculateGlyphWidth(style, start() + end() - offset, 0, charsConsumed, glyphName);

    float x1 = svgChar.x;
    float x2 = svgChar.x + glyphWidth;

    float y1 = svgChar.y - font.ascent();
    float y2 = svgChar.y + font.descent();

    FloatRect glyphRect(x1, y1, x2 - x1, y2 - y1);

    // Take per-character transformations into account
    TransformationMatrix ctm = svgChar.characterTransform();
    if (!ctm.isIdentity())
        glyphRect = ctm.mapRect(glyphRect);

    return glyphRect;
}

// Helper class for closestCharacterToPosition()
struct SVGInlineTextBoxClosestCharacterToPositionWalker {
    SVGInlineTextBoxClosestCharacterToPositionWalker(int x, int y)
        : m_character(0)
        , m_distance(FLT_MAX)
        , m_x(x)
        , m_y(y)
        , m_offsetOfHitCharacter(0)
    {
    }

    void chunkPortionCallback(SVGInlineTextBox* textBox, int startOffset, const TransformationMatrix& chunkCtm,
                              const Vector<SVGChar>::iterator& start, const Vector<SVGChar>::iterator& end)
    {
        RenderStyle* style = textBox->textRenderer()->style();

        Vector<SVGChar>::iterator closestCharacter = 0;
        unsigned int closestOffset = UINT_MAX;

        for (Vector<SVGChar>::iterator it = start; it != end; ++it) {
            if (it->isHidden())
                continue;

            unsigned int newOffset = textBox->start() + (it - start) + startOffset;
            FloatRect glyphRect = chunkCtm.mapRect(textBox->calculateGlyphBoundaries(style, newOffset, *it));

            // Take RTL text into account and pick right glyph width/height.
            // NOTE: This offset has to be corrected _after_ calling calculateGlyphBoundaries
            if (textBox->direction() == RTL)
                newOffset = textBox->start() + textBox->end() - newOffset;

            // Calculate distances relative to the glyph mid-point. I hope this is accurate enough.
            float xDistance = glyphRect.x() + glyphRect.width() / 2.0f - m_x;
            float yDistance = glyphRect.y() - glyphRect.height() / 2.0f - m_y;

            float newDistance = sqrtf(xDistance * xDistance + yDistance * yDistance);
            if (newDistance <= m_distance) {
                m_distance = newDistance;
                closestOffset = newOffset;
                closestCharacter = it;
            }
        }

        if (closestOffset != UINT_MAX) {
            // Record current chunk, if it contains the current closest character next to the mouse.
            m_character = closestCharacter;
            m_offsetOfHitCharacter = closestOffset;
        }
    }

    SVGChar* character() const
    {
        return m_character;
    }

    int offsetOfHitCharacter() const
    {
        if (!m_character)
            return 0;

        return m_offsetOfHitCharacter;
    }

private:
    Vector<SVGChar>::iterator m_character;
    float m_distance;

    int m_x;
    int m_y;
    int m_offsetOfHitCharacter;
};

// Helper class for selectionRect()
struct SVGInlineTextBoxSelectionRectWalker {
    SVGInlineTextBoxSelectionRectWalker()
    {
    }

    void chunkPortionCallback(SVGInlineTextBox* textBox, int startOffset, const TransformationMatrix& chunkCtm,
                              const Vector<SVGChar>::iterator& start, const Vector<SVGChar>::iterator& end)
    {
        RenderStyle* style = textBox->textRenderer()->style();

        for (Vector<SVGChar>::iterator it = start; it != end; ++it) {
            if (it->isHidden())
                continue;

            unsigned int newOffset = textBox->start() + (it - start) + startOffset;
            m_selectionRect.unite(textBox->calculateGlyphBoundaries(style, newOffset, *it));
        }

        m_selectionRect = chunkCtm.mapRect(m_selectionRect);
    }

    FloatRect selectionRect() const
    {
        return m_selectionRect;
    }

private:
    FloatRect m_selectionRect;
};

SVGChar* SVGInlineTextBox::closestCharacterToPosition(int x, int y, int& offsetOfHitCharacter) const
{
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    if (!rootBox)
        return 0;

    SVGInlineTextBoxClosestCharacterToPositionWalker walkerCallback(x, y);
    SVGTextChunkWalker<SVGInlineTextBoxClosestCharacterToPositionWalker> walker(&walkerCallback, &SVGInlineTextBoxClosestCharacterToPositionWalker::chunkPortionCallback);

    rootBox->walkTextChunks(&walker, this);

    offsetOfHitCharacter = walkerCallback.offsetOfHitCharacter();
    return walkerCallback.character();
}

bool SVGInlineTextBox::svgCharacterHitsPosition(int x, int y, int& closestOffsetInBox) const
{
    int offsetOfHitCharacter = 0;
    SVGChar* charAtPosPtr = closestCharacterToPosition(x, y, offsetOfHitCharacter);
    if (!charAtPosPtr)
        return false;

    SVGChar& charAtPos = *charAtPosPtr;
    RenderStyle* style = textRenderer()->style(m_firstLine);
    FloatRect glyphRect = calculateGlyphBoundaries(style, offsetOfHitCharacter, charAtPos);

    // FIXME: Why?
    if (direction() == RTL)
        offsetOfHitCharacter++;

    // The caller actually the closest offset before/after the hit char
    // closestCharacterToPosition returns us offsetOfHitCharacter.
    closestOffsetInBox = offsetOfHitCharacter;

    // FIXME: (bug 13910) This code does not handle bottom-to-top/top-to-bottom vertical text.

    // Check whether y position hits the current character 
    if (y < charAtPos.y - glyphRect.height() || y > charAtPos.y)
        return false;

    // Check whether x position hits the current character
    if (x < charAtPos.x) {
        if (closestOffsetInBox > 0 && direction() == LTR)
            return true;
        else if (closestOffsetInBox < (int) end() && direction() == RTL)
            return true;

        return false;
    }

    // Adjust the closest offset to after the char if x was after the char midpoint
    if (x >= charAtPos.x + glyphRect.width() / 2.0)
        closestOffsetInBox += direction() == RTL ? -1 : 1;

    // If we are past the last glyph of this box, don't mark it as 'hit'
    if (x >= charAtPos.x + glyphRect.width() && closestOffsetInBox == (int) end())
        return false;

    return true;
}

int SVGInlineTextBox::offsetForPosition(int, bool) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

int SVGInlineTextBox::positionForOffset(int) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

bool SVGInlineTextBox::nodeAtPoint(const HitTestRequest&, HitTestResult& result, int x, int y, int tx, int ty)
{
    ASSERT(!isLineBreak());

    IntRect rect = selectionRect(0, 0, 0, len());
    if (renderer()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }

    return false;
}

IntRect SVGInlineTextBox::selectionRect(int, int, int startPos, int endPos)
{
    if (startPos >= endPos)
        return IntRect();

    // TODO: Actually respect startPos/endPos - we're returning the _full_ selectionRect
    // here. This won't lead to visible bugs, but to extra work being done. Investigate.
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    if (!rootBox)
        return IntRect();

    SVGInlineTextBoxSelectionRectWalker walkerCallback;
    SVGTextChunkWalker<SVGInlineTextBoxSelectionRectWalker> walker(&walkerCallback, &SVGInlineTextBoxSelectionRectWalker::chunkPortionCallback);

    rootBox->walkTextChunks(&walker, this);
    return enclosingIntRect(walkerCallback.selectionRect());
}

void SVGInlineTextBox::paintCharacters(RenderObject::PaintInfo& paintInfo, int tx, int ty, const SVGChar& svgChar, const UChar* chars, int length, SVGPaintServer* activePaintServer)
{
    if (renderer()->style()->visibility() != VISIBLE || paintInfo.phase == PaintPhaseOutline)
        return;

    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    RenderText* text = textRenderer();
    ASSERT(text);

    bool isPrinting = text->document()->printing();

    // Determine whether or not we're selected.
    bool haveSelection = !isPrinting && selectionState() != RenderObject::SelectionNone;
    if (!haveSelection && paintInfo.phase == PaintPhaseSelection)
        // When only painting the selection, don't bother to paint if there is none.
        return;

    // Determine whether or not we have a composition.
    bool containsComposition = text->document()->frame()->editor()->compositionNode() == text->node();
    bool useCustomUnderlines = containsComposition && text->document()->frame()->editor()->compositionUsesCustomUnderlines();

    // Set our font
    RenderStyle* styleToUse = text->style(isFirstLineStyle());
    const Font& font = styleToUse->font();

    TransformationMatrix ctm = svgChar.characterTransform();
    if (!ctm.isIdentity())
        paintInfo.context->concatCTM(ctm);

    // 1. Paint backgrounds behind text if needed.  Examples of such backgrounds include selection
    // and marked text.
    if (paintInfo.phase != PaintPhaseSelection && !isPrinting) {
#if PLATFORM(MAC)
        // Custom highlighters go behind everything else.
        if (styleToUse->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(tx, ty, styleToUse->highlight());
#endif

        if (containsComposition && !useCustomUnderlines)
            paintCompositionBackground(paintInfo.context, tx, ty, styleToUse, font, 
                                       text->document()->frame()->editor()->compositionStart(),
                                       text->document()->frame()->editor()->compositionEnd());
        
        paintDocumentMarkers(paintInfo.context, tx, ty, styleToUse, font, true);

        if (haveSelection && !useCustomUnderlines) {
            int boxStartOffset = chars - text->characters() - start();
            paintSelection(boxStartOffset, svgChar, chars, length, paintInfo.context, styleToUse, font);
        }
    }

    // Set a text shadow if we have one.
    // FIXME: Support multiple shadow effects.  Need more from the CG API before
    // we can do this.
    bool setShadow = false;
    if (styleToUse->textShadow()) {
        paintInfo.context->setShadow(IntSize(styleToUse->textShadow()->x, styleToUse->textShadow()->y),
                                     styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
        setShadow = true;
    }

    IntPoint origin((int) svgChar.x, (int) svgChar.y);
    TextRun run = svgTextRunForInlineTextBox(chars, length, styleToUse, this, svgChar.x);

#if ENABLE(SVG_FONTS)
    // SVG Fonts need access to the paint server used to draw the current text chunk.
    // They need to be able to call renderPath() on a SVGPaintServer object.
    run.setActivePaintServer(activePaintServer);
#endif

    paintInfo.context->drawText(font, run, origin);

    if (paintInfo.phase != PaintPhaseSelection) {
        paintDocumentMarkers(paintInfo.context, tx, ty, styleToUse, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = text->document()->frame()->editor()->customCompositionUnderlines();
            size_t numUnderlines = underlines.size();
            
            for (size_t index = 0; index < numUnderlines; ++index) {
                const CompositionUnderline& underline = underlines[index];
                
                if (underline.endOffset <= start())
                    // underline is completely before this run.  This might be an underline that sits
                    // before the first run we draw, or underlines that were within runs we skipped 
                    // due to truncation.
                    continue;
                
                if (underline.startOffset <= end()) {
                    // underline intersects this run.  Paint it.
                    paintCompositionUnderline(paintInfo.context, tx, ty, underline);
                    if (underline.endOffset > end() + 1)
                        // underline also runs into the next run. Bail now, no more marker advancement.
                        break;
                } else
                    // underline is completely after this run, bail.  A later run will paint it.
                    break;
            }
        }
        
    }

    if (setShadow)
        paintInfo.context->clearShadow();

    if (!ctm.isIdentity())
        paintInfo.context->concatCTM(ctm.inverse());
}

void SVGInlineTextBox::paintSelection(int boxStartOffset, const SVGChar& svgChar, const UChar*, int length, GraphicsContext* p, RenderStyle* style, const Font& font)
{
    if (selectionState() == RenderObject::SelectionNone)
        return;

    int startPos, endPos;
    selectionStartEnd(startPos, endPos);

    if (startPos >= endPos)
        return;

    Color textColor = style->color();
    Color color = renderer()->selectionBackgroundColor();
    if (!color.isValid() || color.alpha() == 0)
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.  This should basically never happen, since the selection has transparency.
    if (textColor == color)
        color = Color(0xff - color.red(), 0xff - color.green(), 0xff - color.blue());

    // Map from text box positions and a given start offset to chunk positions
    // 'boxStartOffset' represents the beginning of the text chunk.
    if ((startPos > boxStartOffset && endPos > boxStartOffset + length) || boxStartOffset >= endPos)
        return;

    if (endPos > boxStartOffset + length)
        endPos = boxStartOffset + length;

    if (startPos < boxStartOffset)
        startPos = boxStartOffset;

    ASSERT(startPos >= boxStartOffset);
    ASSERT(endPos <= boxStartOffset + length);
    ASSERT(startPos < endPos);

    p->save();

    int adjust = startPos >= boxStartOffset ? boxStartOffset : 0;
    p->drawHighlightForText(font, svgTextRunForInlineTextBox(textRenderer()->text()->characters() + start() + boxStartOffset, length, style, this, svgChar.x),
                            IntPoint((int) svgChar.x, (int) svgChar.y - font.ascent()),
                            font.ascent() + font.descent(), color, startPos - adjust, endPos - adjust);

    p->restore();
}

static inline Path pathForDecoration(ETextDecoration decoration, RenderObject* object, float x, float y, float width)
{
    float thickness = SVGRenderStyle::cssPrimitiveToLength(object, object->style()->svgStyle()->strokeWidth(), 1.0f);

    const Font& font = object->style()->font();
    thickness = max(thickness * powf(font.size(), 2.0f) / font.unitsPerEm(), 1.0f);

    if (decoration == UNDERLINE)
        y += thickness * 1.5f; // For compatibility with Batik/Opera
    else if (decoration == OVERLINE)
        y += thickness;

    float halfThickness = thickness / 2.0f;
    return Path::createRectangle(FloatRect(x + halfThickness, y, width - 2.0f * halfThickness, thickness));
}

void SVGInlineTextBox::paintDecoration(ETextDecoration decoration, GraphicsContext* context, int tx, int ty, int width, const SVGChar& svgChar, const SVGTextDecorationInfo& info)
{
    if (renderer()->style()->visibility() != VISIBLE)
        return;

    // This function does NOT accept combinated text decorations. It's meant to be invoked for just one.
    ASSERT(decoration == TDNONE || decoration == UNDERLINE || decoration == OVERLINE || decoration == LINE_THROUGH || decoration == BLINK);

    bool isFilled = info.fillServerMap.contains(decoration);
    bool isStroked = info.strokeServerMap.contains(decoration);

    if (!isFilled && !isStroked)
        return;

    int baseline = renderer()->style(m_firstLine)->font().ascent();
    if (decoration == UNDERLINE)
        ty += baseline;
    else if (decoration == LINE_THROUGH)
        ty += 2 * baseline / 3;

    context->save();
    context->beginPath();

    TransformationMatrix ctm = svgChar.characterTransform();
    if (!ctm.isIdentity())
        context->concatCTM(ctm);

    if (isFilled) {
        if (RenderObject* fillObject = info.fillServerMap.get(decoration)) {
            if (SVGPaintServer* fillPaintServer = SVGPaintServer::fillPaintServer(fillObject->style(), fillObject)) {
                context->addPath(pathForDecoration(decoration, fillObject, tx, ty, width));
                fillPaintServer->draw(context, fillObject, ApplyToFillTargetType);
            }
        }
    }

    if (isStroked) {
        if (RenderObject* strokeObject = info.strokeServerMap.get(decoration)) {
            if (SVGPaintServer* strokePaintServer = SVGPaintServer::strokePaintServer(strokeObject->style(), strokeObject)) {
                context->addPath(pathForDecoration(decoration, strokeObject, tx, ty, width));
                strokePaintServer->draw(context, strokeObject, ApplyToStrokeTargetType);
            }
        }
    }

    context->restore();
}

} // namespace WebCore

#endif
