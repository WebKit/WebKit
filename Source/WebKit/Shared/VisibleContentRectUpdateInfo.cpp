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

#include "config.h"

#if ENABLE(UI_SIDE_COMPOSITING)

#include "VisibleContentRectUpdateInfo.h"

#include <wtf/text/TextStream.h>

namespace WebKit {
using namespace WebCore;

String VisibleContentRectUpdateInfo::dump() const
{
    TextStream stream;
    stream << *this;
    return stream.release();
}

TextStream& operator<<(TextStream& ts, ViewStabilityFlag stabilityFlag)
{
    switch (stabilityFlag) {
    case ViewStabilityFlag::ScrollViewInteracting: ts << "scroll view interacting"_s; break;
    case ViewStabilityFlag::ScrollViewAnimatedScrollOrZoom: ts << "scroll view animated scroll or zoom"_s; break;
    case ViewStabilityFlag::ScrollViewRubberBanding: ts << "scroll view rubberbanding"_s; break;
    case ViewStabilityFlag::ChangingObscuredInsetsInteractively: ts << "changing obscured insets interactively"_s; break;
    case ViewStabilityFlag::UnstableForTesting: ts << "unstable for testing"_s; break;
    }
    return ts;
}

TextStream& operator<<(TextStream& ts, const VisibleContentRectUpdateInfo& info)
{
    TextStream::GroupScope scope(ts);
    
    ts << "VisibleContentRectUpdateInfo"_s;

    ts.dumpProperty("lastLayerTreeTransactionID"_s, info.lastLayerTreeTransactionID());

    ts.dumpProperty("exposedContentRect"_s, info.exposedContentRect());
    ts.dumpProperty("unobscuredContentRect"_s, info.unobscuredContentRect());
    ts.dumpProperty("contentInsets"_s, info.contentInsets());
    ts.dumpProperty("unobscuredContentRectRespectingInputViewBounds"_s, info.unobscuredContentRectRespectingInputViewBounds());
    ts.dumpProperty("unobscuredRectInScrollViewCoordinates"_s, info.unobscuredRectInScrollViewCoordinates());
    ts.dumpProperty("layoutViewportRect"_s, info.layoutViewportRect());
    ts.dumpProperty("obscuredInsets"_s, info.obscuredInsets());
    ts.dumpProperty("unobscuredSafeAreaInsets"_s, info.unobscuredSafeAreaInsets());

    ts.dumpProperty("scale"_s, info.scale());
    ts.dumpProperty("viewStability"_s, info.viewStability());
    ts.dumpProperty("isFirstUpdateForNewViewSize"_s, info.isFirstUpdateForNewViewSize());
    if (info.enclosedInScrollableAncestorView())
        ts.dumpProperty("enclosedInScrollableAncestorView"_s, info.enclosedInScrollableAncestorView());

    ts.dumpProperty("allowShrinkToFit"_s, info.allowShrinkToFit());
    ts.dumpProperty("scrollVelocity"_s, info.scrollVelocity());

    return ts;
}

} // namespace WebKit

#endif // ENABLE(UI_SIDE_COMPOSITING)
