/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 *           (C) 2006 Apple Computer Inc.
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
#include "RenderSVGInlineText.h"

#if ENABLE(SVG)
#include "SVGRootInlineBox.h"

#include "Editor.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "InlineTextBox.h"
#include "Range.h"
#include "RenderSVGRoot.h"
#include "SVGInlineFlowBox.h"
#include "SVGInlineTextBox.h"
#include "SVGPaintServer.h"
#include "SVGRenderSupport.h"
#include "SVGResourceClipper.h"
#include "SVGResourceFilter.h"
#include "SVGResourceMasker.h"
#include "SVGTextPositioningElement.h"
#include "SVGURIReference.h"
#include "Text.h"
#include "TextStyle.h"

// Text chunk creation is complex and the whole process
// can easily be traced by setting this variable > 0.
#define DEBUG_CHUNK_BUILDING 0

namespace WebCore {

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
static void prepareTextRendering(RenderObject::PaintInfo& paintInfo, int tx, int ty, InlineFlowBox* flowBox, FloatRect& boundingBox, SVGResourceFilter*& filter)
#else
static void prepareTextRendering(RenderObject::PaintInfo& paintInfo, int tx, int ty, InlineFlowBox* flowBox, FloatRect& boundingBox, void* filter)
#endif
{
    ASSERT(paintInfo.phase == PaintPhaseForeground);
    RenderObject* object = flowBox->object();

    paintInfo.context->save();
    paintInfo.context->concatCTM(object->localTransform());

    boundingBox = FloatRect(tx + flowBox->xPos(), ty + flowBox->yPos(), flowBox->width(), flowBox->height());

    prepareToRenderSVGContent(object, paintInfo, boundingBox, filter);
}

static inline bool isVerticalWritingMode(const SVGRenderStyle* style)
{
    return style->writingMode() == WM_TBRL || style->writingMode() == WM_TB; 
}

static inline void startTextChunk(SVGTextChunkLayoutInfo& info)
{
    info.chunk.boxes.clear();
    info.chunk.boxes.append(SVGInlineBoxCharacterRange());

    info.chunk.start = info.it;
    info.assignChunkProperties = true;
}

static inline void closeTextChunk(SVGTextChunkLayoutInfo& info, Vector<SVGTextChunk>& textChunks)
{
    ASSERT(!info.chunk.boxes.last().isOpen());
    ASSERT(info.chunk.boxes.last().isClosed());

    info.chunk.end = info.it;
    ASSERT(info.chunk.end >= info.chunk.start);

    textChunks.append(info.chunk);
}

RenderSVGRoot* findSVGRootObject(RenderObject* start)
{
    // Find associated root inline box
    while (start && !start->isSVGRoot())
        start = start->parent();

    ASSERT(start);
    ASSERT(start->isSVGRoot());

    return static_cast<RenderSVGRoot*>(start);
}

static inline FloatPoint topLeftPositionOfCharacterRange(Vector<SVGChar>& chars)
{
    return topLeftPositionOfCharacterRange(chars.begin(), chars.end());
}

FloatPoint topLeftPositionOfCharacterRange(Vector<SVGChar>::iterator it, Vector<SVGChar>::iterator end)
{
    float lowX = FLT_MAX, lowY = FLT_MAX;
    for (; it != end; ++it) {
        float x = (*it).x;
        float y = (*it).y;

        if (x < lowX)
            lowX = x;

        if (y < lowY)
            lowY = y;
    }

    return FloatPoint(lowX, lowY);
}

void SVGRootInlineBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.context->paintingDisabled() || paintInfo.phase != PaintPhaseForeground)
        return;

    FloatRect boundingBox;

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    SVGResourceFilter* filter = 0;
#else
    void* filter = 0;
#endif
    prepareTextRendering(paintInfo, tx, ty, this, boundingBox, filter);

    RenderObject::PaintInfo pi(paintInfo);

    float opacity = object()->style()->opacity();
    if (opacity < 1.0f) {
        paintInfo.context->clip(enclosingIntRect(boundingBox));
        paintInfo.context->beginTransparencyLayer(opacity);
    }

    SVGPaintServer* fillPaintServer = SVGPaintServer::fillPaintServer(object()->style(), object());
    if (fillPaintServer) {
        if (fillPaintServer->setup(pi.context, object(), ApplyToFillTargetType, true)) {
            Vector<SVGChar>::iterator it = m_svgChars.begin();
            paintInlineBoxes(pi, tx, ty, this, it);
            ASSERT(it == m_svgChars.end());

            fillPaintServer->teardown(pi.context, object(), ApplyToFillTargetType, true);
        }
    }

    SVGPaintServer* strokePaintServer = SVGPaintServer::strokePaintServer(object()->style(), object());
    if (strokePaintServer) {
        if (strokePaintServer->setup(pi.context, object(), ApplyToStrokeTargetType, true)) {
            Vector<SVGChar>::iterator it = m_svgChars.begin();
            paintInlineBoxes(pi, tx, ty, this, it);
            ASSERT(it == m_svgChars.end());

            strokePaintServer->teardown(pi.context, object(), ApplyToStrokeTargetType, true);
        }
    }

