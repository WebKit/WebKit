/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGTextChunkLayoutInfo.h"

// FIXME: This code is currently deactivated, until the SVG Text rewrite patch lands.
#if ENABLE(SVG) && 0
#include "InlineFlowBox.h"
#include "SVGInlineTextBox.h"
#include "SVGRenderStyle.h"

// Text chunk creation is complex and the whole process
// can easily be traced by setting this variable > 0.
#define DEBUG_CHUNK_BUILDING 0

namespace WebCore {

static float cummulatedWidthOrHeightOfTextChunk(SVGTextChunk& chunk, bool calcWidthOnly)
{
    float length = 0.0f;
    Vector<SVGChar>::iterator charIt = chunk.start;

    Vector<SVGInlineBoxCharacterRange>::iterator it = chunk.boxes.begin();
    Vector<SVGInlineBoxCharacterRange>::iterator end = chunk.boxes.end();

    for (; it != end; ++it) {
        SVGInlineBoxCharacterRange& range = *it;

        SVGInlineTextBox* box = static_cast<SVGInlineTextBox*>(range.box);
        RenderStyle* style = box->renderer()->style();

        for (int i = range.startOffset; i < range.endOffset; ++i) {
            ASSERT(charIt <= chunk.end);

            // Determine how many characters - starting from the current - can be measured at once.
            // Important for non-absolute positioned non-latin1 text (ie. Arabic) where ie. the width
            // of a string is not the sum of the boundaries of all contained glyphs.
            Vector<SVGChar>::iterator itSearch = charIt + 1;
            Vector<SVGChar>::iterator endSearch = charIt + range.endOffset - i;
            while (itSearch != endSearch) {
                // No need to check for 'isHidden()' here as this function is not called for text paths.
                if (itSearch->drawnSeperated)
                    break;

                itSearch++;
            }

            unsigned int positionOffset = itSearch - charIt;

            // Calculate width/height of subrange
            SVGInlineBoxCharacterRange subRange;
            subRange.box = range.box;
            subRange.startOffset = i;
            subRange.endOffset = i + positionOffset;

            if (calcWidthOnly)
                length += cummulatedWidthOfInlineBoxCharacterRange(subRange);
            else
                length += cummulatedHeightOfInlineBoxCharacterRange(subRange);

            // Calculate gap between the previous & current range
            // <text x="10 50 70">ABCD</text> - we need to take the gaps between A & B into account
            // so add "40" as width, and analogous for B & C, add "20" as width.
            if (itSearch > chunk.start && itSearch < chunk.end) {
                SVGChar& lastCharacter = *(itSearch - 1);
                SVGChar& currentCharacter = *itSearch;

                int charsConsumed = 0;
                float glyphWidth = 0.0f;
                float glyphHeight = 0.0f;
                String glyphName;
                String unicodeString;
                box->measureCharacter(style, i + positionOffset - 1, charsConsumed, glyphName, unicodeString, glyphWidth, glyphHeight);

                if (calcWidthOnly)
                    length += currentCharacter.x - lastCharacter.x - glyphWidth;
                else
                    length += currentCharacter.y - lastCharacter.y - glyphHeight;
            }

            // Advance processed characters
            i += positionOffset - 1;
            charIt = itSearch;
        }
    }

    ASSERT(charIt == chunk.end);
    return length;
}

static float cummulatedWidthOfTextChunk(SVGTextChunk& chunk)
{
    return cummulatedWidthOrHeightOfTextChunk(chunk, true);
}

static float cummulatedHeightOfTextChunk(SVGTextChunk& chunk)
{
    return cummulatedWidthOrHeightOfTextChunk(chunk, false);
}

float calculateTextAnchorShiftForTextChunk(SVGTextChunk& chunk, ETextAnchor anchor)
{
    float shift = 0.0f;

    if (chunk.isVerticalText)
        shift = cummulatedHeightOfTextChunk(chunk);
    else
        shift = cummulatedWidthOfTextChunk(chunk);

    if (anchor == TA_MIDDLE)
        shift *= -0.5f;
    else
        shift *= -1.0f;

    return shift;
}

static void applyTextAnchorToTextChunk(SVGTextChunk& chunk)
{
    // This method is not called for chunks containing chars aligned on a path.
    // -> all characters are visible, no need to check for "isHidden()" anywhere.

    if (chunk.anchor == TA_START)
        return;

    float shift = calculateTextAnchorShiftForTextChunk(chunk, chunk.anchor);

    // Apply correction to chunk
    Vector<SVGChar>::iterator chunkIt = chunk.start;
    for (; chunkIt != chunk.end; ++chunkIt) {
        SVGChar& curChar = *chunkIt;

        if (chunk.isVerticalText)
            curChar.y += shift;
        else
            curChar.x += shift;
    }

    // Move inline boxes
    Vector<SVGInlineBoxCharacterRange>::iterator boxIt = chunk.boxes.begin();
    Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = chunk.boxes.end();

    for (; boxIt != boxEnd; ++boxIt) {
        SVGInlineBoxCharacterRange& range = *boxIt;

        InlineBox* curBox = range.box;
        ASSERT(curBox->isSVGInlineTextBox());

        // Move target box
        if (chunk.isVerticalText)
            curBox->setY(curBox->y() + static_cast<int>(shift));
        else
            curBox->setX(curBox->x() + static_cast<int>(shift));
    }
}

float calculateTextLengthCorrectionForTextChunk(SVGTextChunk& chunk, ELengthAdjust lengthAdjust, float& computedLength)
{
    if (chunk.textLength <= 0.0f)
        return 0.0f;

    computedLength = 0.0f;

    float computedWidth = cummulatedWidthOfTextChunk(chunk);
    float computedHeight = cummulatedHeightOfTextChunk(chunk);
    if ((computedWidth <= 0.0f && !chunk.isVerticalText)
        || (computedHeight <= 0.0f && chunk.isVerticalText))
        return 0.0f;

    computedLength = chunk.isVerticalText ? computedHeight : computedWidth;
    if (lengthAdjust == SVGTextContentElement::LENGTHADJUST_SPACINGANDGLYPHS) {
        if (chunk.isVerticalText)    
            chunk.ctm.scaleNonUniform(1.0f, chunk.textLength / computedLength);
        else
            chunk.ctm.scaleNonUniform(chunk.textLength / computedLength, 1.0f);

        return 0.0f;
    }

    return (chunk.textLength - computedLength) / float(chunk.end - chunk.start);
}

static void applyTextLengthCorrectionToTextChunk(SVGTextChunk& chunk)
{
    // This method is not called for chunks containing chars aligned on a path.
    // -> all characters are visible, no need to check for "isHidden()" anywhere.

    // lengthAdjust="spacingAndGlyphs" is handled by setting a scale factor for the whole chunk
    float textLength = 0.0f;
    float spacingToApply = calculateTextLengthCorrectionForTextChunk(chunk, chunk.lengthAdjust, textLength);

    if (!chunk.ctm.isIdentity() && chunk.lengthAdjust == SVGTextContentElement::LENGTHADJUST_SPACINGANDGLYPHS) {
        SVGChar& firstChar = *(chunk.start);

        // Assure we apply the chunk scaling in the right origin
        AffineTransform newChunkCtm(chunk.ctm);
        newChunkCtm.translateRight(firstChar.x, firstChar.y);
        newChunkCtm.translate(-firstChar.x, -firstChar.y);

        chunk.ctm = newChunkCtm;
    }

    // Apply correction to chunk 
    if (spacingToApply != 0.0f) {
        Vector<SVGChar>::iterator chunkIt = chunk.start;
        for (; chunkIt != chunk.end; ++chunkIt) {
            SVGChar& curChar = *chunkIt;
            curChar.drawnSeperated = true;

            if (chunk.isVerticalText)
                curChar.y += (chunkIt - chunk.start) * spacingToApply;
            else
                curChar.x += (chunkIt - chunk.start) * spacingToApply;
        }
    }
}

void SVGTextChunkLayoutInfo::startTextChunk()
{
    m_chunk.boxes.clear();
    m_chunk.boxes.append(SVGInlineBoxCharacterRange());

    m_chunk.start = m_charsIt;
    m_assignChunkProperties = true;
}

void SVGTextChunkLayoutInfo::closeTextChunk()
{
    ASSERT(!m_chunk.boxes.last().isOpen());
    ASSERT(m_chunk.boxes.last().isClosed());

    m_chunk.end = m_charsIt;
    ASSERT(m_chunk.end >= m_chunk.start);

    m_svgTextChunks.append(m_chunk);
}

void SVGTextChunkLayoutInfo::buildTextChunks(Vector<SVGChar>::iterator begin, Vector<SVGChar>::iterator end, InlineFlowBox* start)
{
    m_charsBegin = begin;
    m_charsEnd = end;

    m_charsIt = begin;
    m_chunk = SVGTextChunk(begin);

    recursiveBuildTextChunks(start);
    ASSERT(m_charsIt == m_charsEnd);
}

void SVGTextChunkLayoutInfo::recursiveBuildTextChunks(InlineFlowBox* start)
{
#if DEBUG_CHUNK_BUILDING > 1
    fprintf(stderr, " -> buildTextChunks(start=%p)\n", start);
#endif

    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isText()) {
            InlineTextBox* textBox = static_cast<InlineTextBox*>(curr);

            unsigned length = textBox->len();
            if (!length)
                continue;

#if DEBUG_CHUNK_BUILDING > 1
            fprintf(stderr, " -> Handle inline text box (%p) with %i characters (start: %i, end: %i), handlingTextPath=%i\n",
                            textBox, length, textBox->start(), textBox->end(), (int) m_handlingTextPath);
#endif

            RenderText* text = textBox->textRenderer();
            ASSERT(text);
            ASSERT(text->node());

            SVGTextContentElement* textContent = 0;
            Node* node = text->node()->parent();
            while (node && node->isSVGElement() && !textContent) {
                if (static_cast<SVGElement*>(node)->isTextContent())
                    textContent = static_cast<SVGTextContentElement*>(node);
                else
                    node = node->parentNode();
            }
            ASSERT(textContent);

            // Start new character range for the first chunk
            bool isFirstCharacter = m_svgTextChunks.isEmpty() && m_chunk.start == m_charsIt && m_chunk.start == m_chunk.end;
            if (isFirstCharacter) {
                ASSERT(m_chunk.boxes.isEmpty());
                m_chunk.boxes.append(SVGInlineBoxCharacterRange());
            } else
                ASSERT(!m_chunk.boxes.isEmpty());

            // Walk string to find out new chunk positions, if existent
            for (unsigned i = 0; i < length; ++i) {
                ASSERT(m_charsIt != m_charsEnd);

                SVGInlineBoxCharacterRange& range = m_chunk.boxes.last();
                if (range.isOpen()) {
                    range.box = curr;
                    range.startOffset = !i ? 0 : i - 1;

#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " | -> Range is open! box=%p, startOffset=%i\n", range.box, range.startOffset);
#endif
                }

                // If a new (or the first) chunk has been started, record it's text-anchor and writing mode.
                if (m_assignChunkProperties) {
                    m_assignChunkProperties = false;

                    m_chunk.isVerticalText = isVerticalWritingMode(text->style()->svgStyle());
                    m_chunk.isTextPath = m_handlingTextPath;
                    m_chunk.anchor = text->style()->svgStyle()->textAnchor();
                    m_chunk.textLength = textContent->textLength().value(textContent);
                    m_chunk.lengthAdjust = (ELengthAdjust) textContent->lengthAdjust();

#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " | -> Assign chunk properties, isVerticalText=%i, anchor=%i\n", m_chunk.isVerticalText, m_chunk.anchor);
#endif
                }

                if (i > 0 && !isFirstCharacter && m_charsIt->newTextChunk) {
                    // Close mid chunk & character range
                    ASSERT(!range.isOpen());
                    ASSERT(!range.isClosed());

                    range.endOffset = i;
                    closeTextChunk();

#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " | -> Close mid-text chunk, at endOffset: %i and starting new mid chunk!\n", range.endOffset);
#endif
    
                    // Prepare for next chunk, if we're not at the end
                    startTextChunk();
                    if (i + 1 == length) {
#if DEBUG_CHUNK_BUILDING > 1
                        fprintf(stderr, " | -> Record last chunk of inline text box!\n");
#endif

                        startTextChunk();
                        SVGInlineBoxCharacterRange& range = m_chunk.boxes.last();

                        m_assignChunkProperties = false;
                        m_chunk.isVerticalText = isVerticalWritingMode(text->style()->svgStyle());
                        m_chunk.isTextPath = m_handlingTextPath;
                        m_chunk.anchor = text->style()->svgStyle()->textAnchor();
                        m_chunk.textLength = textContent->textLength().value(textContent);
                        m_chunk.lengthAdjust = (ELengthAdjust) textContent->lengthAdjust();

                        range.box = curr;
                        range.startOffset = i;

                        ASSERT(!range.isOpen());
                        ASSERT(!range.isClosed());
                    }
                }

                // This should only hold true for the first character of the first chunk
                if (isFirstCharacter)
                    isFirstCharacter = false;
    
                ++m_charsIt;
            }

#if DEBUG_CHUNK_BUILDING > 1    
            fprintf(stderr, " -> Finished inline text box!\n");
#endif

            SVGInlineBoxCharacterRange& range = m_chunk.boxes.last();
            if (!range.isOpen() && !range.isClosed()) {
#if DEBUG_CHUNK_BUILDING > 1
                fprintf(stderr, " -> Last range not closed - closing with endOffset: %i\n", length);
#endif

                // Current text chunk is not yet closed. Finish the current range, but don't start a new chunk.
                range.endOffset = length;

                if (m_charsIt != m_charsEnd) {
#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " -> Not at last character yet!\n");
#endif

                    // If we're not at the end of the last box to be processed, and if the next
                    // character starts a new chunk, then close the current chunk and start a new one.
                    if (m_charsIt->newTextChunk) {
#if DEBUG_CHUNK_BUILDING > 1
                        fprintf(stderr, " -> Next character starts new chunk! Closing current chunk, and starting a new one...\n");
#endif

                        closeTextChunk();
                        startTextChunk();
                    } else {
                        // Just start a new character range
                        m_chunk.boxes.append(SVGInlineBoxCharacterRange());

#if DEBUG_CHUNK_BUILDING > 1
                        fprintf(stderr, " -> Next character does NOT start a new chunk! Starting new character range...\n");
#endif
                    }
                } else {
#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " -> Closing final chunk! Finished processing!\n");
#endif

                    // Close final chunk, once we're at the end of the last box
                    closeTextChunk();
                }
            }
        } else {
            ASSERT(curr->isInlineFlowBox());
            InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(curr);

            // Skip generated content.
            if (!flowBox->renderer()->node())
                continue;

            bool isTextPath = flowBox->renderer()->node()->hasTagName(SVGNames::textPathTag);

#if DEBUG_CHUNK_BUILDING > 1
            fprintf(stderr, " -> Handle inline flow box (%p), isTextPath=%i\n", flowBox, (int) isTextPath);
#endif

            if (isTextPath)
                m_handlingTextPath = true;

            recursiveBuildTextChunks(flowBox);

            if (isTextPath)
                m_handlingTextPath = false;
        }
    }

