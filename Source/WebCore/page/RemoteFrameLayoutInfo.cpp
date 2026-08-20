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

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteFrameLayoutInfo);

RemoteFrameLayoutInfo::RemoteFrameLayoutInfo(
    LayoutRect windowClipRectInParent,
    std::optional<LayoutRect> visibleRectInParent,
#if PLATFORM(IOS_FAMILY)
    std::optional<LayoutRect> exposedContentRectInParent,
#endif
    bool ownerHasRenderer,
    TransformationMatrix childFrameOwnerToRootContentTransform,
    TransformationMatrix absoluteToChildFrameOwnerLocalTransform,
    float usedZoom,
    LayoutPoint contentBoxLocation,
    OptionSet<FrameOwnerElementAppearance> ownerElementAppearance
)
    : m_windowClipRectInParent(windowClipRectInParent)
    , m_visibleRectInParent(visibleRectInParent)
#if PLATFORM(IOS_FAMILY)
    , m_exposedContentRectInParent(exposedContentRectInParent)
#endif
    , m_ownerHasRenderer(ownerHasRenderer)
    , m_childFrameOwnerToRootContentTransform(WTF::move(childFrameOwnerToRootContentTransform))
    , m_absoluteToChildFrameOwnerLocalTransform(WTF::move(absoluteToChildFrameOwnerLocalTransform))
    , m_usedZoom(usedZoom)
    , m_contentBoxLocation(contentBoxLocation)
    , m_ownerElementAppearance(ownerElementAppearance)
{
}

std::optional<FloatRect> RemoteFrameLayoutInfo::mapParentContentsToChildWindow(const LayoutRect& rectInParent) const
{
    // A non-affine owner transform (a 3D transform, say) has no meaningful rect inverse, so report
    // that the rect is unknown.
    if (!m_absoluteToChildFrameOwnerLocalTransform.isAffine())
        return std::nullopt;

    // Inverse of LocalFrameView::visibleRectOfChild(): visibleRectInParent is in the parent document's
    // coordinates. Map it into the iframe owner element's local space, subtract the owner content-box
    // offset so the rect is relative to the iframe content origin, and undo the owner's used CSS zoom so
    // the result is in the child frame's unzoomed root-content coordinates (its RenderView space).
    auto ownerLocal = m_absoluteToChildFrameOwnerLocalTransform.mapRect(FloatRect { rectInParent });
    ownerLocal.moveBy(-FloatPoint { m_contentBoxLocation });
    if (m_usedZoom > 0)
        ownerLocal.scale(1.0f / m_usedZoom);
    return ownerLocal;
}

} // namespace WebCore
