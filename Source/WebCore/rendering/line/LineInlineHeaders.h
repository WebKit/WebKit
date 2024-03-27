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
        || (whitespacePosition == TrailingWhitespace && style->whiteSpace() == WhiteSpace::PreWrap && !lineInfo.isEmpty());
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
    if (lineInfo.isEmpty())
        return false;

    return true;
}

inline bool requiresLineBox(const LegacyInlineIterator& it, const LineInfo& lineInfo = LineInfo(), WhitespacePosition whitespacePosition = LeadingWhitespace)
{
    bool rendererIsEmptyInline = false;
    if (auto* inlineRenderer = dynamicDowncast<RenderInline>(*it.renderer())) {
        if (!requiresLineBoxForContent(*inlineRenderer, lineInfo))
            return false;
        rendererIsEmptyInline = isEmptyInline(*inlineRenderer);
    }

    if (!shouldCollapseWhiteSpace(&it.renderer()->style(), lineInfo, whitespacePosition))
        return true;

    UChar current = it.current();
    bool notJustWhitespace = current != ' ' && current != '\t' && current != softHyphen && (current != '\n' || it.renderer()->preservesNewline()) && !skipNonBreakingSpace(it, lineInfo);
    return notJustWhitespace || rendererIsEmptyInline;
}

} // namespace WebCore
