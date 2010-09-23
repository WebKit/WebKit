/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
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
#include "SVGRootInlineBox.h"

#if ENABLE(SVG)
#include "GraphicsContext.h"
#include "RenderBlock.h"
#include "SVGInlineFlowBox.h"
#include "SVGInlineTextBox.h"
#include "SVGRenderSupport.h"
#include "SVGTextLayoutUtilities.h"
#include "SVGTextPositioningElement.h"

// Text chunk part propagation can be traced by setting this variable > 0.
#define DEBUG_CHUNK_PART_PROPAGATION 0

namespace WebCore {

void SVGRootInlineBox::paint(PaintInfo& paintInfo, int, int)
{
    ASSERT(paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection);
    ASSERT(!paintInfo.context->paintingDisabled());

    RenderObject* boxRenderer = renderer();
    ASSERT(boxRenderer);

    PaintInfo childPaintInfo(paintInfo);
    childPaintInfo.context->save();

    if (SVGRenderSupport::prepareToRenderSVGContent(boxRenderer, childPaintInfo)) {
        for (InlineBox* child = firstChild(); child; child = child->nextOnLine())
            child->paint(childPaintInfo, 0, 0);
    }

    SVGRenderSupport::finishRenderSVGContent(boxRenderer, childPaintInfo, paintInfo.context);
    childPaintInfo.context->restore();
}

void SVGRootInlineBox::computePerCharacterLayoutInformation()
{
    // Clean up any previous layout information
    m_svgChars.clear();
    m_svgTextChunks.clear();

    // Build layout information for all contained render objects
    SVGCharacterLayoutInfo charInfo;
    buildLayoutInformation(this, charInfo);
    m_svgChars = charInfo.svgChars;

    // Now all layout information are available for every character
    // contained in any of our child inline/flow boxes. Build list
    // of text chunks now, to be able to apply text-anchor shifts.
    SVGTextChunkLayoutInfo chunkInfo;
    chunkInfo.buildTextChunks(m_svgChars.begin(), m_svgChars.end(), this);

    // Layout all text chunks
    // text-anchor needs to be applied to individual chunks.
    chunkInfo.layoutTextChunks();
    m_svgTextChunks = chunkInfo.textChunks();

    // Propagate text chunk part information to all SVGInlineTextBoxes, see SVGTextChunkLayoutInfo.h for details
    propagateTextChunkPartInformation();

    // Layout all child boxes.
    layoutChildBoxes(this);

    // Resize our root box and our RenderSVGText parent block
    layoutRootBox();
}

void SVGRootInlineBox::buildLayoutInformation(InlineFlowBox* start, SVGCharacterLayoutInfo& info)
{
    if (start->isRootInlineBox()) {
        ASSERT(start->renderer()->node()->hasTagName(SVGNames::textTag));

        SVGTextPositioningElement* positioningElement = static_cast<SVGTextPositioningElement*>(start->renderer()->node());
        ASSERT(positioningElement);
        ASSERT(positioningElement->parentNode());

        info.addLayoutInformation(positioningElement);
    }

    SVGLastGlyphInfo lastGlyph;
    
    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->renderer()->isText())
            static_cast<SVGInlineTextBox*>(curr)->buildLayoutInformation(info, lastGlyph);
        else {
            ASSERT(curr->isInlineFlowBox());
            InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(curr);

            // Skip generated content.
            if (!flowBox->renderer()->node())
                continue;

            bool isAnchor = flowBox->renderer()->node()->hasTagName(SVGNames::aTag);
            bool isTextPath = flowBox->renderer()->node()->hasTagName(SVGNames::textPathTag);

            if (!isTextPath && !isAnchor) {
                SVGTextPositioningElement* positioningElement = static_cast<SVGTextPositioningElement*>(flowBox->renderer()->node());
                ASSERT(positioningElement);
                ASSERT(positioningElement->parentNode());

                info.addLayoutInformation(positioningElement);
            } else if (!isAnchor) {
                info.setInPathLayout(true);

                // Handle text-anchor/textLength on path, which is special.
                SVGTextContentElement* textContent = 0;
                Node* node = flowBox->renderer()->node();
                if (node && node->isSVGElement())
                    textContent = static_cast<SVGTextContentElement*>(node);
                ASSERT(textContent);

                ELengthAdjust lengthAdjust = (ELengthAdjust) textContent->lengthAdjust();
                ETextAnchor anchor = flowBox->renderer()->style()->svgStyle()->textAnchor();
                float textAnchorStartOffset = 0.0f;

                // Initialize sub-layout. We need to create text chunks from the textPath
                // children using our standard layout code, to be able to measure the
                // text length using our normal methods and not textPath specific hacks.
                Vector<SVGTextChunk> tempChunks;

                SVGCharacterLayoutInfo tempCharInfo;
                buildLayoutInformation(flowBox, tempCharInfo);

                SVGTextChunkLayoutInfo tempChunkInfo;
                tempChunkInfo.buildTextChunks(tempCharInfo.svgChars.begin(), tempCharInfo.svgChars.end(), flowBox);
                tempChunks = tempChunkInfo.textChunks();

                Vector<SVGTextChunk>::iterator it = tempChunks.begin();
                Vector<SVGTextChunk>::iterator end = tempChunks.end();

                float computedLength = 0.0f;
 
                for (; it != end; ++it) {
                    SVGTextChunk& chunk = *it;

                    // Apply text-length calculation
                    info.pathExtraAdvance += calculateTextLengthCorrectionForTextChunk(chunk, lengthAdjust, computedLength);

                    if (lengthAdjust == SVGTextContentElement::LENGTHADJUST_SPACINGANDGLYPHS) {
                        info.pathTextLength += computedLength;
                        info.pathChunkLength += chunk.textLength;
                    }

                    // Calculate text-anchor start offset
                    if (anchor == TA_START)
                        continue;

                    textAnchorStartOffset += calculateTextAnchorShiftForTextChunk(chunk, anchor);
                }

                info.addLayoutInformation(flowBox, textAnchorStartOffset);
            }

            float shiftxSaved = info.shiftx;
            float shiftySaved = info.shifty;

            buildLayoutInformation(flowBox, info);
            info.processedChunk(shiftxSaved, shiftySaved);

            if (isTextPath)
                info.setInPathLayout(false);
        }
    }
}