#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    if (filter)
        filter->applyFilter(paintInfo.context, boundingBox);
#endif

    if (opacity < 1.0f)
        paintInfo.context->endTransparencyLayer();

    paintInfo.context->restore();
}

int SVGRootInlineBox::placeBoxesHorizontally(int x, int& leftPosition, int& rightPosition, bool& needsWordSpacing)
{
    // Remove any offsets caused by RTL text layout
    leftPosition = 0;
    rightPosition = 0;
    return 0;
}

void SVGRootInlineBox::verticallyAlignBoxes(int& heightOfBlock)
{
    // height is set by layoutInlineBoxes.
    heightOfBlock = height();
}

float cummulatedWidthOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange& range)
{
    ASSERT(!range.isOpen());
    ASSERT(range.isClosed());
    ASSERT(range.box->isInlineTextBox());

    InlineTextBox* textBox = static_cast<InlineTextBox*>(range.box);
    RenderText* text = textBox->textObject();
    RenderStyle* style = text->style();

    return style->font().floatWidth(TextRun(text->characters() + textBox->start() + range.startOffset, range.endOffset - range.startOffset), svgTextStyleForInlineTextBox(style, textBox, 0));
}

float cummulatedHeightOfInlineBoxCharacterRange(SVGInlineBoxCharacterRange& range)
{
    ASSERT(!range.isOpen());
    ASSERT(range.isClosed());
    ASSERT(range.box->isInlineTextBox());

    InlineTextBox* textBox = static_cast<InlineTextBox*>(range.box);
    RenderText* text = textBox->textObject();
    const Font& font = text->style()->font();

    // FIXME: Wild guess - works for the W3C 1.1 vertical text examples - not really heavily tested so far.
    return (range.endOffset - range.startOffset) * (font.ascent() + font.descent());
}

TextStyle svgTextStyleForInlineTextBox(RenderStyle* style, const InlineTextBox* textBox, float xPos)
{
    ASSERT(textBox);
    ASSERT(style);

    return TextStyle(false, xPos, textBox->toAdd(), textBox->m_reversed, textBox->m_dirOverride || style->visuallyOrdered());
}

static float cummulatedWidthOfTextChunk(SVGTextChunk& chunk)
{
    float width = 0.0;

    Vector<SVGInlineBoxCharacterRange>::iterator it = chunk.boxes.begin();
    Vector<SVGInlineBoxCharacterRange>::iterator end = chunk.boxes.end();

    for (; it != end; ++it)
        width += cummulatedWidthOfInlineBoxCharacterRange(*it);

    return width;
}

static float cummulatedHeightOfTextChunk(SVGTextChunk& chunk)
{
    float height = 0.0;

    Vector<SVGInlineBoxCharacterRange>::iterator it = chunk.boxes.begin();
    Vector<SVGInlineBoxCharacterRange>::iterator end = chunk.boxes.end();

    for (; it != end; ++it)
        height += cummulatedHeightOfInlineBoxCharacterRange(*it);

    return height;
}

static void applyTextAnchorToTextChunk(SVGTextChunk& chunk)
{
#if DEBUG_CHUNK_BUILDING > 0
    {
        fprintf(stderr, "Handle TEXT CHUNK! anchor=%i, isVerticalText=%i, isTextPath=%i start=%p, end=%p -> dist: %i\n",
                (int) chunk.anchor, (int) chunk.isVerticalText, (int) chunk.isTextPath, chunk.start, chunk.end, (unsigned int) (chunk.end-chunk.start));

        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = chunk.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = chunk.boxes.end();

        unsigned int i = 0;
        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt; i++;
            fprintf(stderr, "RANGE %i STARTOFFSET: %i, ENDOFFSET: %i, BOX: %p\n", i, range.startOffset, range.endOffset, range.box);
        }
    }
#endif

    if (chunk.anchor == TA_START || chunk.isTextPath)
        return;

    float shift = 0.0;

    if (chunk.isVerticalText)
        shift = cummulatedHeightOfTextChunk(chunk);
    else
        shift = cummulatedWidthOfTextChunk(chunk);

    if (chunk.anchor == TA_MIDDLE)
        shift *= -0.5;
    else
        shift *= -1.0;

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
        ASSERT(curBox->isInlineTextBox());
        ASSERT(curBox->parent() && (curBox->parent()->isRootInlineBox() || curBox->parent()->isInlineFlowBox()));

        // Move target box
        if (chunk.isVerticalText)
            curBox->setYPos(curBox->yPos() + shift);
        else
            curBox->setXPos(curBox->xPos() + shift);
    }
}

