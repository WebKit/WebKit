/*
 * Copyright (C) 2017,2018 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS IN..0TERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderLinesClampFlow.h"

#include "RenderChildIterator.h"
#include "RenderLinesClampSet.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderLinesClampFlow);

RenderLinesClampFlow::RenderLinesClampFlow(Document& document, RenderStyle&& style)
    : RenderMultiColumnFlow(document, WTFMove(style))
{
}

RenderLinesClampFlow::~RenderLinesClampFlow() = default;

const char* RenderLinesClampFlow::renderName() const
{    
    return "RenderLinesClampFlow";
}

RenderPtr<RenderMultiColumnSet> RenderLinesClampFlow::createMultiColumnSet(RenderStyle&& style)
{
    return createRenderer<RenderLinesClampSet>(*this, WTFMove(style));
}

bool RenderLinesClampFlow::isChildAllowedInFragmentedFlow(const RenderBlockFlow& parent, const RenderElement& renderElement) const
{
    const auto& idAttr = renderElement.element() ? renderElement.element()->getIdAttribute() : nullAtom();
    const auto& clampAttr = parent.style().linesClamp().center();
    return clampAttr == nullAtom() || clampAttr != idAttr;
}

void RenderLinesClampFlow::layoutFlowExcludedObjects(bool relayoutChildren)
{
    auto* clampContainer = multiColumnBlockFlow();
    if (!clampContainer)
        return;
    auto* clampSet = firstMultiColumnSet();
    if (!clampSet || !is<RenderLinesClampSet>(clampSet))
        return;
    auto& linesClampSet = downcast<RenderLinesClampSet>(*clampSet);
    
    for (auto& sibling : childrenOfType<RenderElement>(*clampContainer)) {
        if (isChildAllowedInFragmentedFlow(*clampContainer, sibling) || !is<RenderBox>(sibling))
            continue;

        auto& siblingBox = downcast<RenderBox>(sibling);
        
        siblingBox.setIsExcludedFromNormalLayout(true);
        
        auto marginBefore = clampContainer->marginBeforeForChild(siblingBox);
        auto marginAfter = clampContainer->marginAfterForChild(siblingBox);
        
        setLogicalTopForChild(siblingBox, clampContainer->borderAndPaddingBefore() + linesClampSet.startPageHeight() + marginBefore);
        
        if (relayoutChildren)
            siblingBox.setChildNeedsLayout(MarkOnlyThis);
        
        if (siblingBox.needsLayout())
            siblingBox.layout();
        clampContainer->determineLogicalLeftPositionForChild(siblingBox);
        
        linesClampSet.setMiddleObjectHeight(marginBefore + marginAfter + clampContainer->logicalHeightForChild(siblingBox));
    }
}

}
