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

#include "config.h"
#include "InlineElementBox.h"

#include "InlineFlowBox.h"
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderLineBreak.h"

namespace WebCore {

void InlineElementBox::deleteLine()
{
    if (!extracted()) {
        if (renderer().isBox())
            toRenderBox(renderer()).setInlineBoxWrapper(nullptr);
        else if (renderer().isLineBreak())
            toRenderLineBreak(renderer()).setInlineBoxWrapper(nullptr);
    }
    delete this;
}

void InlineElementBox::extractLine()
{
    setExtracted(true);
    if (renderer().isBox())
        toRenderBox(renderer()).setInlineBoxWrapper(nullptr);
    else if (renderer().isLineBreak())
        toRenderLineBreak(renderer()).setInlineBoxWrapper(nullptr);
}

void InlineElementBox::attachLine()
{
    setExtracted(false);
    if (renderer().isBox())
        toRenderBox(renderer()).setInlineBoxWrapper(this);
    else if (renderer().isLineBreak())
        toRenderLineBreak(renderer()).setInlineBoxWrapper(this);
}

void InlineElementBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()) || (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection))
        return;

    LayoutPoint childPoint = paintOffset;
    if (renderer().isBox() && parent()->renderer().style().isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = renderer().containingBlock()->flipForWritingModeForChild(&toRenderBox(renderer()), childPoint);

    // Paint all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    bool preservePhase = paintInfo.phase == PaintPhaseSelection || paintInfo.phase == PaintPhaseTextClip;
    PaintInfo info(paintInfo);
    info.phase = preservePhase ? paintInfo.phase : PaintPhaseBlockBackground;
    renderer().paint(info, childPoint);
    if (!preservePhase) {
        info.phase = PaintPhaseChildBlockBackgrounds;
        renderer().paint(info, childPoint);
        info.phase = PaintPhaseFloat;
        renderer().paint(info, childPoint);
        info.phase = PaintPhaseForeground;
        renderer().paint(info, childPoint);
        info.phase = PaintPhaseOutline;
        renderer().paint(info, childPoint);
    }
}

bool InlineElementBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    // Hit test all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    LayoutPoint childPoint = accumulatedOffset;
    if (renderer().isBox() && parent()->renderer().style().isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = renderer().containingBlock()->flipForWritingModeForChild(&toRenderBox(renderer()), childPoint);

    return renderer().hitTest(request, result, locationInContainer, childPoint);
}

}