void SVGRootInlineBox::computePerCharacterLayoutInformation()
{
    // Clean up any previous layout information
    m_svgChars.clear();
    m_svgTextChunks.clear();

    // Build layout information for all contained render objects
    SVGCharacterLayoutInfo info;
    buildLayoutInformation(this, info);

    // Now all layout information are available for every character
    // contained in any of our child inline/flow boxes. Build list
    // of text chunks now, to be able to apply text-anchor shifts.
    buildTextChunks();

    // Layout all text chunks
    // text-anchor needs to be applied to individual chunks.
    layoutTextChunks();

    // Finally the top left position of our box is known.
    // Propogate this knownledge to our RenderSVGText parent.
    FloatPoint topLeft = topLeftPositionOfCharacterRange(m_svgChars);
    object()->setPos((int) floorf(topLeft.x()), (int) floorf(topLeft.y()));

    // Layout all InlineText/Flow boxes
    // BEWARE: This requires the root top/left position to be set correctly before!
    layoutInlineBoxes();
}

static void totalAdvanceOfInlineTextBox(InlineTextBox* textBox, int from, int to, bool isVerticalText, float& totalAdvance)
{
    ASSERT(textBox);
    ASSERT(textBox->object());

    if (to > 0 && (unsigned int) to > textBox->len())
        to = textBox->len();

    RenderStyle* style = textBox->object()->style();
    SVGInlineTextBox* svgTextBox = static_cast<SVGInlineTextBox*>(textBox);

    for (int i = from; i < to; ++i) {
        int offset = textBox->m_reversed ? textBox->end() - i : textBox->start() + i;

        if (!isVerticalText)
            totalAdvance += svgTextBox->calculateGlyphWidth(style, offset);
        else
            totalAdvance += svgTextBox->calculateGlyphHeight(style, offset);
    }
}

static void totalAdvanceOfInlineBox(InlineFlowBox* start, bool isVerticalText, float& totalAdvance)
{
    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText()) {
            InlineTextBox* textBox = static_cast<InlineTextBox*>(curr);
            totalAdvanceOfInlineTextBox(textBox, 0, textBox->len(), isVerticalText, totalAdvance);
        } else {
            ASSERT(curr->isInlineFlowBox());
            totalAdvanceOfInlineBox(static_cast<InlineFlowBox*>(curr), isVerticalText, totalAdvance);
        }
    }
}

