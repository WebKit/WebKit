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

#if ENABLE(SVG)
#include "SVGRootInlineBox.h"

#include "Editor.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "RenderSVGRoot.h"
#include "SVGInlineFlowBox.h"
#include "SVGInlineTextBox.h"
#include "SVGPaintServer.h"
#include "SVGRenderSupport.h"
#include "SVGResourceFilter.h"
#include "SVGTextPositioningElement.h"
#include "SVGURIReference.h"
#include "Text.h"
#include "TextStyle.h"

// Text chunk creation is complex and the whole process
// can easily be traced by setting this variable > 0.
#define DEBUG_CHUNK_BUILDING 0

namespace WebCore {

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

// Helper class for paint()
struct SVGRootInlineBoxPaintWalker {
    SVGRootInlineBoxPaintWalker(SVGRootInlineBox* rootBox, SVGResourceFilter* rootFilter, RenderObject::PaintInfo paintInfo, int tx, int ty)
        : m_rootBox(rootBox)
        , m_chunkStarted(false)
        , m_paintInfo(paintInfo)
        , m_savedInfo(paintInfo)
        , m_boundingBox(tx + rootBox->xPos(), ty + rootBox->yPos(), rootBox->width(), rootBox->height())
        , m_filter(0)
        , m_rootFilter(rootFilter)
        , m_fillPaintServer(0)
        , m_strokePaintServer(0)
        , m_fillPaintServerObject(0)
        , m_strokePaintServerObject(0)
        , m_tx(tx)
        , m_ty(ty)
    {
    }

    ~SVGRootInlineBoxPaintWalker()
    {
        ASSERT(!m_filter);
        ASSERT(!m_fillPaintServer);
        ASSERT(!m_fillPaintServerObject);
        ASSERT(!m_strokePaintServer);
        ASSERT(!m_strokePaintServerObject);
        ASSERT(!m_chunkStarted);
    }

    void teardownFillPaintServer()
    {
        if (!m_fillPaintServer)
            return;

        m_fillPaintServer->teardown(m_paintInfo.context, m_fillPaintServerObject, ApplyToFillTargetType, true);

        m_fillPaintServer = 0;
        m_fillPaintServerObject = 0;
    }

    void teardownStrokePaintServer()
    {
        if (!m_strokePaintServer)
            return;

        m_strokePaintServer->teardown(m_paintInfo.context, m_strokePaintServerObject, ApplyToStrokeTargetType, true);

        m_strokePaintServer = 0;
        m_strokePaintServerObject = 0;
    }

    void chunkStartCallback(InlineBox* box)
    {
        ASSERT(!m_chunkStarted);
        m_chunkStarted = true;

        InlineFlowBox* flowBox = box->parent();

        // Initialize text rendering
        RenderObject* object = flowBox->object();
        ASSERT(object);

        m_savedInfo = m_paintInfo;
        m_paintInfo.context->save();

        if (!flowBox->isRootInlineBox())
            m_paintInfo.context->concatCTM(m_rootBox->object()->localTransform());

        m_paintInfo.context->concatCTM(object->localTransform());

        if (!flowBox->isRootInlineBox()) {
            prepareToRenderSVGContent(object, m_paintInfo, m_boundingBox, m_filter, m_rootFilter);
            m_paintInfo.rect = object->localTransform().inverse().mapRect(m_paintInfo.rect);
        }
    }

    void chunkEndCallback(InlineBox* box)
    {
        ASSERT(m_chunkStarted);
        m_chunkStarted = false;

        InlineFlowBox* flowBox = box->parent();

        RenderObject* object = flowBox->object();
        ASSERT(object);

        // Clean up last used paint server
        teardownFillPaintServer();
        teardownStrokePaintServer();

        // Finalize text rendering 
        if (!flowBox->isRootInlineBox()) {
            finishRenderSVGContent(object, m_paintInfo, m_boundingBox, m_filter, m_savedInfo.context);
            m_filter = 0;
        }

        // Restore context & repaint rect
        m_paintInfo.context->restore();
        m_paintInfo.rect = m_savedInfo.rect;
    }

    bool chunkSetupFillCallback(InlineBox* box)
    {
        InlineFlowBox* flowBox = box->parent();

        // Setup fill paint server
        RenderObject* object = flowBox->object();
        ASSERT(object);

        ASSERT(!m_strokePaintServer);
        teardownFillPaintServer();

        m_fillPaintServer = SVGPaintServer::fillPaintServer(object->style(), object);
        if (m_fillPaintServer) {
            m_fillPaintServer->setup(m_paintInfo.context, object, ApplyToFillTargetType, true);
            m_fillPaintServerObject = object;
            return true;
        }

        return false;
    }

