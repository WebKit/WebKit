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

RenderNamedFlowFragment::RenderNamedFlowFragment(Document& document)
    : RenderRegion(document, nullptr)
{
}

RenderNamedFlowFragment::~RenderNamedFlowFragment()
{
}

void RenderNamedFlowFragment::setStyleForNamedFlowFragment(const RenderStyle* parentStyle)
{
    RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, BLOCK);

    newStyle->setFlowThread(parentStyle->flowThread());
    newStyle->setRegionThread(parentStyle->regionThread());
    newStyle->setRegionFragment(parentStyle->regionFragment());
#if ENABLE(CSS_SHAPES)
    newStyle->setShapeInside(parentStyle->shapeInside());
#endif

    setStyle(newStyle.release());
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

    RenderStyle* styleToUse = parent()->style();
    bool hasSpecifiedEndpointsForHeight = styleToUse->logicalTop().isSpecified() && styleToUse->logicalBottom().isSpecified();
    bool hasAnchoredEndpointsForHeight = parent()->isOutOfFlowPositioned() && hasSpecifiedEndpointsForHeight;
    return styleToUse->logicalHeight().isAuto() && !hasAnchoredEndpointsForHeight;
}

LayoutUnit RenderNamedFlowFragment::maxPageLogicalHeight() const
{
    ASSERT(m_flowThread);
    ASSERT(hasAutoLogicalHeight() && m_flowThread->inMeasureContentLayoutPhase());
    ASSERT(isAnonymous());
    ASSERT(parent());

    RenderStyle* styleToUse = parent()->style();
    return styleToUse->logicalMaxHeight().isUndefined() ? RenderFlowThread::maxLogicalHeight() : toRenderBlock(parent())->computeReplacedLogicalHeightUsing(styleToUse->logicalMaxHeight());    
}

} // namespace WebCore