void SVGRootInlineBox::buildLayoutInformation(InlineFlowBox* start, SVGCharacterLayoutInfo& info)
{
    if (start->isRootInlineBox()) {
        ASSERT(start->object()->element()->hasTagName(SVGNames::textTag));

        SVGTextPositioningElement* positioningElement = static_cast<SVGTextPositioningElement*>(start->object()->element());
        ASSERT(positioningElement);
        ASSERT(positioningElement->parentNode());

        info.addLayoutInformation(positioningElement);
    }

    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText())
            buildLayoutInformationForTextBox(info, static_cast<InlineTextBox*>(curr));
        else {
            ASSERT(curr->isInlineFlowBox());
            InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(curr);

            bool isAnchor = flowBox->object()->element()->hasTagName(SVGNames::aTag);
            bool isTextPath = flowBox->object()->element()->hasTagName(SVGNames::textPathTag);

            if (!isTextPath && !isAnchor) {
                SVGTextPositioningElement* positioningElement = static_cast<SVGTextPositioningElement*>(flowBox->object()->element());
                ASSERT(positioningElement);
                ASSERT(positioningElement->parentNode());

                info.addLayoutInformation(positioningElement);
            } else if (!isAnchor) {
                info.setInPathLayout(true);

                float textAnchorStartOffset = 0.0;
                bool isVerticalText = isVerticalWritingMode(flowBox->object()->style()->svgStyle());

                // Handle text-anchor on path, which is special.
                ETextAnchor anchor = flowBox->object()->style()->svgStyle()->textAnchor();
                switch (anchor) {
                    case TA_END:
                    case TA_MIDDLE:
                    {
                        float totalAdvance = 0.0;
                        totalAdvanceOfInlineBox(flowBox, isVerticalText, totalAdvance);

                        if (anchor == TA_END)
                            textAnchorStartOffset -= totalAdvance;
                        else
                            textAnchorStartOffset -= totalAdvance / 2.0;
                    }
                    case TA_START:
                        break;
                    default:
                        ASSERT_NOT_REACHED();
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

void SVGRootInlineBox::layoutInlineBoxes()
{
    int lowX = INT_MAX;
    int lowY = INT_MAX;
    int highX = INT_MIN;
    int highY = INT_MIN;

    // Layout all child boxes
    Vector<SVGChar>::iterator it = m_svgChars.begin(); 
    layoutInlineBoxes(this, it, lowX, highX, lowY, highY);
    ASSERT(it == m_svgChars.end());
}

void SVGRootInlineBox::layoutInlineBoxes(InlineFlowBox* start, Vector<SVGChar>::iterator& it, int& lowX, int& highX, int& lowY, int& highY)
{
    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        RenderStyle* style = curr->object()->style();    
        const Font& font = style->font();

        if (curr->object()->isText()) {
            SVGInlineTextBox* textBox = static_cast<SVGInlineTextBox*>(curr);
            unsigned length = textBox->len();

            SVGChar curChar = *it; 
            ASSERT(it != m_svgChars.end());  

            FloatRect stringRect;
            for (unsigned i = 0; i < length; ++i) {
                ASSERT(it != m_svgChars.end());

                stringRect.unite(textBox->calculateGlyphBoundaries(style, textBox->start() + i, *it));
                it++;
            }

            IntRect enclosedStringRect = enclosingIntRect(stringRect);

            int minX = enclosedStringRect.x();
            int maxX = minX + enclosedStringRect.width();

            int minY = enclosedStringRect.y();
            int maxY = minY + enclosedStringRect.height();

            curr->setXPos(minX - object()->xPos());
            curr->setWidth(enclosedStringRect.width());

            curr->setYPos(minY - object()->yPos());
            curr->setBaseline(font.ascent());
            curr->setHeight(enclosedStringRect.height());

            if (minX < lowX)
                lowX = minX;

            if (maxX > highX)
                highX = maxX;

            if (minY < lowY)
                lowY = minY;

            if (maxY > highY)
                highY = maxY;
        } else {
            ASSERT(curr->isInlineFlowBox());

            int minX = INT_MAX;
            int minY = INT_MAX;
            int maxX = INT_MIN;
            int maxY = INT_MIN;

            InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(curr);
            layoutInlineBoxes(flowBox, it, minX, maxX, minY, maxY);

            curr->setXPos(minX - object()->xPos());
            curr->setWidth(maxX - minX);

            curr->setYPos(minY - object()->yPos());
            curr->setBaseline(font.ascent());
            curr->setHeight(maxY - minY);

            if (minX < lowX)
                lowX = minX;

            if (maxX > highX)
                highX = maxX;

            if (minY < lowY)
                lowY = minY;

            if (maxY > highY)
                highY = maxY;
        }
    }

    if (start->isRootInlineBox()) {
        int top = lowY - object()->yPos();
        int bottom = highY - object()->yPos();

        start->setXPos(lowX - object()->xPos());
        start->setYPos(top);

        start->setWidth(highX - lowX);
        start->setHeight(highY - lowY);

        start->setVerticalOverflowPositions(top, bottom);
        start->setVerticalSelectionPositions(top, bottom);
    }
}

void SVGRootInlineBox::buildLayoutInformationForTextBox(SVGCharacterLayoutInfo& info, InlineTextBox* textBox)
{
    RenderText* text = textBox->textObject();
    ASSERT(text);

    RenderStyle* style = text->style(textBox->isFirstLineStyle());
    ASSERT(style);

    SVGInlineTextBox* svgTextBox = static_cast<SVGInlineTextBox*>(textBox);

    unsigned length = textBox->len();
    bool isVerticalText = isVerticalWritingMode(style->svgStyle());

    for (unsigned i = 0; i < length; ++i) {
        SVGChar svgChar;
        svgChar.drawnSeperated = false;
        svgChar.newTextChunk = false;

        float angle = 0.0;
        float glyphWidth = 0.0;
        float glyphHeight = 0.0;

        if (!textBox->m_reversed) {
            glyphWidth = svgTextBox->calculateGlyphWidth(style, textBox->start() + i);
            glyphHeight = svgTextBox->calculateGlyphHeight(style, textBox->start() + i);
        } else {
            glyphWidth = svgTextBox->calculateGlyphWidth(style, textBox->end() - i);
            glyphHeight = svgTextBox->calculateGlyphHeight(style, textBox->end() - i);
        }

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

        // Calculate absolute x/y values, if needed
        if (!info.inPathLayout()) {
            svgChar.visible = true;
        } else {
            float glyphAdvance = isVerticalText ? glyphHeight : glyphWidth;
            float newOffset = FLT_MIN;

            if (assignedX && !isVerticalText)
                newOffset = info.curx;
            else if (assignedY && isVerticalText)
                newOffset = info.cury;

            svgChar.visible = info.nextPathLayoutPointAndAngle(info.curx, info.cury, angle, glyphAdvance, newOffset);
        }

        float dx = 0.0;
        float dy = 0.0;

        // Apply x-axis shift
        if (info.dxValueAvailable()) {
            dx = info.dxValueNext();
            svgChar.drawnSeperated = true;

            info.dx += dx;
    
            if (!info.inPathLayout())
                info.curx += dx;
        }

        // Apply y-axis shift
        if (info.dyValueAvailable()) {
            dy = info.dyValueNext();
            svgChar.drawnSeperated = true;

            info.dy += dy;

            if (!info.inPathLayout())
                info.cury += dy;
        }

        // Apply rotation
        if (info.angleValueAvailable()) {
            svgChar.drawnSeperated = true;
            angle = info.angleValueNext();
        }

        // Apply baseline-shift
        if (info.baselineShiftValueAvailable()) {
            svgChar.drawnSeperated = true;
            float shift = info.baselineShiftValueNext();

            if (isVerticalText) {
                info.shiftx += shift;
                info.curx += shift;
            } else {
                info.shifty -= shift;
                info.cury -= shift;
            }
        }

        svgChar.x = info.curx;
        svgChar.y = info.cury;

        float pathXShift = 0.0;
        float pathYShift = 0.0;

        // Correct character position for text on path layout
        // All placed characters on a path use absolute positioning (internally).
        // So we still have to apply baseline-shift/dx/dy corrections manually.
        if (info.inPathLayout()) {
            svgChar.drawnSeperated = true;

            // Translate to glyph midpoint
            if (isVerticalText)
                pathYShift = glyphHeight / 2.0;
            else
                pathXShift = glyphWidth / 2.0;

            // Respect accumulated dx/dy values
            pathXShift -= info.dx;
            pathYShift -= info.dy;

            // Alter character position
            svgChar.x -= pathXShift;
            svgChar.y -= pathYShift;
        }

        // Correct character position for vertical text layout
        if (isVerticalText) {
            svgChar.drawnSeperated = true;
            svgChar.x -= glyphWidth / 2.0;
            svgChar.y += glyphHeight;
        }

        // Setup affine transform for single glyph
        if (angle != 0.0) {
            AffineTransform rotationMatrix;

            rotationMatrix.translate(svgChar.x + pathXShift, svgChar.y + pathYShift);
            rotationMatrix.rotate(angle);
            rotationMatrix.translate(-svgChar.x - pathXShift, -svgChar.y - pathYShift);

            svgChar.transform = rotationMatrix;
        }

        // Advance current position
        if (!isVerticalText)
            info.curx += glyphWidth;
        else
            info.cury += glyphHeight;

        m_svgChars.append(svgChar);
        info.processedSingleCharacter();
    }
}

void SVGRootInlineBox::buildTextChunks()
{
    SVGTextChunkLayoutInfo info;
    info.it = m_svgChars.begin();
    info.chunk.start = m_svgChars.begin();
    info.chunk.end = m_svgChars.begin();

    buildTextChunks(this, info);
    ASSERT(info.it == m_svgChars.end());
}

void SVGRootInlineBox::buildTextChunks(InlineFlowBox* start, SVGTextChunkLayoutInfo& info)
{
#if DEBUG_CHUNK_BUILDING > 1
    fprintf(stderr, " -> buildTextChunks(start=%p)\n", start);
#endif

    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText()) {
            InlineTextBox* textBox = static_cast<InlineTextBox*>(curr);

            unsigned length = textBox->len();
            if (!length)
                continue;

#if DEBUG_CHUNK_BUILDING > 1
            fprintf(stderr, " -> Handle inline text box (%p) with %i characters (start: %i, end: %i), handlingTextPath=%i\n",
                            textBox, length, textBox->start(), textBox->end(), (int) info.handlingTextPath);
#endif

            RenderText* text = textBox->textObject();
            ASSERT(text);

            // Start new character range for the first chunk
            bool isFirstCharacter = m_svgTextChunks.isEmpty() && info.chunk.start == info.it && info.chunk.start == info.chunk.end;
            if (isFirstCharacter) {
                ASSERT(info.chunk.boxes.isEmpty());
                info.chunk.boxes.append(SVGInlineBoxCharacterRange());
            } else
                ASSERT(!info.chunk.boxes.isEmpty());

            // Walk string to find out new chunk positions, if existant
            for (unsigned i = 0; i < length; ++i) {
                ASSERT(info.it != m_svgChars.end());

                SVGInlineBoxCharacterRange& range = info.chunk.boxes.last();
                if (range.isOpen()) {
                    range.box = curr;
                    range.startOffset = (i == 0 ? 0 : i - 1);

#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " | -> Range is open! box=%p, startOffset=%i\n", range.box, range.startOffset);
#endif
                }

                // If a new (or the first) chunk has been started, record it's text-anchor and writing mode.
                if (info.assignChunkProperties) {
                    info.assignChunkProperties = false;

                    info.chunk.isVerticalText = isVerticalWritingMode(text->style()->svgStyle());
                    info.chunk.isTextPath = info.handlingTextPath;
                    info.chunk.anchor = text->style()->svgStyle()->textAnchor();

#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " | -> Assign chunk properties, isVerticalText=%i, anchor=%i\n", info.chunk.isVerticalText, info.chunk.anchor);
#endif
                }

                if (i > 0 && !isFirstCharacter && (*info.it).newTextChunk) {
                    // Close mid chunk & character range
                    ASSERT(!range.isOpen());
                    ASSERT(!range.isClosed());

                    range.endOffset = i;
                    closeTextChunk(info, m_svgTextChunks);

#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " | -> Close mid-text chunk, at endOffset: %i and starting new mid chunk!\n", range.endOffset);
#endif
    
                    // Prepare for next chunk, if we're not at the end
                    startTextChunk(info);
                    if (i + 1 == length) {
#if DEBUG_CHUNK_BUILDING > 1
                        fprintf(stderr, " | -> Record last chunk of inline text box!\n");
#endif

                        startTextChunk(info);
                        SVGInlineBoxCharacterRange& range = info.chunk.boxes.last();

                        info.assignChunkProperties = false;
                        info.chunk.isVerticalText = isVerticalWritingMode(text->style()->svgStyle());
                        info.chunk.isTextPath = info.handlingTextPath;
                        info.chunk.anchor = text->style()->svgStyle()->textAnchor();

                        range.box = curr;
                        range.startOffset = i;

                        ASSERT(!range.isOpen());
                        ASSERT(!range.isClosed());
                    }
                }

                // This should only hold true for the first character of the first chunk
                if (isFirstCharacter)
                    isFirstCharacter = false;
    
                info.it++;
            }

#if DEBUG_CHUNK_BUILDING > 1    
            fprintf(stderr, " -> Finished inline text box!\n");
#endif

            SVGInlineBoxCharacterRange& range = info.chunk.boxes.last();
            if (!range.isOpen() && !range.isClosed()) {
#if DEBUG_CHUNK_BUILDING > 1
                fprintf(stderr, " -> Last range not closed - closing with endOffset: %i\n", length);
#endif

                // Current text chunk is not yet closed. Finish the current range, but don't start a new chunk.
                range.endOffset = length;

                if (info.it != m_svgChars.end()) {
#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " -> Not at last character yet!\n");
#endif

                    // If we're not at the end of the last box to be processed, and if the next
                    // character starts a new chunk, then close the current chunk and start a new one.
                    if ((*info.it).newTextChunk) {
#if DEBUG_CHUNK_BUILDING > 1
                        fprintf(stderr, " -> Next character starts new chunk! Closing current chunk, and starting a new one...\n");
#endif

                        closeTextChunk(info, m_svgTextChunks);
                        startTextChunk(info);
                    } else {
                        // Just start a new character range
                        info.chunk.boxes.append(SVGInlineBoxCharacterRange());

#if DEBUG_CHUNK_BUILDING > 1
                        fprintf(stderr, " -> Next character does NOT start a new chunk! Starting new character range...\n");
#endif
                    }
                } else {
#if DEBUG_CHUNK_BUILDING > 1
                    fprintf(stderr, " -> Closing final chunk! Finished processing!\n");
#endif

                    // Close final chunk, once we're at the end of the last box
                    closeTextChunk(info, m_svgTextChunks);
                }
            }
        } else {
            ASSERT(curr->isInlineFlowBox());
            InlineFlowBox* flowBox = static_cast<InlineFlowBox*>(curr);

            bool isTextPath = flowBox->object()->element()->hasTagName(SVGNames::textPathTag);

#if DEBUG_CHUNK_BUILDING > 1
            fprintf(stderr, " -> Handle inline flow box (%p), isTextPath=%i\n", flowBox, (int) isTextPath);
#endif

            if (isTextPath)
                info.handlingTextPath = true;

            buildTextChunks(flowBox, info);

            if (isTextPath)
                info.handlingTextPath = false;
        }
    }

#if DEBUG_CHUNK_BUILDING > 1
    fprintf(stderr, " <- buildTextChunks(start=%p)\n", start);
#endif
}