    bool chunkSetupStrokeCallback(InlineBox* box)
    {
        InlineFlowBox* flowBox = box->parent();

        // Setup stroke paint server
        RenderObject* object = flowBox->object();
        ASSERT(object);

        // If we're both stroked & filled, teardown fill paint server before stroking.
        teardownFillPaintServer();
        teardownStrokePaintServer();

        m_strokePaintServer = SVGPaintServer::strokePaintServer(object->style(), object);

        if (m_strokePaintServer) {
            m_strokePaintServer->setup(m_paintInfo.context, object, ApplyToStrokeTargetType, true);
            m_strokePaintServerObject = object;
            return true;
        }

        return false;
    }

    void chunkPortionCallback(SVGInlineTextBox* textBox, int startOffset, const Vector<SVGChar>::iterator& start, const Vector<SVGChar>::iterator& end)
    {
        RenderText* text = textBox->textObject();
        ASSERT(text);

        RenderStyle* styleToUse = text->style(textBox->isFirstLineStyle());
        ASSERT(styleToUse);
        ASSERT(textBox->parent()->object());

        startOffset += textBox->start();

        int textDecorations = styleToUse->textDecorationsInEffect(); 

        int textWidth = 0.0;
        IntPoint decorationOrigin;
        SVGTextDecorationInfo info;

        for (Vector<SVGChar>::iterator it = start; it != end; ++it) {
            if (!(*it).visible)
                continue;

            // Determine how many characters - starting from the current - can be drawn at once.
            Vector<SVGChar>::iterator itSearch = it + 1;
            while (itSearch != end) {
                if ((*itSearch).drawnSeperated)
                    break;

                itSearch++;
            }

            const UChar* stringStart = text->characters() + startOffset + (it - start);
            unsigned int stringLength = itSearch - it;

            // Paint decorations, that have to be drawn before the text gets drawn
            if (textDecorations != TDNONE && m_paintInfo.phase != PaintPhaseSelection) {
                textWidth = styleToUse->font().width(TextRun(stringStart, stringLength), styleToUse);
                decorationOrigin = IntPoint((int) (*it).x, (int) (*it).y - styleToUse->font().ascent());
                info = m_rootBox->retrievePaintServersForTextDecoration(text);
            }

            if (textDecorations & UNDERLINE && textWidth != 0.0)
                textBox->paintDecoration(UNDERLINE, m_paintInfo.context, decorationOrigin.x(), decorationOrigin.y(), textWidth, *it, info);

            if (textDecorations & OVERLINE && textWidth != 0.0)
                textBox->paintDecoration(OVERLINE, m_paintInfo.context, decorationOrigin.x(), decorationOrigin.y(), textWidth, *it, info);

            // Paint text
            textBox->paintCharacters(m_paintInfo, m_tx, m_ty, *it, stringStart, stringLength);

            // Paint decorations, that have to be drawn afterwards
            if (textDecorations & LINE_THROUGH && textWidth != 0.0)
                textBox->paintDecoration(LINE_THROUGH, m_paintInfo.context, decorationOrigin.x(), decorationOrigin.y(), textWidth, *it, info);

            // Skip processed characters
            it = itSearch - 1;
        }
    }

private:
    SVGRootInlineBox* m_rootBox;
    bool m_chunkStarted : 1;

    RenderObject::PaintInfo m_paintInfo;
    RenderObject::PaintInfo m_savedInfo;

    FloatRect m_boundingBox;
    SVGResourceFilter* m_filter;
    SVGResourceFilter* m_rootFilter;

    SVGPaintServer* m_fillPaintServer;
    SVGPaintServer* m_strokePaintServer;

    RenderObject* m_fillPaintServerObject;
    RenderObject* m_strokePaintServerObject;

    int m_tx;
    int m_ty;
};

void SVGRootInlineBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    if (paintInfo.context->paintingDisabled() || paintInfo.phase != PaintPhaseForeground)
        return;

    RenderObject::PaintInfo savedInfo(paintInfo);
    paintInfo.context->save();

    SVGResourceFilter* filter = 0;
    FloatRect boundingBox(tx + xPos(), ty + yPos(), width(), height());

    // Initialize text rendering
    paintInfo.context->concatCTM(object()->localTransform());
    prepareToRenderSVGContent(object(), paintInfo, boundingBox, filter);
    paintInfo.context->concatCTM(object()->localTransform().inverse());
 
    // Render text, chunk-by-chunk
    SVGRootInlineBoxPaintWalker walkerCallback(this, filter, paintInfo, tx, ty);
    SVGTextChunkWalker<SVGRootInlineBoxPaintWalker> walker(&walkerCallback,
                                                           &SVGRootInlineBoxPaintWalker::chunkPortionCallback,
                                                           &SVGRootInlineBoxPaintWalker::chunkStartCallback,
                                                           &SVGRootInlineBoxPaintWalker::chunkEndCallback,
                                                           &SVGRootInlineBoxPaintWalker::chunkSetupFillCallback,
                                                           &SVGRootInlineBoxPaintWalker::chunkSetupStrokeCallback);

