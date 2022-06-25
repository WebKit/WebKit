/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/OptionSet.h>

namespace WebCore {

// FIXME: Some of these milestones are about layout, and others are about painting.
// We should either re-name them to something more generic, or split them into
// two enums -- one for painting and one for layout.
enum LayoutMilestone {
    DidFirstLayout                                      = 1 << 0,
    DidFirstVisuallyNonEmptyLayout                      = 1 << 1,
    DidHitRelevantRepaintedObjectsAreaThreshold         = 1 << 2,
    DidFirstFlushForHeaderLayer                         = 1 << 3,
    DidFirstLayoutAfterSuppressedIncrementalRendering   = 1 << 4,
    DidFirstPaintAfterSuppressedIncrementalRendering    = 1 << 5,
    ReachedSessionRestorationRenderTreeSizeThreshold    = 1 << 6, // FIXME: only implemented by WK2 currently.
    DidRenderSignificantAmountOfText                    = 1 << 7,
    DidFirstMeaningfulPaint                             = 1 << 8,
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::LayoutMilestone> {
    using values = EnumValues<
        WebCore::LayoutMilestone,
        WebCore::LayoutMilestone::DidFirstLayout,
        WebCore::LayoutMilestone::DidFirstVisuallyNonEmptyLayout,
        WebCore::LayoutMilestone::DidHitRelevantRepaintedObjectsAreaThreshold,
        WebCore::LayoutMilestone::DidFirstFlushForHeaderLayer,
        WebCore::LayoutMilestone::DidFirstLayoutAfterSuppressedIncrementalRendering,
        WebCore::LayoutMilestone::DidFirstPaintAfterSuppressedIncrementalRendering,
        WebCore::LayoutMilestone::ReachedSessionRestorationRenderTreeSizeThreshold,
        WebCore::LayoutMilestone::DidRenderSignificantAmountOfText,
        WebCore::LayoutMilestone::DidFirstMeaningfulPaint
    >;
};

} // namespace WTF