const Vector<SVGTextChunk>& SVGRootInlineBox::svgTextChunks() const 
{
    return m_svgTextChunks;
}

void SVGRootInlineBox::layoutTextChunks()
{
    Vector<SVGTextChunk>::iterator it = m_svgTextChunks.begin();
    Vector<SVGTextChunk>::iterator end = m_svgTextChunks.end();

    for (; it != end; ++it)
        applyTextAnchorToTextChunk(*it);
}

void SVGRootInlineBox::paintSelectionForTextBox(InlineTextBox* textBox, int boxStartOffset, const SVGChar& svgChar, const UChar* chars, int length, GraphicsContext* p, RenderStyle* style, const Font* f)
{
    if (textBox->selectionState() == RenderObject::SelectionNone)
        return;

    int startPos, endPos;
    textBox->selectionStartEnd(startPos, endPos);

    if (startPos >= endPos)
        return;

    Color textColor = style->color();
    Color color = textBox->object()->selectionBackgroundColor();
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

    if (!svgChar.transform.isIdentity())
        p->concatCTM(svgChar.transform);

    int adjust = startPos >= boxStartOffset ? boxStartOffset : 0;
    p->drawHighlightForText(TextRun(textBox->textObject()->text()->characters() + textBox->start() + boxStartOffset, length),
                            IntPoint((int) svgChar.x, (int) svgChar.y - f->ascent()), f->ascent() + f->descent(),
                            svgTextStyleForInlineTextBox(style, textBox, svgChar.x), color,
                            startPos - adjust, endPos - adjust);

    p->restore();
}

