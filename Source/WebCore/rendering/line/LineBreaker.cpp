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

#include "RenderCombineText.h"

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
        RenderObject& object = *iterator.m_obj;
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
        RenderObject& object = *resolver.position().m_obj;
        if (object.isOutOfFlowPositioned()) {
            setStaticPositions(m_block, toRenderBox(object));
            if (object.style().isOriginalDisplayInlineType()) {
                resolver.runs().addRun(new BidiRun(0, 1, object, resolver.context(), resolver.dir()));
                lineInfo.incrementRunsFromLeadingWhitespace();
            }
        } else if (object.isFloating()) {
            // The top margin edge of a self-collapsing block that clears a float intrudes up into it by the height of the margin,
            // so in order to place this first child float at the top content edge of the self-collapsing block add the margin back in before placement.
            LayoutUnit marginOffset = (!object.previousSibling() && m_block.isSelfCollapsingBlock() && m_block.style().clear() && m_block.getClearDelta(m_block, LayoutUnit())) ? m_block.collapsedMarginBeforeForChild(m_block) : LayoutUnit();
            LayoutUnit oldLogicalHeight = m_block.logicalHeight();
            m_block.setLogicalHeight(oldLogicalHeight + marginOffset);
            m_block.positionNewFloatOnLine(m_block.insertFloatingObject(toRenderBox(object)), lastFloatFromPreviousLine, lineInfo, width);
            m_block.setLogicalHeight(oldLogicalHeight);
        } else if (object.isText() && object.style().hasTextCombine() && object.isCombineText() && !toRenderCombineText(object).isCombined()) {
            toRenderCombineText(object).combineText();
            if (toRenderCombineText(object).isCombined())
                continue;
        }
        resolver.increment();
    }
    resolver.commitExplicitEmbedding();
}

}