void SVGRootInlineBox::layoutChildBoxes(InlineFlowBox* start)
{
    for (InlineBox* child = start->firstChild(); child; child = child->nextOnLine()) {
        if (child->renderer()->isText()) {
            SVGInlineTextBox* textBox = static_cast<SVGInlineTextBox*>(child);
            IntRect boxRect = textBox->calculateBoundaries();
            textBox->setX(boxRect.x());
            textBox->setY(boxRect.y());
            textBox->setLogicalWidth(boxRect.width());
            textBox->setLogicalHeight(boxRect.height());
        } else {
            ASSERT(child->isInlineFlowBox());
   
            // Skip generated content.
            if (!child->renderer()->node())
                continue;

            SVGInlineFlowBox* flowBox = static_cast<SVGInlineFlowBox*>(child);
            layoutChildBoxes(flowBox);

            IntRect boxRect = flowBox->calculateBoundaries();
            flowBox->setX(boxRect.x());
            flowBox->setY(boxRect.y());
            flowBox->setLogicalWidth(boxRect.width());
            flowBox->setLogicalHeight(boxRect.height());
        }
    }
}

void SVGRootInlineBox::layoutRootBox()
{
    RenderBlock* parentBlock = block();
    ASSERT(parentBlock);

    IntRect childRect;
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        // Skip generated content.
        if (!child->renderer()->node())
            continue;
        childRect.unite(child->calculateBoundaries());
    }

    int xBlock = childRect.x();
    int yBlock = childRect.y();
    int widthBlock = childRect.width();
    int heightBlock = childRect.height();

    // Finally, assign the root block position, now that all content is laid out.
    parentBlock->setLocation(xBlock, yBlock);
    parentBlock->setWidth(widthBlock);
    parentBlock->setHeight(heightBlock);

    // Position all children relative to the parent block.
    for (InlineBox* child = firstChild(); child; child = child->nextOnLine()) {
        // Skip generated content.
        if (!child->renderer()->node())
            continue;
        child->adjustPosition(-xBlock, -yBlock);
    }

    // Position ourselves.
    setX(0);
    setY(0);
    setLogicalWidth(widthBlock);
    setLogicalHeight(heightBlock);
    setBlockHeight(heightBlock);
    setLineTopBottomPositions(0, heightBlock);
}

