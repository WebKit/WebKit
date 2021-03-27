/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderFragmentContainerSet.h"

#include "RenderBoxFragmentInfo.h"
#include "RenderFragmentedFlow.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFragmentContainerSet);

RenderFragmentContainerSet::RenderFragmentContainerSet(Document& document, RenderStyle&& style, RenderFragmentedFlow& fragmentedFlow)
    : RenderFragmentContainer(document, WTFMove(style), &fragmentedFlow)
{
}

void RenderFragmentContainerSet::installFragmentedFlow()
{
    // We don't have to do anything, since we were able to connect the flow thread
    // in the constructor.
}

void RenderFragmentContainerSet::expandToEncompassFragmentedFlowContentsIfNeeded()
{
    // Whenever the last region is a set, it always expands its region rect to consume all
    // of the flow thread content. This is because it is always capable of generating an
    // infinite number of boxes in order to hold all of the remaining content.
    auto rect = fragmentedFlowPortionRect();
    
    // Get the offset within the flow thread in its block progression direction. Then get the
    // flow thread's remaining logical height including its overflow and expand our rect
    // to encompass that remaining height and overflow. The idea is that we will generate
    // additional columns and pages to hold that overflow, since people do write bad
    // content like <body style="height:0px"> in multi-column layouts.
    bool isHorizontal = fragmentedFlow()->isHorizontalWritingMode();
    auto logicalTopOffset = isHorizontal ? rect.y() : rect.x();
    auto overflowHeight = isHorizontal ? fragmentedFlow()->layoutOverflowRect().maxY() : fragmentedFlow()->layoutOverflowRect().maxX();
    auto logicalHeightWithOverflow = logicalTopOffset == RenderFragmentedFlow::maxLogicalHeight() ? overflowHeight : overflowHeight - logicalTopOffset;
    setFragmentedFlowPortionRect({ rect.x(), rect.y(), isHorizontal ? rect.width() : logicalHeightWithOverflow, isHorizontal ? logicalHeightWithOverflow : rect.height() });
}

}
