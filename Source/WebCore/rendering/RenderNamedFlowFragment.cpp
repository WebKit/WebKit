/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "RenderNamedFlowFragment.h"

#include "FlowThreadController.h"
#include "PaintInfo.h"
#include "RenderBoxRegionInfo.h"
#include "RenderFlowThread.h"
#include "RenderNamedFlowThread.h"
#include "RenderView.h"
#include "StyleResolver.h"

using namespace std;

namespace WebCore {

RenderNamedFlowFragment::RenderNamedFlowFragment(Document& document, PassRef<RenderStyle> style)
    : RenderRegion(document, std::move(style), nullptr)
{
}

RenderNamedFlowFragment::~RenderNamedFlowFragment()
{
}

PassRef<RenderStyle> RenderNamedFlowFragment::createStyle(const RenderStyle& parentStyle)
{
    auto style = RenderStyle::createAnonymousStyleWithDisplay(&parentStyle, BLOCK);

    style.get().setFlowThread(parentStyle.flowThread());
    style.get().setRegionThread(parentStyle.regionThread());
    style.get().setRegionFragment(parentStyle.regionFragment());
#if ENABLE(CSS_SHAPES)
    style.get().setShapeInside(parentStyle.shapeInside());
#endif

    return style;
}

void RenderNamedFlowFragment::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderRegion::styleDidChange(diff, oldStyle);
    if (!m_flowThread)
        return;

    if (parent() && parent()->needsLayout())
        setNeedsLayout(MarkOnlyThis);
}

bool RenderNamedFlowFragment::shouldHaveAutoLogicalHeight() const
{
    ASSERT(parent());

    const RenderStyle& styleToUse = parent()->style();
    bool hasSpecifiedEndpointsForHeight = styleToUse.logicalTop().isSpecified() && styleToUse.logicalBottom().isSpecified();
    bool hasAnchoredEndpointsForHeight = parent()->isOutOfFlowPositioned() && hasSpecifiedEndpointsForHeight;
    return styleToUse.logicalHeight().isAuto() && !hasAnchoredEndpointsForHeight;
}

LayoutUnit RenderNamedFlowFragment::maxPageLogicalHeight() const
{
    ASSERT(m_flowThread);
    ASSERT(hasAutoLogicalHeight() && m_flowThread->inMeasureContentLayoutPhase());
    ASSERT(isAnonymous());
    ASSERT(parent());

    const RenderStyle& styleToUse = parent()->style();
    return styleToUse.logicalMaxHeight().isUndefined() ? RenderFlowThread::maxLogicalHeight() : toRenderBlock(parent())->computeReplacedLogicalHeightUsing(styleToUse.logicalMaxHeight());
}

} // namespace WebCore
