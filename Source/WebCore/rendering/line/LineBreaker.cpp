/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All right reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2013 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2013 Adobe Systems Inc. All right reserved.
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
#include "LineBreaker.h"

#include "BreakingContextInlineHeaders.h"
#include "RenderCombineText.h"
#include "ShapeInsideInfo.h"

namespace WebCore {

void LineBreaker::reset()
{
    m_positionedObjects.clear();
    m_hyphenated = false;
    m_clear = CNONE;
}

// FIXME: The entire concept of the skipTrailingWhitespace function is flawed, since we really need to be building
// line boxes even for containers that may ultimately collapse away. Otherwise we'll never get positioned
// elements quite right. In other words, we need to build this function's work into the normal line
// object iteration process.
// NB. this function will insert any floating elements that would otherwise
// be skipped but it will not position them.
void LineBreaker::skipTrailingWhitespace(InlineIterator& iterator, const LineInfo& lineInfo)
{
    while (!iterator.atEnd() && !requiresLineBox(iterator, lineInfo, TrailingWhitespace)) {
        RenderObject& object = *iterator.renderer();
        if (object.isOutOfFlowPositioned())
            setStaticPositions(m_block, toRenderBox(object));
        else if (object.isFloating())
            m_block.insertFloatingObject(toRenderBox(object));
        iterator.increment();
    }
}

void LineBreaker::skipLeadingWhitespace(InlineBidiResolver& resolver, LineInfo& lineInfo, FloatingObject* lastFloatFromPreviousLine, LineWidth& width)
{
    while (!resolver.position().atEnd() && !requiresLineBox(resolver.position(), lineInfo, LeadingWhitespace)) {
        RenderObject& object = *resolver.position().renderer();
        if (object.isOutOfFlowPositioned()) {
            setStaticPositions(m_block, toRenderBox(object));
            if (object.style().isOriginalDisplayInlineType()) {
                resolver.runs().addRun(new BidiRun(0, 1, object, resolver.context(), resolver.dir()));
                lineInfo.incrementRunsFromLeadingWhitespace();
            }
        } else if (object.isFloating())
            m_block.positionNewFloatOnLine(m_block.insertFloatingObject(toRenderBox(object)), lastFloatFromPreviousLine, lineInfo, width);
        else if (object.isText() && object.style().hasTextCombine() && object.isCombineText() && !toRenderCombineText(object).isCombined()) {
            toRenderCombineText(object).combineText();
            if (toRenderCombineText(object).isCombined())
                continue;
        }
        resolver.increment();
    }
    resolver.commitExplicitEmbedding();
}

InlineIterator LineBreaker::nextLineBreak(InlineBidiResolver& resolver, LineInfo& lineInfo, RenderTextInfo& renderTextInfo, FloatingObject* lastFloatFromPreviousLine, unsigned consecutiveHyphenatedLines, WordMeasurements& wordMeasurements)
{
#if !ENABLE(CSS_SHAPES) || !ENABLE(CSS_SHAPE_INSIDE)
    return nextSegmentBreak(resolver, lineInfo, renderTextInfo, lastFloatFromPreviousLine, consecutiveHyphenatedLines, wordMeasurements);
#else
    ShapeInsideInfo* shapeInsideInfo = m_block.layoutShapeInsideInfo();

    if (!shapeInsideInfo || !shapeInsideInfo->lineOverlapsShapeBounds())
        return nextSegmentBreak(resolver, lineInfo, renderTextInfo, lastFloatFromPreviousLine, consecutiveHyphenatedLines, wordMeasurements);

    InlineIterator end = resolver.position();
    InlineIterator oldEnd = end;

    if (!shapeInsideInfo->hasSegments()) {
        end = nextSegmentBreak(resolver, lineInfo, renderTextInfo, lastFloatFromPreviousLine, consecutiveHyphenatedLines, wordMeasurements);
        resolver.setPositionIgnoringNestedIsolates(oldEnd);
        return oldEnd;
    }

    const SegmentList& segments = shapeInsideInfo->segments();
    SegmentRangeList& segmentRanges = shapeInsideInfo->segmentRanges();

    for (unsigned i = 0; i < segments.size() && !end.atEnd(); i++) {
        InlineIterator segmentStart = resolver.position();
        end = nextSegmentBreak(resolver, lineInfo, renderTextInfo, lastFloatFromPreviousLine, consecutiveHyphenatedLines, wordMeasurements);

        ASSERT(segmentRanges.size() == i);
        if (resolver.position().atEnd()) {
            segmentRanges.append(LineSegmentRange(segmentStart, end));
            break;
        }
        if (resolver.position() == end) {
            // Nothing fit this segment
            end = segmentStart;
            segmentRanges.append(LineSegmentRange(segmentStart, segmentStart));
            resolver.setPositionIgnoringNestedIsolates(segmentStart);
        } else {
            // Note that resolver.position is already skipping some of the white space at the beginning of the line,
            // so that's why segmentStart might be different than resolver.position().
            LineSegmentRange range(resolver.position(), end);
            segmentRanges.append(range);
            resolver.setPosition(end, numberOfIsolateAncestors(end));

            if (lineInfo.previousLineBrokeCleanly()) {
                // If we hit a new line break, just stop adding anything to this line.
                break;
            }
        }
    }
    resolver.setPositionIgnoringNestedIsolates(oldEnd);
    return end;
#endif
}

InlineIterator LineBreaker::nextSegmentBreak(InlineBidiResolver& resolver, LineInfo& lineInfo, RenderTextInfo& renderTextInfo, FloatingObject* lastFloatFromPreviousLine, unsigned consecutiveHyphenatedLines, WordMeasurements& wordMeasurements)
{
    reset();

    ASSERT(resolver.position().root() == &m_block);

    bool appliedStartWidth = resolver.position().offset();

    LineWidth width(m_block, lineInfo.isFirstLine(), requiresIndent(lineInfo.isFirstLine(), lineInfo.previousLineBrokeCleanly(), m_block.style()));

    skipLeadingWhitespace(resolver, lineInfo, lastFloatFromPreviousLine, width);

    if (resolver.position().atEnd())
        return resolver.position();

    BreakingContext context(*this, resolver, lineInfo, width, renderTextInfo, lastFloatFromPreviousLine, appliedStartWidth, m_block);

    while (context.currentObject()) {
        context.initializeForCurrentObject();
        if (context.currentObject()->isBR()) {
            context.handleBR(m_clear);
        } else if (context.currentObject()->isOutOfFlowPositioned()) {
            context.handleOutOfFlowPositioned(m_positionedObjects);
        } else if (context.currentObject()->isFloating()) {
            context.handleFloat();
        } else if (context.currentObject()->isRenderInline()) {
            context.handleEmptyInline();
        } else if (context.currentObject()->isReplaced()) {
            context.handleReplaced();
        } else if (context.currentObject()->isText()) {
            if (context.handleText(wordMeasurements, m_hyphenated, consecutiveHyphenatedLines)) {
                // We've hit a hard text line break. Our line break iterator is updated, so go ahead and early return.
                return context.lineBreak();
            }
        } else if (context.currentObject()->isLineBreakOpportunity())
            context.commitLineBreakAtCurrentWidth(context.currentObject());
        else
            ASSERT_NOT_REACHED();

        if (context.atEnd())
            return context.handleEndOfLine();

        context.commitAndUpdateLineBreakIfNeeded();

        if (context.atEnd())
            return context.handleEndOfLine();

        context.increment();
    }

    context.clearLineBreakIfFitsOnLine(true);

    return context.handleEndOfLine();
}

}
