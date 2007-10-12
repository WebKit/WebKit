/**
 * This file is part of the DOM implementation for KDE.
 *
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
#include "SVGInlineTextBox.h"

#if ENABLE(SVG)
#include "FloatRect.h"
#include "TextStyle.h"
#include "InlineFlowBox.h"
#include "SVGCharacterLayoutInfo.h"
#include "SVGRootInlineBox.h"
#endif

namespace WebCore {

SVGInlineTextBox::SVGInlineTextBox(RenderObject* obj)
    : InlineTextBox(obj)
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

    return static_cast<SVGRootInlineBox*>(parentBox);
}

SVGChar* SVGInlineTextBox::closestCharacterToPosition(int x, int y, int& offset) const
{
    // Find corresponding text chunk for our inline box & reference x position
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    // Iterate through the characters, respecting their individual placement
    // Find the character within the chunk with the smallest diagonal distance
    // to the current position. Check wheter the passed x value hits that character.
    float distance = FLT_MAX;
    int chunkOffset = 0;

    SVGTextChunk chunk;
    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator end = chunks.end();

    const Font& font = textObject()->style()->font();

    for (; it != end; ++it) {
        SVGTextChunk& curChunk = *it;

        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = curChunk.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = curChunk.boxes.end();

        bool found = false;
        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;
            if (range.box == this) {
                found = true;
                break;
            }
        }

        if (!found)
            continue;

        boxIt = curChunk.boxes.begin();

        unsigned int startOffset = 0;
        unsigned int length = curChunk.end - curChunk.start;

        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;
            if (range.box == this)
                break;

            startOffset += (range.endOffset - range.startOffset);
        }

        if (length > len())
            length = len();

        Vector<SVGChar>::iterator itChar = curChunk.start + startOffset;
        Vector<SVGChar>::iterator itCharEnd = curChunk.start + startOffset + length;

        if (itCharEnd > curChunk.end)
            itCharEnd = curChunk.end;

        for (; itChar != itCharEnd; ++itChar) {
            float glyphWidth = font.floatWidth(TextRun(textObject()->characters() + startOffset, 1), TextStyle(0, 0));
            float glyphHeight = font.ascent() + font.descent();

            float xDistance = (*itChar).x + glyphWidth / 2.0 - x;
            float yDistance = (*itChar).y - glyphHeight / 2.0 - y;

            float newDistance = sqrt(xDistance * xDistance + yDistance * yDistance);
            if (newDistance <= distance) {
                distance = newDistance;
                offset = itChar - curChunk.start;
                chunkOffset = (it - chunks.begin()) - startOffset;
                chunk = curChunk;
            }
        }
    }

    if (!chunk.start) {
        offset = 0;
        return 0;
    }

    SVGChar* result = chunk.start + offset;
    offset += chunkOffset;

    return result;
}

bool SVGInlineTextBox::svgCharacterHitsPosition(int x, int y, int& offset) const
{
    SVGChar* charAtPosPtr = closestCharacterToPosition(x, y, offset);
    if (!charAtPosPtr)
        return false;

    SVGChar& charAtPos = *charAtPosPtr;
    RenderStyle* style = textObject()->style(m_firstLine);

    // Check wheter y position hits the current character
    float glyphHeight = style->font().ascent() + style->font().descent();

    // TODO: selection logic must depend on the text direction (ie. top-to-bottom vertical text)
    // In vertical text the selection logic tricks must be done here and not below!
    if (y < charAtPos.y - glyphHeight || y > charAtPos.y)
        return false;

    // Check wheter x position hits the current character
    float glyphWidth = style->font().floatWidth(TextRun(textObject()->text()->characters() + offset, 1), TextStyle(0, 0));
    if (x < charAtPos.x || x > charAtPos.x + glyphWidth)
        return false;

    // Snap to character at half of it's advance
    if (x >= charAtPos.x + glyphWidth / 2.0)
        offset++;

    return true;
}

int SVGInlineTextBox::offsetForPosition(int x, bool includePartialGlyphs) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

int SVGInlineTextBox::positionForOffset(int offset) const
{
    // SVG doesn't use the offset <-> position selection system. 
    ASSERT_NOT_REACHED();
    return 0;
}

bool SVGInlineTextBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    ASSERT(!isLineBreak());

    if (object()->style()->visibility() != VISIBLE)
       return false;

    int offset = 0;
    if (!svgCharacterHitsPosition(x, y, offset))
        return false;

    object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
    return true;
}

IntRect SVGInlineTextBox::selectionRect(int tx, int ty, int startPos, int endPos)
{
    if (startPos >= endPos)
        return IntRect();

    SVGRootInlineBox* rootBox = svgRootInlineBox();

    // Find corresponding text chunk for our inline box
    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator end = chunks.end();

    float width = 0.0;
    float beforeWidth = 0.0;

    for (; it != end; ++it) {
        SVGTextChunk& chunk = *it;

        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = chunk.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = chunk.boxes.end();

        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;
            if (range.box != this)
                continue;

            float newWidth = rootBox->cummulatedWidthOfSelectionRange(this, startPos, endPos, len(), 0);
            if (newWidth != FLT_MAX) {
                width += newWidth;

                if (startPos > 0) {
                    newWidth = rootBox->cummulatedWidthOfSelectionRange(this, 0, startPos, len(), 0);
                    if (newWidth != FLT_MAX)
                        beforeWidth += newWidth;
                }
            }    
        }
    }

    RenderStyle* style = textObject()->style(m_firstLine);
    ASSERT(style);

    float height = style->font().ascent() + style->font().descent();
    return enclosingIntRect(FloatRect(tx + beforeWidth + xPos(), ty + yPos(), width, height));
}

} // namespace WebCore
