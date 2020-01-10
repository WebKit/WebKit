/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RenderBlockFlow.h"
#include "RenderText.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class RenderBlockFlow;
struct PaintInfo;

namespace SimpleLineLayout {
class FlowContents;

LayoutUnit computeFlowHeight(const RenderBlockFlow&, const Layout&);
LayoutUnit computeFlowFirstLineBaseline(const RenderBlockFlow&, const Layout&);
LayoutUnit computeFlowLastLineBaseline(const RenderBlockFlow&, const Layout&);
FloatRect computeOverflow(const RenderBlockFlow&, const FloatRect&);

void paintFlow(const RenderBlockFlow&, const Layout&, PaintInfo&, const LayoutPoint& paintOffset);
bool hitTestFlow(const RenderBlockFlow&, const Layout&, const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
void collectFlowOverflow(RenderBlockFlow&, const Layout&);

unsigned textOffsetForPoint(const LayoutPoint&, const RenderText&, const Layout&);
Vector<FloatQuad> collectAbsoluteQuadsForRange(const RenderObject&, unsigned start, unsigned end, const Layout&, bool ignoreEmptyTextSelections, bool* wasFixed);

LayoutUnit lineHeightFromFlow(const RenderBlockFlow&);
LayoutUnit baselineFromFlow(const RenderBlockFlow&);

bool canUseForLineBoxTree(RenderBlockFlow&, const Layout&);
void generateLineBoxTree(RenderBlockFlow&, const Layout&);

void simpleLineLayoutWillBeDeleted(const Layout&);

#if ENABLE(TREE_DEBUGGING)
void outputLineLayoutForFlow(WTF::TextStream&, const RenderBlockFlow&, const Layout&, int depth);
#endif

}

namespace SimpleLineLayout {

inline LayoutUnit computeFlowHeight(const RenderBlockFlow& flow, const Layout& layout)
{
    auto flowHeight = lineHeightFromFlow(flow) * layout.lineCount();
    if (!layout.hasLineStruts())
        return flowHeight;
    for (auto& strutEntry : layout.struts())
        flowHeight += strutEntry.offset;
    return flowHeight;
}

inline LayoutUnit computeFlowFirstLineBaseline(const RenderBlockFlow& flow, const Layout& layout)
{
    ASSERT_UNUSED(layout, layout.lineCount());
    return flow.borderAndPaddingBefore() + baselineFromFlow(flow);
}

inline LayoutUnit computeFlowLastLineBaseline(const RenderBlockFlow& flow, const Layout& layout)
{
    ASSERT(layout.lineCount());
    return flow.borderAndPaddingBefore() + lineHeightFromFlow(flow) * (layout.lineCount() - 1) + baselineFromFlow(flow);
}

inline LayoutUnit lineHeightFromFlow(const RenderBlockFlow& flow)
{
    return flow.lineHeight(false, HorizontalLine, PositionOfInteriorLineBoxes);
}

inline LayoutUnit baselineFromFlow(const RenderBlockFlow& flow)
{
    return flow.baselinePosition(AlphabeticBaseline, false, HorizontalLine, PositionOfInteriorLineBoxes);
}

}
}