#if DEBUG_CHUNK_BUILDING > 1
    fprintf(stderr, " <- buildTextChunks(start=%p)\n", start);
#endif
}

void SVGTextChunkLayoutInfo::layoutTextChunks()
{
    Vector<SVGTextChunk>::iterator it = m_svgTextChunks.begin();
    Vector<SVGTextChunk>::iterator end = m_svgTextChunks.end();

    for (; it != end; ++it) {
        SVGTextChunk& chunk = *it;

#if DEBUG_CHUNK_BUILDING > 0
        {
            fprintf(stderr, "Handle TEXT CHUNK! anchor=%i, textLength=%f, lengthAdjust=%i, isVerticalText=%i, isTextPath=%i start=%p, end=%p -> dist: %i\n",
                    (int) chunk.anchor, chunk.textLength, (int) chunk.lengthAdjust, (int) chunk.isVerticalText,
                    (int) chunk.isTextPath, chunk.start, chunk.end, (unsigned int) (chunk.end - chunk.start));

            Vector<SVGInlineBoxCharacterRange>::iterator boxIt = chunk.boxes.begin();
            Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = chunk.boxes.end();

            unsigned int i = 0;
            for (; boxIt != boxEnd; ++boxIt) {
                SVGInlineBoxCharacterRange& range = *boxIt;
                ++i;
                fprintf(stderr, " -> RANGE %i STARTOFFSET: %i, ENDOFFSET: %i, BOX: %p\n", i, range.startOffset, range.endOffset, range.box);
            }
        }
#endif

        if (chunk.isTextPath)
            continue;

        // text-path & textLength, with lengthAdjust="spacing" is already handled for textPath layouts.
        applyTextLengthCorrectionToTextChunk(chunk);
 
        // text-anchor is already handled for textPath layouts.
        applyTextAnchorToTextChunk(chunk);
    }
}

} // namespace WebCore

#endif // ENABLE(SVG)