void SVGRootInlineBox::paintInlineBoxes(RenderObject::PaintInfo& paintInfo, int tx, int ty, InlineFlowBox* start, Vector<SVGChar>::iterator& it)
{
    for (InlineBox* curr = start->firstChild(); curr; curr = curr->nextOnLine()) {
        if (curr->object()->isText())
            paintChildInlineTextBox(paintInfo, tx, ty, static_cast<InlineTextBox*>(curr), it);
        else {
            ASSERT(curr->isInlineFlowBox());
            paintChildInlineFlowBox(paintInfo, tx, ty, static_cast<InlineFlowBox*>(curr), it);
        }
    }
}

void SVGRootInlineBox::paintChildInlineTextBox(RenderObject::PaintInfo& paintInfo, int tx, int ty, InlineTextBox* textBox, Vector<SVGChar>::iterator& it)
{
    unsigned length = textBox->len();
    if (!length)
        return;
    
    RenderText* text = textBox->textObject();
    ASSERT(text);
    
    // Paint all contained characters, one-by-one as worst case.
    for (unsigned i = 0; i < length; ++i) {
        ASSERT(it != m_svgChars.end());
        
        if (!(*it).visible) {
            it++;
            continue;
        }
        
        // Determine how many characters - starting from the current - can be drawn at once.
        unsigned startOffset = i + 1, run = 1;
        while (startOffset < length) {
            SVGChar chunk = *(it + startOffset - i);
            if (chunk.drawnSeperated)
                break;
            
            run++;
            startOffset++;
        }

        paintCharacterRangeForTextBox(paintInfo, tx, ty, textBox, *it, text->characters() + textBox->start() + i, run);

        i += run - 1;
        it += run;
    }
}

