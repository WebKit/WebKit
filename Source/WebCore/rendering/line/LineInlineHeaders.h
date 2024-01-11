/**
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
 */

#pragma once

#include "LineInfo.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"

namespace WebCore {

enum WhitespacePosition : bool { LeadingWhitespace, TrailingWhitespace };

inline bool hasInlineDirectionBordersPaddingOrMargin(const RenderInline& flow)
{
    // Where an empty inline is split across anonymous blocks we should only give lineboxes to the 'sides' of the
    // inline that have borders, padding or margin.
    bool shouldApplyStartBorderPaddingOrMargin = !flow.parent()->isAnonymousBlock() || !flow.isContinuation();
    if (shouldApplyStartBorderPaddingOrMargin && (flow.borderStart() || flow.marginStart() || flow.paddingStart()))
        return true;

    bool shouldApplyEndBorderPaddingOrMargin = !flow.parent()->isAnonymousBlock() || flow.isContinuation() || !flow.inlineContinuation();
    return shouldApplyEndBorderPaddingOrMargin && (flow.borderEnd() || flow.marginEnd() || flow.paddingEnd());
}

inline const RenderStyle& lineStyle(const RenderObject& renderer, const LineInfo& lineInfo)
{
    return lineInfo.isFirstLine() ? renderer.firstLineStyle() : renderer.style();
}

inline bool requiresLineBoxForContent(const RenderInline& flow, const LineInfo& lineInfo)
{
    RenderElement* parent = flow.parent();
    if (flow.document().inNoQuirksMode()) {
        const RenderStyle& flowStyle = lineStyle(flow, lineInfo);
        const RenderStyle& parentStyle = lineStyle(*parent, lineInfo);
        if (flowStyle.lineHeight() != parentStyle.lineHeight()
            || flowStyle.verticalAlign() != parentStyle.verticalAlign()
            || !parentStyle.fontCascade().metricsOfPrimaryFont().hasIdenticalAscentDescentAndLineGap(flowStyle.fontCascade().metricsOfPrimaryFont()))
        return true;
    }
    return false;
}

inline bool shouldCollapseWhiteSpace(const RenderStyle* style, const LineInfo& lineInfo, WhitespacePosition whitespacePosition)
{
    // CSS2 16.6.1
    // If a space (U+0020) at the beginning of a line has 'white-space' set to 'normal', 'nowrap', or 'pre-line', it is removed.
    // If a space (U+0020) at the end of a line has 'white-space' set to 'normal', 'nowrap', or 'pre-line', it is also removed.
    // If spaces (U+0020) or tabs (U+0009) at the end of a line have 'white-space' set to 'pre-wrap', UAs may visually collapse them.
    return style->collapseWhiteSpace()
        || (whitespacePosition == TrailingWhitespace && style->whiteSpace() == WhiteSpace::PreWrap && (!lineInfo.isEmpty() || !lineInfo.previousLineBrokeCleanly()));
}

inline bool skipNonBreakingSpace(const LegacyInlineIterator& it, const LineInfo& lineInfo)
{
    if (it.renderer()->style().nbspMode() != NBSPMode::Space || it.current() != noBreakSpace)
        return false;

    // FIXME: This is bad. It makes nbsp inconsistent with space and won't work correctly
    // with m_minWidth/m_maxWidth.
    // Do not skip a non-breaking space if it is the first character
    // on a line after a clean line break (or on the first line, since previousLineBrokeCleanly starts off
    // |true|).
    if (lineInfo.isEmpty() && lineInfo.previousLineBrokeCleanly())
        return false;

    return true;
}

inline bool alwaysRequiresLineBox(const RenderInline& flow)
{
    // FIXME: Right now, we only allow line boxes for inlines that are truly empty.
    // We need to fix this, though, because at the very least, inlines containing only
    // ignorable whitespace should should also have line boxes.
    return isEmptyInline(flow) && hasInlineDirectionBordersPaddingOrMargin(flow);
}

inline bool requiresLineBox(const LegacyInlineIterator& it, const LineInfo& lineInfo = LineInfo(), WhitespacePosition whitespacePosition = LeadingWhitespace)
{
    if (it.renderer()->isFloatingOrOutOfFlowPositioned())
        return false;

    if (it.renderer()->isBR())
        return true;

    bool rendererIsEmptyInline = false;
    if (auto* inlineRenderer = dynamicDowncast<RenderInline>(*it.renderer())) {
        if (!alwaysRequiresLineBox(*inlineRenderer) && !requiresLineBoxForContent(*inlineRenderer, lineInfo))
            return false;
        rendererIsEmptyInline = isEmptyInline(*inlineRenderer);
    }

    if (!shouldCollapseWhiteSpace(&it.renderer()->style(), lineInfo, whitespacePosition))
        return true;

    UChar current = it.current();
    bool notJustWhitespace = current != ' ' && current != '\t' && current != softHyphen && (current != '\n' || it.renderer()->preservesNewline()) && !skipNonBreakingSpace(it, lineInfo);
    return notJustWhitespace || rendererIsEmptyInline;
}

inline void setStaticPositions(RenderBlockFlow& block, RenderBox& child, IndentTextOrNot shouldIndentText)
{
    // FIXME: The math here is actually not really right. It's a best-guess approximation that
    // will work for the common cases
    RenderElement* containerBlock = child.container();
    LayoutUnit blockHeight = block.logicalHeight();
    if (auto* renderInline = dynamicDowncast<RenderInline>(*containerBlock)) {
        // A relative positioned inline encloses us. In this case, we also have to determine our
        // position as though we were an inline. Set |staticInlinePosition| and |staticBlockPosition| on the relative positioned
        // inline so that we can obtain the value later.
        renderInline->layer()->setStaticInlinePosition(block.startAlignedOffsetForLine(blockHeight, DoNotIndentText));
        renderInline->layer()->setStaticBlockPosition(blockHeight);
    }
    block.updateStaticInlinePositionForChild(child, blockHeight, shouldIndentText);
    child.layer()->setStaticBlockPosition(blockHeight);
}

} // namespace WebCore