    walkTextChunks(&walker);

    // Finalize text rendering 
    finishRenderSVGContent(object(), paintInfo, boundingBox, filter, savedInfo.context);
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
        if (!info.inPathLayout())
            svgChar.visible = true;
        else {
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

static inline void addPaintServerToTextDecorationInfo(ETextDecoration decoration, SVGTextDecorationInfo& info, RenderObject* object)
{
    if (object->style()->svgStyle()->hasFill())
        info.fillServerMap.set(decoration, object);

    if (object->style()->svgStyle()->hasStroke())
        info.strokeServerMap.set(decoration, object);
}

SVGTextDecorationInfo SVGRootInlineBox::retrievePaintServersForTextDecoration(RenderObject* start)
{
    ASSERT(start);

    Vector<RenderObject*> parentChain;
    while ((start = start->parent())) {
        parentChain.prepend(start);

        // Stop at our direct <text> parent.
        if (start->isSVGText())
            break;
    }

    Vector<RenderObject*>::iterator it = parentChain.begin();
    Vector<RenderObject*>::iterator end = parentChain.end();

    SVGTextDecorationInfo info;

    for (; it != end; ++it) {
        RenderObject* object = *it;
        ASSERT(object);

        RenderStyle* style = object->style();
        ASSERT(style);

        int decorations = style->textDecoration();
        if (decorations != NONE) {
            if (decorations & OVERLINE)
                addPaintServerToTextDecorationInfo(OVERLINE, info, object);

            if (decorations & UNDERLINE)
                addPaintServerToTextDecorationInfo(UNDERLINE, info, object);

            if (decorations & LINE_THROUGH)
                addPaintServerToTextDecorationInfo(LINE_THROUGH, info, object);
        }
    }

    return info;
}

void SVGRootInlineBox::walkTextChunks(SVGTextChunkWalkerBase* walker, const SVGInlineTextBox* textBox)
{
    ASSERT(walker);

    Vector<SVGTextChunk>::iterator it = m_svgTextChunks.begin();
    Vector<SVGTextChunk>::iterator itEnd = m_svgTextChunks.end();

    for (; it != itEnd; ++it) {
        SVGTextChunk& curChunk = *it;

        Vector<SVGInlineBoxCharacterRange>::iterator boxIt = curChunk.boxes.begin();
        Vector<SVGInlineBoxCharacterRange>::iterator boxEnd = curChunk.boxes.end();

        InlineBox* lastNotifiedBox = 0;
        InlineBox* prevBox = 0;

        unsigned int chunkOffset = 0;
        bool startedFirstChunk = false;

        for (; boxIt != boxEnd; ++boxIt) {
            SVGInlineBoxCharacterRange& range = *boxIt;

            ASSERT(range.box->isInlineTextBox());
            SVGInlineTextBox* rangeTextBox = static_cast<SVGInlineTextBox*>(range.box);

            if (textBox && rangeTextBox != textBox) {
                chunkOffset += range.endOffset - range.startOffset;
                continue;
            }

            // Eventually notify that we started a new chunk
            if (!textBox && !startedFirstChunk) {
                startedFirstChunk = true;

                lastNotifiedBox = range.box;
                walker->start(range.box);
            } else {
                // Eventually apply new style, as this chunk spans multiple boxes (with possible different styling)
                if (prevBox && prevBox != range.box) {
                    lastNotifiedBox = range.box;

                    walker->end(prevBox);
                    walker->start(lastNotifiedBox);
                }
            }

            unsigned int length = range.endOffset - range.startOffset;

            Vector<SVGChar>::iterator itCharBegin = curChunk.start + chunkOffset;
            Vector<SVGChar>::iterator itCharEnd = curChunk.start + chunkOffset + length;
            ASSERT(itCharEnd <= curChunk.end);

            // Process this chunk portion
            if (textBox)
                (*walker)(rangeTextBox, range.startOffset, itCharBegin, itCharEnd);
            else {
                if (walker->setupFill(range.box))
                    (*walker)(rangeTextBox, range.startOffset, itCharBegin, itCharEnd);

                if (walker->setupStroke(range.box))
                    (*walker)(rangeTextBox, range.startOffset, itCharBegin, itCharEnd);
            }

            chunkOffset += length;

            if (!textBox)
                prevBox = range.box;
        }

        if (!textBox && startedFirstChunk)
            walker->end(lastNotifiedBox);
    }
}

} // namespace WebCore

#endif // ENABLE(SVG)
