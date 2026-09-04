/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "RemoteFrameLayoutInfo.h"

#include "FloatRect.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteFrameLayoutInfo);

RemoteFrameLayoutInfo::RemoteFrameLayoutInfo(
    std::optional<LayoutRect> visibleRectInParent,
    IntRect onScreenRectInChildView,
#if PLATFORM(IOS_FAMILY)
    FloatRect exposedContentRectInChildView,
#endif
    bool ownerHasRenderer,
    TransformationMatrix childFrameOwnerToRootContentTransform,
    TransformationMatrix absoluteToChildFrameOwnerLocalTransform,
    float usedZoom,
    LayoutPoint contentBoxLocation,
    OptionSet<FrameOwnerElementAppearance> ownerElementAppearance
)
    : m_visibleRectInParent(visibleRectInParent)
    , m_onScreenRectInChildView(onScreenRectInChildView)
#if PLATFORM(IOS_FAMILY)
    , m_exposedContentRectInChildView(exposedContentRectInChildView)
#endif
    , m_ownerHasRenderer(ownerHasRenderer)
    , m_childFrameOwnerToRootContentTransform(WTF::move(childFrameOwnerToRootContentTransform))
    , m_absoluteToChildFrameOwnerLocalTransform(WTF::move(absoluteToChildFrameOwnerLocalTransform))
    , m_usedZoom(usedZoom)
    , m_contentBoxLocation(contentBoxLocation)
    , m_ownerElementAppearance(ownerElementAppearance)
{
}

WTF::TextStream& operator<<(WTF::TextStream& ts, FrameOwnerElementAppearance appearance)
{
    switch (appearance) {
    case FrameOwnerElementAppearance::IsDark:
        ts << "IsDark"_s;
        break;
    case FrameOwnerElementAppearance::ExplicitlySet:
        ts << "ExplicitlySet"_s;
        break;
    }
    return ts;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, const RemoteFrameLayoutInfo& info)
{
    WTF::TextStream::GroupScope scope(ts);
    ts << "RemoteFrameLayoutInfo"_s;
    ts.dumpProperty("visibleRectInParent"_s, info.visibleRectInParent());
    ts.dumpProperty("onScreenRectInChildView"_s, info.onScreenRectInChildView());
#if PLATFORM(IOS_FAMILY)
    ts.dumpProperty("exposedContentRectInChildView"_s, info.exposedContentRectInChildView());
#endif
    ts.dumpProperty("ownerHasRenderer"_s, info.ownerHasRenderer());
    ts.dumpProperty("childFrameOwnerToRootContentTransform"_s, info.childFrameOwnerToRootContentTransform());
    ts.dumpProperty("absoluteToChildFrameOwnerLocalTransform"_s, info.absoluteToChildFrameOwnerLocalTransform());
    ts.dumpProperty("usedZoom"_s, info.usedZoom());
    ts.dumpProperty("contentBoxLocation"_s, info.contentBoxLocation());
    ts.dumpProperty("ownerElementAppearance"_s, info.ownerElementAppearance());
    return ts;
}

} // namespace WebCore