void SVGRootInlineBox::paintChildInlineFlowBox(RenderObject::PaintInfo& paintInfo, int tx, int ty, InlineFlowBox* flowBox, Vector<SVGChar>::iterator& it)
{
    FloatRect boundingBox;
    
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    SVGResourceFilter* filter = 0;
#else
    void* filter = 0;
#endif
    prepareTextRendering(paintInfo, tx, ty, flowBox, boundingBox, filter);
    
    RenderObject* object = flowBox->object();
    RenderObject::PaintInfo pi(paintInfo);
    
    if (!flowBox->isRootInlineBox())
        pi.rect = (object->localTransform()).inverse().mapRect(pi.rect);
    
    float opacity = object->style()->opacity();
    if (opacity < 1.0f) {
        paintInfo.context->clip(enclosingIntRect(boundingBox));
        paintInfo.context->beginTransparencyLayer(opacity);
    }
    
    bool painted = false;
    Vector<SVGChar>::iterator savedIt = it;
    
    SVGPaintServer* fillPaintServer = SVGPaintServer::fillPaintServer(object->style(), object);
    if (fillPaintServer) {
        if (fillPaintServer->setup(pi.context, object, ApplyToFillTargetType, true)) {
            painted = true;
            
            paintInlineBoxes(pi, tx, ty, flowBox, it);
            fillPaintServer->teardown(pi.context, object, ApplyToFillTargetType, true);
        }
    }
    
    SVGPaintServer* strokePaintServer = SVGPaintServer::strokePaintServer(object->style(), object);
    if (strokePaintServer) {
        if (strokePaintServer->setup(pi.context, object, ApplyToStrokeTargetType, true)) {
            if (painted)
                it = savedIt;
            
            paintInlineBoxes(pi, tx, ty, flowBox, it);
            strokePaintServer->teardown(pi.context, object, ApplyToStrokeTargetType, true);
        }
    }
    
#if ENABLE(SVG_EXPERIMENTAL_FEATURES)
    if (filter)
        filter->applyFilter(paintInfo.context, boundingBox);
#endif
    
    if (opacity < 1.0f)
        paintInfo.context->endTransparencyLayer();
    
    paintInfo.context->restore();
}

