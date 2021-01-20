/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "RenderIFrame.h"

#include "Frame.h"
#include "FrameView.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "RenderView.h"
#include "Settings.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderIFrame);

using namespace HTMLNames;
    
RenderIFrame::RenderIFrame(HTMLIFrameElement& element, RenderStyle&& style)
    : RenderFrameBase(element, WTFMove(style))
{
}

HTMLIFrameElement& RenderIFrame::iframeElement() const
{
    return downcast<HTMLIFrameElement>(RenderFrameBase::frameOwnerElement());
}

bool RenderIFrame::shouldComputeSizeAsReplaced() const
{
    return true;
}

bool RenderIFrame::isInlineBlockOrInlineTable() const
{
    return isInline();
}

bool RenderIFrame::requiresLayer() const
{
    return RenderFrameBase::requiresLayer() || style().resize() != Resize::None;
}

RenderView* RenderIFrame::contentRootRenderer() const
{
    FrameView* childFrameView = childView();
    return childFrameView ? childFrameView->frame().contentRenderer() : 0;
}

bool RenderIFrame::isFullScreenIFrame() const
{
    // Some authors implement fullscreen popups as out-of-flow iframes with size set to full viewport (using vw/vh units).
    // The size used may not perfectly match the viewport size so the following heuristic uses a relaxed constraint.
    return style().hasOutOfFlowPosition() && style().hasViewportUnits();
}

bool RenderIFrame::flattenFrame() const
{
    if (view().frameView().effectiveFrameFlattening() == FrameFlattening::Disabled)
        return false;

    if (style().width().isFixed() && style().height().isFixed()) {
        // Do not flatten iframes with scrolling="no".
        if (iframeElement().scrollingMode() == ScrollbarAlwaysOff)
            return false;
        // Do not flatten iframes that have zero size, as flattening might make them visible.
        if (style().width().value() <= 0 || style().height().value() <= 0)
            return false;
        // Do not flatten "fullscreen" iframes or they could become larger than the viewport.
        if (view().frameView().effectiveFrameFlattening() <= FrameFlattening::EnabledForNonFullScreenIFrames && isFullScreenIFrame())
            return false;
    }

    // Do not flatten offscreen inner frames during frame flattening, as flattening might make them visible.
    IntRect boundingRect = absoluteBoundingBoxRectIgnoringTransforms();
    return boundingRect.maxX() > 0 && boundingRect.maxY() > 0;
}

void RenderIFrame::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    updateLogicalWidth();
    // No kids to layout as a replaced element.
    updateLogicalHeight();

    if (flattenFrame())
        layoutWithFlattening(style().width().isFixed(), style().height().isFixed());

    clearOverflow();
    addVisualEffectOverflow();
    updateLayerTransform();

    clearNeedsLayout();
}

}
