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

#ifndef SimpleLineLayoutFunctions_h
#define SimpleLineLayoutFunctions_h

#include "LayoutRect.h"
#include "RenderBlockFlow.h"
#include "RenderText.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

class HitTestLocation;
class HitTestRequest;
class HitTestResult;
class RenderBlockFlow;
class RenderText;
struct PaintInfo;

namespace SimpleLineLayout {

LayoutUnit computeFlowHeight(const RenderBlockFlow&, const Layout&);
LayoutUnit computeFlowFirstLineBaseline(const RenderBlockFlow&, const Layout&);
LayoutUnit computeFlowLastLineBaseline(const RenderBlockFlow&, const Layout&);

void paintFlow(const RenderBlockFlow&, const Layout&, PaintInfo&, const LayoutPoint& paintOffset);
bool hitTestFlow(const RenderBlockFlow&, const Layout&, const HitTestRequest&, HitTestResult&, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, HitTestAction);
void collectFlowOverflow(RenderBlockFlow&, const Layout&);

bool isTextRendered(const RenderText&, const Layout&);
bool containsTextCaretOffset(const RenderText&, const Layout&, unsigned);
unsigned findTextCaretMinimumOffset(const RenderText&, const Layout&);
unsigned findTextCaretMaximumOffset(const RenderText&, const Layout&);
IntRect computeTextBoundingBox(const RenderText&, const Layout&);

LayoutUnit lineHeightFromFlow(const RenderBlockFlow&);
LayoutUnit baselineFromFlow(const RenderBlockFlow&);

}

namespace SimpleLineLayout {

inline LayoutUnit computeFlowHeight(const RenderBlockFlow& flow, const Layout& layout)
{
    return lineHeightFromFlow(flow) * layout.lineCount;
}

inline LayoutUnit computeFlowFirstLineBaseline(const RenderBlockFlow& flow, const Layout& layout)
{
    ASSERT_UNUSED(layout, !layout.runs.isEmpty());
    return flow.borderAndPaddingBefore() + baselineFromFlow(flow);
}

inline LayoutUnit computeFlowLastLineBaseline(const RenderBlockFlow& flow, const Layout& layout)
{
    ASSERT(!layout.runs.isEmpty());
    return flow.borderAndPaddingBefore() + lineHeightFromFlow(flow) * (layout.runs.size() - 1) + baselineFromFlow(flow);
}

inline unsigned findTextCaretMinimumOffset(const RenderText&, const Layout& layout)
{
    if (layout.runs.isEmpty())
        return 0;
    return layout.runs[0].textOffset;
}

inline unsigned findTextCaretMaximumOffset(const RenderText& renderer, const Layout& layout)
{
    if (layout.runs.isEmpty())
        return renderer.textLength();
    auto& last = layout.runs[layout.runs.size() - 1];
    return last.textOffset + last.textLength;
}

inline bool containsTextCaretOffset(const RenderText&, const Layout& layout, unsigned offset)
{
    for (unsigned i = 0; i < layout.runs.size(); ++i) {
        auto& line = layout.runs[i];
        if (offset < line.textOffset)
            return false;
        if (offset <= line.textOffset + line.textLength)
            return true;
    }
    return false;
}

inline bool isTextRendered(const RenderText&, const Layout& layout)
{
    for (unsigned i = 0; i < layout.runs.size(); ++i) {
        if (layout.runs[i].textLength)
            return true;
    }
    return false;
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

#endif