void SVGRootInlineBox::propagateTextChunkPartInformation()
{
#if DEBUG_CHUNK_PART_PROPAGATION > 0
    ListHashSet<SVGInlineTextBox*> boxes;
#endif

    // Loop through all text chunks
    const Vector<SVGTextChunk>::const_iterator end = m_svgTextChunks.end();
    for (Vector<SVGTextChunk>::const_iterator it = m_svgTextChunks.begin(); it != end; ++it) {
        const SVGTextChunk& chunk = *it;
        int processedChunkCharacters = 0;

        // Loop through all ranges contained in this chunk
        const Vector<SVGInlineBoxCharacterRange>::const_iterator boxEnd = chunk.boxes.end();
        for (Vector<SVGInlineBoxCharacterRange>::const_iterator boxIt = chunk.boxes.begin(); boxIt != boxEnd; ++boxIt) {
            const SVGInlineBoxCharacterRange& range = *boxIt;
            ASSERT(range.box->isSVGInlineTextBox());

            // Access style & font information of this text box
            SVGInlineTextBox* rangeTextBox = static_cast<SVGInlineTextBox*>(range.box);
            rangeTextBox->setChunkTransformation(chunk.ctm);

            RenderText* text = rangeTextBox->textRenderer();
            ASSERT(text);

            RenderStyle* style = text->style();
            ASSERT(style);

            const Font& font = style->font();
 
            // Figure out first and last character of this range in this chunk
            int rangeLength = range.endOffset - range.startOffset;
            Vector<SVGChar>::iterator itCharBegin = chunk.start + processedChunkCharacters;
            Vector<SVGChar>::iterator itCharEnd = chunk.start + processedChunkCharacters + rangeLength;
            ASSERT(itCharEnd <= chunk.end);

            // Loop through all characters in range 
            int processedRangeCharacters = 0;
            for (Vector<SVGChar>::iterator itChar = itCharBegin; itChar != itCharEnd; ++itChar) {
                if (itChar->isHidden()) {
                    ++processedRangeCharacters;
                    continue;
                }

                // Determine how many characters - starting from the current - can be drawn at once.
                Vector<SVGChar>::iterator itSearch = itChar + 1;
                while (itSearch != itCharEnd) {
                    if (itSearch->drawnSeperated || itSearch->isHidden())
                        break;

                    ++itSearch;
                }

                // Calculate text chunk part information for this chunk sub-range
                const UChar* partStart = text->characters() + rangeTextBox->start() + range.startOffset + processedRangeCharacters;

                SVGTextChunkPart part;
                part.firstCharacter = itChar;
                part.length = itSearch - itChar;
                part.width = font.floatWidth(svgTextRunForInlineTextBox(partStart, part.length, style, rangeTextBox));
                part.height = font.height();
                part.offset = range.startOffset + processedRangeCharacters;
                rangeTextBox->addChunkPartInformation(part);
                processedRangeCharacters += part.length;

                // Skip processed characters
                itChar = itSearch - 1;
            }

            ASSERT(processedRangeCharacters == rangeLength);
            processedChunkCharacters += rangeLength;

#if DEBUG_CHUNK_PART_PROPAGATION > 0
            boxes.add(rangeTextBox);
#endif
        }
    }

#if DEBUG_CHUNK_PART_PROPAGATION > 0
    {
        fprintf(stderr, "Propagated text chunk part information:\n");

        ListHashSet<SVGInlineTextBox*>::const_iterator it = boxes.begin();
        const ListHashSet<SVGInlineTextBox*>::const_iterator end = boxes.end();

        for (; it != end; ++it) {
            const SVGInlineTextBox* box = *it;
            const Vector<SVGTextChunkPart>& parts = box->svgTextChunkParts();

            fprintf(stderr, " Box %p contains %i text chunk parts:\n", box, static_cast<int>(parts.size()));
            Vector<SVGTextChunkPart>::const_iterator partIt = parts.begin();
            const Vector<SVGTextChunkPart>::const_iterator partEnd = parts.end();
            for (; partIt != partEnd; ++partIt) {
                const SVGTextChunkPart& part = *partIt;
                fprintf(stderr, "   -> firstCharacter x=%lf, y=%lf, offset=%i, length=%i, width=%lf, height=%lf, textRenderer=%p\n"
                              , part.firstCharacter->x, part.firstCharacter->y, part.offset, part.length, part.width, part.height, box->textRenderer());
            }
        }
    }
#endif
}

} // namespace WebCore

#endif // ENABLE(SVG)
