/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/FrameView.h>
#include <WebCore/RemoteFrame.h>

#include <wtf/TZoneMalloc.h>

namespace WebCore {

class RemoteFrame;

class RemoteFrameView final : public FrameView {
    WTF_MAKE_TZONE_ALLOCATED(RemoteFrameView);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RemoteFrameView);
public:
    static Ref<RemoteFrameView> create(RemoteFrame& frame) { return adoptRef(*new RemoteFrameView(frame)); }

    Type viewType() const final { return Type::Remote; }
    void writeRenderTreeAsText(TextStream&, OptionSet<RenderAsTextFlag>) override;
    RemoteFrame& frame() const final { return m_frame; }

    NODELETE WEBCORE_EXPORT LayoutRect layoutViewportRect() const final;
    WEBCORE_EXPORT IntSize contentsSize() const final;
    std::optional<LayoutRect> visibleRectOfChild(const Frame&) const final;

    // Set the frame rectangle, like setFrameRect, without synching the new rect to other Local/RemoteFrameViews.
    // When frameRect of a RemoteFrameView changes, it syncs the new rect to other Local/RemoteFrameViews.
    // RemoteFrameViews on the receiving end will set using this method to avoid repeating the sync.
    WEBCORE_EXPORT void setFrameRectWithoutSync(const IntRect&);

    OptionSet<FrameOwnerElementAppearance> appearanceOfOwnerElementOfChildFrame(const Frame&) const final;
    LayoutPoint childFrameOwnerContentBoxLocation(const Frame&) const final;
    TransformationMatrix childFrameOwnerToRootContentTransform(const Frame&) const final;
    TransformationMatrix absoluteToChildFrameOwnerLocalTransform(const Frame&) const final;

private:
    WEBCORE_EXPORT RemoteFrameView(RemoteFrame&);

    bool isRemoteFrameView() const final { return true; }
    NODELETE bool isScrollableOrRubberbandable() final;
    NODELETE bool hasScrollableOrRubberbandableAncestor() final;
    NODELETE bool shouldPlaceVerticalScrollbarOnLeft() const final;
    NODELETE void invalidateScrollbarRect(Scrollbar&, const IntRect&) final;
    IntRect windowClipRect() const final;
    void paintContents(GraphicsContext&, const IntRect& damageRect, SecurityOriginPaintPolicy, RegionContext*) final;
    NODELETE void addedOrRemovedScrollbar() final;
    NODELETE void delegatedScrollingModeDidChange() final;
    NODELETE void updateScrollCorner() final;
    NODELETE bool scrollContentsFastPath(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect) final;
    NODELETE void scrollContentsSlowPath(const IntRect& updateRect) final;
    NODELETE bool isVerticalDocument() const final;
    NODELETE bool isFlippedDocument() const final;
    NODELETE bool shouldDeferScrollUpdateAfterContentSizeChange() final;
    NODELETE void scrollOffsetChangedViaPlatformWidgetImpl(const ScrollOffset&, const ScrollOffset&) final;
    NODELETE void unobscuredContentSizeChanged() final;
    NODELETE void didFinishProhibitingScrollingWhenChangingContentSize() final;
    NODELETE void updateLayerPositionsAfterScrolling() final;
    NODELETE void updateCompositingLayersAfterScrolling() final;

    void setFrameRect(const IntRect&) final;

    const Ref<RemoteFrame> m_frame;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::RemoteFrameView)
static bool isType(const WebCore::FrameView& view) { return view.viewType() == WebCore::FrameView::Type::Remote; }
static bool isType(const WebCore::Widget& widget) { return widget.isRemoteFrameView(); }
SPECIALIZE_TYPE_TRAITS_END()
