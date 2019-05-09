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
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineElementBox);

void InlineElementBox::deleteLine()
{
    if (!extracted()) {
        if (is<RenderBox>(renderer()))
            downcast<RenderBox>(renderer()).setInlineBoxWrapper(nullptr);
        else if (is<RenderLineBreak>(renderer()))
            downcast<RenderLineBreak>(renderer()).setInlineBoxWrapper(nullptr);
    }
    delete this;
}

void InlineElementBox::extractLine()
{
    setExtracted(true);
    if (is<RenderBox>(renderer()))
        downcast<RenderBox>(renderer()).setInlineBoxWrapper(nullptr);
    else if (is<RenderLineBreak>(renderer()))
        downcast<RenderLineBreak>(renderer()).setInlineBoxWrapper(nullptr);
}

void InlineElementBox::attachLine()
{
    setExtracted(false);
    if (is<RenderBox>(renderer()))
        downcast<RenderBox>(renderer()).setInlineBoxWrapper(this);
    else if (is<RenderLineBreak>(renderer()))
        downcast<RenderLineBreak>(renderer()).setInlineBoxWrapper(this);
}

void InlineElementBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()))
        return;

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection && paintInfo.phase != PaintPhase::EventRegion)
        return;

    LayoutPoint childPoint = paintOffset;
    if (is<RenderBox>(renderer()) && parent()->renderer().style().isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = renderer().containingBlock()->flipForWritingModeForChild(&downcast<RenderBox>(renderer()), childPoint);

    renderer().paintAsInlineBlock(paintInfo, childPoint);
}

bool InlineElementBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/,
    HitTestAction)
{
    // Hit test all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    LayoutPoint childPoint = accumulatedOffset;
    if (is<RenderBox>(renderer()) && parent()->renderer().style().isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = renderer().containingBlock()->flipForWritingModeForChild(&downcast<RenderBox>(renderer()), childPoint);

    return renderer().hitTest(request, result, locationInContainer, childPoint);
}

}
