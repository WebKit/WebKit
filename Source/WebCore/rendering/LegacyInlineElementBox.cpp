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
#include "LegacyInlineElementBox.h"

#include "LegacyInlineFlowBox.h"
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderLineBreak.h"
#include "RenderStyleInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyInlineElementBox);

void LegacyInlineElementBox::deleteLine()
{
    if (!extracted()) {
        if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer()))
            box->setInlineBoxWrapper(nullptr);
        else if (CheckedPtr lineBreak = dynamicDowncast<RenderLineBreak>(renderer()))
            lineBreak->setInlineBoxWrapper(nullptr);
    }
    delete this;
}

void LegacyInlineElementBox::extractLine()
{
    setExtracted(true);
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer()))
        box->setInlineBoxWrapper(nullptr);
    else if (CheckedPtr lineBreak = dynamicDowncast<RenderLineBreak>(renderer()))
        lineBreak->setInlineBoxWrapper(nullptr);
}

void LegacyInlineElementBox::attachLine()
{
    setExtracted(false);
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer()))
        box->setInlineBoxWrapper(this);
    else if (CheckedPtr lineBreak = dynamicDowncast<RenderLineBreak>(renderer()))
        lineBreak->setInlineBoxWrapper(this);
}

void LegacyInlineElementBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/)
{
    if (!paintInfo.shouldPaintWithinRoot(renderer()))
        return;

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Selection && paintInfo.phase != PaintPhase::EventRegion && paintInfo.phase != PaintPhase::Accessibility)
        return;

    LayoutPoint childPoint = paintOffset;
    if (auto* box = dynamicDowncast<RenderBox>(renderer()); box && parent()->renderer().style().isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = renderer().containingBlock()->flipForWritingModeForChild(*box, childPoint);

    renderer().paintAsInlineBlock(paintInfo, childPoint);
}

bool LegacyInlineElementBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit /* lineTop */, LayoutUnit /*lineBottom*/,
    HitTestAction)
{
    // Hit test all phases of replaced elements atomically, as though the replaced element established its
    // own stacking context.  (See Appendix E.2, section 6.4 on inline block/table elements in the CSS2.1
    // specification.)
    LayoutPoint childPoint = accumulatedOffset;
    if (auto* box = dynamicDowncast<RenderBox>(renderer()); box && parent()->renderer().style().isFlippedBlocksWritingMode()) // Faster than calling containingBlock().
        childPoint = renderer().containingBlock()->flipForWritingModeForChild(*box, childPoint);

    return renderer().hitTest(request, result, locationInContainer, childPoint);
}

}
