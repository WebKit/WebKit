/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "_WKRenderingProgressEvents.h"

#import <WebCore/LayoutMilestone.h>

static inline _WKRenderingProgressEvents renderingProgressEvents(OptionSet<WebCore::LayoutMilestone> milestones)
{
    _WKRenderingProgressEvents events = 0;

    if (milestones & WebCore::DidFirstLayout)
        events |= _WKRenderingProgressEventFirstLayout;

    if (milestones & WebCore::DidFirstVisuallyNonEmptyLayout)
        events |= _WKRenderingProgressEventFirstVisuallyNonEmptyLayout;

    if (milestones & WebCore::DidHitRelevantRepaintedObjectsAreaThreshold)
        events |= _WKRenderingProgressEventFirstPaintWithSignificantArea;

    if (milestones & WebCore::ReachedSessionRestorationRenderTreeSizeThreshold)
        events |= _WKRenderingProgressEventReachedSessionRestorationRenderTreeSizeThreshold;

    if (milestones & WebCore::DidFirstLayoutAfterSuppressedIncrementalRendering)
        events |= _WKRenderingProgressEventFirstLayoutAfterSuppressedIncrementalRendering;

    if (milestones & WebCore::DidFirstPaintAfterSuppressedIncrementalRendering)
        events |= _WKRenderingProgressEventFirstPaintAfterSuppressedIncrementalRendering;

    if (milestones & WebCore::DidRenderSignificantAmountOfText)
        events |= _WKRenderingProgressEventDidRenderSignificantAmountOfText;

    if (milestones & WebCore::DidFirstMeaningfulPaint)
        events |= _WKRenderingProgressEventFirstMeaningfulPaint;

    return events;
}