void SVGRootInlineBox::paintCharacterRangeForTextBox(RenderObject::PaintInfo& paintInfo, int tx, int ty, InlineTextBox* textBox, const SVGChar& svgChar, const UChar* chars, int length)
{
    if (object()->style()->visibility() != VISIBLE || paintInfo.phase == PaintPhaseOutline)
        return;

    ASSERT(paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines);

    RenderText* text = textBox->textObject();
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
    RenderStyle* styleToUse = text->style(textBox->isFirstLineStyle());
    int d = styleToUse->textDecorationsInEffect();
    const Font* font = &styleToUse->font();
    if (*font != paintInfo.context->font())
        paintInfo.context->setFont(*font);

    // 1. Paint backgrounds behind text if needed.  Examples of such backgrounds include selection
    // and composition decorations.
    if (paintInfo.phase != PaintPhaseSelection && !isPrinting) {
#if PLATFORM(MAC)
        // Custom highlighters go behind everything else.
        if (styleToUse->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            textBox->paintCustomHighlight(tx, ty, styleToUse->highlight());
#endif

        if (containsComposition && !useCustomUnderlines)
            textBox->paintCompositionBackground(paintInfo.context, tx, ty, styleToUse, font, 
                                                text->document()->frame()->editor()->compositionStart(),
                                                text->document()->frame()->editor()->compositionEnd());
        
        textBox->paintDocumentMarkers(paintInfo.context, tx, ty, styleToUse, font, true);
        
        if (haveSelection && !useCustomUnderlines) {
            int boxStartOffset = chars - text->characters() - textBox->start();
            paintSelectionForTextBox(textBox, boxStartOffset, svgChar, chars, length, paintInfo.context, styleToUse, font);
        }
    }

    Color textFillColor;
    Color textStrokeColor;
    float textStrokeWidth = styleToUse->textStrokeWidth();

    if (paintInfo.forceBlackText) {
        textFillColor = Color::black;
        textStrokeColor = Color::black;
    } else {
        textFillColor = styleToUse->textFillColor();
        if (!textFillColor.isValid())
            textFillColor = styleToUse->color();

        // Make the text fill color legible against a white background
        if (styleToUse->forceBackgroundsToWhite())
            textFillColor = correctedTextColor(textFillColor, Color::white);

        textStrokeColor = styleToUse->textStrokeColor();
        if (!textStrokeColor.isValid())
            textStrokeColor = styleToUse->color();

        // Make the text stroke color legible against a white background
        if (styleToUse->forceBackgroundsToWhite())
            textStrokeColor = correctedTextColor(textStrokeColor, Color::white);
    }

    // For stroked painting, we have to change the text drawing mode.  It's probably dangerous to leave that mutated as a side
    // effect, so only when we know we're stroking, do a save/restore.
    if (textStrokeWidth > 0)
        paintInfo.context->save();

    if (!svgChar.transform.isIdentity())
        paintInfo.context->concatCTM(svgChar.transform);

    updateGraphicsContext(paintInfo.context, textFillColor, textStrokeColor, textStrokeWidth);
    
    // Set a text shadow if we have one.
    // FIXME: Support multiple shadow effects.  Need more from the CG API before
    // we can do this.
    bool setShadow = false;
    if (styleToUse->textShadow()) {
        paintInfo.context->setShadow(IntSize(styleToUse->textShadow()->x, styleToUse->textShadow()->y),
                                     styleToUse->textShadow()->blur, styleToUse->textShadow()->color);
        setShadow = true;
    }

    bool paintSelectedTextOnly = (paintInfo.phase == PaintPhaseSelection);
    bool paintSelectedTextSeparately = false; // Whether or not we have to do multiple paints.  Only
                                              // necessary when a custom ::selection foreground color is applied.
    Color selectionFillColor = textFillColor;
    Color selectionStrokeColor = textStrokeColor;
    float selectionStrokeWidth = textStrokeWidth;
    ShadowData* selectionTextShadow = 0;

    if (haveSelection) {
        // Check foreground color first.
        Color foreground = object()->selectionForegroundColor();
        if (foreground.isValid() && foreground != selectionFillColor) {
            if (!paintSelectedTextOnly)
                paintSelectedTextSeparately = true;
            selectionFillColor = foreground;
        }
        RenderStyle* pseudoStyle = object()->getPseudoStyle(RenderStyle::SELECTION);
        if (pseudoStyle) {
            if (pseudoStyle->textShadow()) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                if (pseudoStyle->textShadow())
                    selectionTextShadow = pseudoStyle->textShadow();
            }

            float strokeWidth = pseudoStyle->textStrokeWidth();
            if (strokeWidth != selectionStrokeWidth) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeWidth = strokeWidth;
            }

            Color stroke = pseudoStyle->textStrokeColor();
            if (!stroke.isValid())
                stroke = pseudoStyle->color();
            if (stroke != selectionStrokeColor) {
                if (!paintSelectedTextOnly)
                    paintSelectedTextSeparately = true;
                selectionStrokeColor = stroke;
            }
        }
    }

    ASSERT(!paintSelectedTextOnly && !paintSelectedTextSeparately);
    paintInfo.context->drawText(TextRun(chars, length), IntPoint((int) svgChar.x, (int) svgChar.y), svgTextStyleForInlineTextBox(styleToUse, textBox, svgChar.x));

    // Paint decorations
    if (d != TDNONE && paintInfo.phase != PaintPhaseSelection) {
        // InlineTextBox::paint, checks for htmlHacks() being enabled - obey that.
        // InlineTextBox::paint, doesn't save/restore context here, needed for us!
        paintInfo.context->save();
        paintInfo.context->setStrokeColor(styleToUse->color());
        textBox->paintDecoration(paintInfo.context, tx, ty, d);
        paintInfo.context->restore();
    }

    if (paintInfo.phase != PaintPhaseSelection) {
        textBox->paintDocumentMarkers(paintInfo.context, tx, ty, styleToUse, font, false);

        if (useCustomUnderlines) {
            const Vector<CompositionUnderline>& underlines = text->document()->frame()->editor()->customCompositionUnderlines();
            size_t numUnderlines = underlines.size();

            for (size_t index = 0; index < numUnderlines; ++index) {
                const CompositionUnderline& underline = underlines[index];
                
                if (underline.endOffset <= textBox->start())
                    // underline is completely before this run.  This might be an underline that sits
                    // before the first run we draw, or underlines that were within runs we skipped 
                    // due to truncation.
                    continue;
                
                if (underline.startOffset <= textBox->end()) {
                    // underline intersects this run.  Paint it.
                    textBox->paintCompositionUnderline(paintInfo.context, tx, ty, underline);
                    if (underline.endOffset > textBox->end() + 1)
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

    if (!svgChar.transform.isIdentity())
        paintInfo.context->concatCTM(svgChar.transform.inverse());

    if (textStrokeWidth > 0)
        paintInfo.context->restore();
}

} // namespace WebCore

#endif // ENABLE(SVG)
