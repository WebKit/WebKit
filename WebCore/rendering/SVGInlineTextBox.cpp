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
#include "RenderSVGRoot.h"
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

float SVGInlineTextBox::calculateGlyphWidth(RenderStyle* style, int offset) const
{
    return style->font().floatWidth(TextRun(textObject()->text()->characters() + offset, 1), svgTextStyleForInlineTextBox(style, this, 0));
}

float SVGInlineTextBox::calculateGlyphHeight(RenderStyle* style, int offset) const
{
    ASSERT(style);

    // This is just a guess, and the only purpose of this function is to centralize this hack.
    // In real-life top-top-bottom scripts this won't be enough, I fear.
    return style->font().ascent() + style->font().descent();
}

FloatRect SVGInlineTextBox::calculateGlyphBoundaries(RenderStyle* style, int offset, const SVGChar& svgChar) const
{
    const Font& font = style->font();

    // Take RTL text into account and pick right glyph width/height.
    float glyphWidth = 0.0;

    if (!m_reversed)
        glyphWidth = calculateGlyphWidth(style, offset);
    else
        glyphWidth = calculateGlyphWidth(style, start() + end() - offset);

    float x1 = svgChar.x;
    float x2 = svgChar.x + glyphWidth;

    float y1 = svgChar.y - font.ascent();
    float y2 = svgChar.y + font.descent();

    FloatRect glyphRect(x1, y1, x2 - x1, y2 - y1);

    // Take per-character transformations into account
    if (!svgChar.transform.isIdentity())
        glyphRect = svgChar.transform.mapRect(glyphRect);

    return glyphRect;
}

SVGChar* SVGInlineTextBox::closestCharacterToPosition(int x, int y, int& offset) const
{
    // Find corresponding text chunk for our inline box & reference x position
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    // Iterate through the characters, respecting their individual placement
    // Find the character within the chunk with the smallest diagonal distance
    // to the current position. Check whether the passed x value hits that character.
    Vector<SVGChar>::iterator character = 0;
    float distance = FLT_MAX;

    RenderStyle* style = textObject()->style();

    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator itEnd = chunks.end();

    for (; it != itEnd; ++it) {
        SVGTextChunk& curChunk = *it;

        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = curChunk.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = curChunk.boxes.end();

        unsigned int chunkOffset = 0;        
        unsigned int firstRangeInFirstChunkStartOffset = 0;
    
        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;

            if (boxIt == curChunk.boxes.begin())
                firstRangeInFirstChunkStartOffset = range.startOffset;

            if (range.box != this) {
                chunkOffset += range.endOffset - range.startOffset;
                continue;
            }

            Vector<SVGChar>::iterator closestCharacter = 0;
            unsigned int closestOffset = UINT_MAX;

            // Walk chunk finding closest character
            Vector<SVGChar>::iterator itCharBegin = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.startOffset;
            Vector<SVGChar>::iterator itCharEnd = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.endOffset;
            ASSERT(itCharEnd <= curChunk.end);

            for (Vector<SVGChar>::iterator itChar = itCharBegin; itChar != itCharEnd; ++itChar) {
                unsigned int newOffset = start() + (itChar - itCharBegin) + firstRangeInFirstChunkStartOffset;

                // Take RTL text into account and pick right glyph width/height.
                if (m_reversed)
                    newOffset = start() + end() - newOffset;

                float glyphWidth = calculateGlyphWidth(style, newOffset);
                float glyphHeight = calculateGlyphHeight(style, newOffset);

                // Calculate distances relative to the glyph mid-point. I hope this is accurate enough.
                float xDistance = (*itChar).x + glyphWidth / 2.0 - x;
                float yDistance = (*itChar).y - glyphHeight / 2.0 - y;

                float newDistance = sqrt(xDistance * xDistance + yDistance * yDistance);
                if (newDistance <= distance) {
                    distance = newDistance;
                    closestOffset = newOffset;
                    closestCharacter = itChar;
                }
            }

            chunkOffset += range.endOffset - range.startOffset;
            if (closestOffset == UINT_MAX)
                continue;

            // Record current chunk, if it contains the current closest character next to the mouse.
            character = closestCharacter;
            offset = closestOffset;
        }
    }

    if (!character) {
        offset = 0;
        return 0;
    }

    return character;
}

bool SVGInlineTextBox::svgCharacterHitsPosition(int x, int y, int& offset) const
{
    SVGChar* charAtPosPtr = closestCharacterToPosition(x, y, offset);
    if (!charAtPosPtr)
        return false;

    SVGChar& charAtPos = *charAtPosPtr;
    RenderStyle* style = textObject()->style(m_firstLine);

    float glyphWidth = calculateGlyphWidth(style, offset);
    float glyphHeight = calculateGlyphHeight(style, offset);

    if (m_reversed)
        offset++;

    // FIXME: todo list
    // (#13910) This code does not handle bottom-to-top/top-to-bottom vertical text.

    // Check whether y position hits the current character 
    if (y < charAtPos.y - glyphHeight || y > charAtPos.y)
        return false;

    // Check whether x position hits the current character
    if (x < charAtPos.x) {
        if (offset > 0 && !m_reversed)
            return true;
        else if (offset < (int) end() && m_reversed)
            return true;

        return false;
    }

    // If we are past the last glyph of this box, don't mark it as 'hit' anymore.
    if (x >= charAtPos.x + glyphWidth && offset == (int) end())
        return false;

    // Snap to character at half of it's advance
    if (x >= charAtPos.x + glyphWidth / 2.0)
        offset += m_reversed ? -1 : 1;

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

    IntRect rect = selectionRect(0, 0, 0, len());
    if (object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
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

    // Find corresponding text chunk for our inline box & reference x position
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    RenderStyle* style = textObject()->style();
    FloatRect selectionRect;
    
    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator itEnd = chunks.end();

    for (; it != itEnd; ++it) {
        SVGTextChunk& curChunk = *it;

        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = curChunk.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = curChunk.boxes.end();

        unsigned int chunkOffset = 0;        
        unsigned int firstRangeInFirstChunkStartOffset = 0;
    
        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;

            if (boxIt == curChunk.boxes.begin())
                firstRangeInFirstChunkStartOffset = range.startOffset;

            if (range.box != this) {
                chunkOffset += range.endOffset - range.startOffset;
                continue;
            }

            // Walk chunk finding closest character
            Vector<SVGChar>::iterator itCharBegin = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.startOffset;
            Vector<SVGChar>::iterator itCharEnd = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.endOffset;
            ASSERT(itCharEnd <= curChunk.end);

            for (Vector<SVGChar>::iterator itChar = itCharBegin; itChar != itCharEnd; ++itChar) {
                unsigned int newOffset = start() + (itChar - itCharBegin) + firstRangeInFirstChunkStartOffset;

                int offset = m_reversed ? start() + end() - newOffset : newOffset;
                selectionRect.unite(calculateGlyphBoundaries(style, offset, *itChar));
            }

            chunkOffset += range.endOffset - range.startOffset;
        }
    }

    return enclosingIntRect(selectionRect);
}

} // namespace WebCore
