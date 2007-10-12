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

SVGChar* SVGInlineTextBox::closestCharacterToPosition(int x, int y, int& offset) const
{
    // Find corresponding text chunk for our inline box & reference x position
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    // Iterate through the characters, respecting their individual placement
    // Find the character within the chunk with the smallest diagonal distance
    // to the current position. Check whether the passed x value hits that character.
    SVGTextChunk chunk;
    float distance = FLT_MAX;

    const Font& font = textObject()->style()->font();

    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator end = chunks.end();

    for (; it != end; ++it) {
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

            unsigned int closestOffset = UINT_MAX;

            // Walk chunk finding closest character
            Vector<SVGChar>::iterator itCharBegin = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.startOffset;
            Vector<SVGChar>::iterator itCharEnd = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.endOffset;
            ASSERT(itCharEnd <= curChunk.end);

            for (Vector<SVGChar>::iterator itChar = itCharBegin; itChar != itCharEnd; ++itChar) {
                unsigned int newOffset = (itChar - itCharBegin) + firstRangeInFirstChunkStartOffset;

                float glyphWidth = font.floatWidth(TextRun(textObject()->text()->characters() + newOffset, 1), TextStyle(0, 0));
                float glyphHeight = font.ascent() + font.descent();

                // Calculate distances relative to the glyph mid-point. I hope this is accurate enough.
                float xDistance = (*itChar).x + glyphWidth / 2.0 - x;
                float yDistance = (*itChar).y - glyphHeight / 2.0 - y;

                float newDistance = sqrt(xDistance * xDistance + yDistance * yDistance);
                if (newDistance <= distance) {
                    distance = newDistance;
                    closestOffset = newOffset;
                }
            }

            chunkOffset += range.endOffset - range.startOffset;
            if (closestOffset == UINT_MAX)
                continue;

            // Record current chunk, if it contains the current closest character next to the mouse.
            chunk = curChunk;
            offset = closestOffset;
        }
    }

    if (!chunk.start) {
        offset = 0;
        return 0;
    }

    // Be very careful here! The chunk.start offset already takes the first range's
    // startOffset into account! Need to subtract it here.
    ASSERT(!chunk.boxes.isEmpty());
    Vector<SVGInlineBoxCharacterRange>::iterator boxStart = chunk.boxes.begin();

    return (chunk.start + offset - (*boxStart).startOffset);
}

bool SVGInlineTextBox::svgCharacterHitsPosition(int x, int y, int& offset) const
{
    SVGChar* charAtPosPtr = closestCharacterToPosition(x, y, offset);
    if (!charAtPosPtr)
        return false;

    SVGChar& charAtPos = *charAtPosPtr;
    RenderStyle* style = textObject()->style(m_firstLine);

    float glyphHeight = style->font().ascent() + style->font().descent();

    // FIXME: Several things need to be done:
    // (#13909) This code does not handle bottom-to-top vertical text (low priority).
    // (#13909) This code does not handle top-to-bottom vertical text (higher priority).
    // (#13010) This code does not handle right-to-left selection yet (highest priority).

    // Check whether y position hits the current character 
    if (y < charAtPos.y - glyphHeight || y > charAtPos.y)
        return false;

    // Check whether x position hits the current character
    float glyphWidth = style->font().floatWidth(TextRun(textObject()->text()->characters() + offset, 1), TextStyle(0, 0));
    if (x < charAtPos.x)
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

    RenderStyle* style = textObject()->style(m_firstLine);
    IntRect rect = selectionRect(0, -style->font().ascent(), 0, len());
    if (object()->style()->visibility() == VISIBLE && rect.contains(x, y)) {
        object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }

    return false;
}

IntRect SVGInlineTextBox::selectionRect(int tx, int ty, int startPos, int endPos)
{
    if (startPos >= endPos)
        return IntRect();

    // TODO: Actually respect startPos/endPos - we're returning the _full_ selectionRect
    // here. This won't lead to visible bugs, but to extra work being done. Investigate.
    
    // Find corresponding text chunk for our inline box & reference x position
    SVGRootInlineBox* rootBox = svgRootInlineBox();
    Vector<SVGTextChunk>& chunks = const_cast<Vector<SVGTextChunk>& >(rootBox->svgTextChunks());

    const Font& font = textObject()->style()->font();

    FloatRect selectionRect;
    
    Vector<SVGTextChunk>::iterator it = chunks.begin();
    Vector<SVGTextChunk>::iterator end = chunks.end();

    for (; it != end; ++it) {
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

            // Figure out chunk size
            Vector<SVGChar>::iterator itCharBegin = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.startOffset;
            Vector<SVGChar>::iterator itCharEnd = curChunk.start + chunkOffset - firstRangeInFirstChunkStartOffset + range.endOffset;
            ASSERT(itCharEnd <= curChunk.end);

            for (Vector<SVGChar>::iterator itChar = itCharBegin; itChar != itCharEnd; ++itChar) {
                unsigned int newOffset = (itChar - itCharBegin) + firstRangeInFirstChunkStartOffset;
                float glyphWidth = font.floatWidth(TextRun(textObject()->text()->characters() + newOffset, 1), TextStyle(0, 0));
 
                float x1 = (*itChar).x;
                float x2 = (*itChar).x + glyphWidth;

                float y1 = (*itChar).y - font.ascent();
                float y2 = (*itChar).y + font.descent();

                FloatRect glyphRect(x1, y1, x2 - x1, y2 - y1);

                // Take per-character transformations into account
                if (!(*itChar).transform.isIdentity())
                    glyphRect = (*itChar).transform.mapRect(glyphRect);

                selectionRect.unite(glyphRect);
            }
        }
    }

    return enclosingIntRect(selectionRect);
}

} // namespace WebCore
