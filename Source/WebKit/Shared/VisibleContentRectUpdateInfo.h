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

#pragma once

#if ENABLE(UI_SIDE_COMPOSITING)

#include "TransactionID.h"
#include <WebCore/FloatRect.h>
#include <WebCore/LengthBox.h>
#include <WebCore/VelocityData.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WTF {
class TextStream;
}

namespace WebKit {

enum class ViewStabilityFlag : uint8_t {
    ScrollViewInteracting               = 1 << 0, // Dragging, zooming, interrupting deceleration
    ScrollViewAnimatedScrollOrZoom      = 1 << 1, // Decelerating, scrolling to top, animated zoom
    ScrollViewRubberBanding             = 1 << 2,
    ChangingObscuredInsetsInteractively = 1 << 3,
    UnstableForTesting                  = 1 << 4
};

class VisibleContentRectUpdateInfo {
public:
    VisibleContentRectUpdateInfo() = default;

    VisibleContentRectUpdateInfo(const WebCore::FloatRect& exposedContentRect, const WebCore::FloatRect& unobscuredContentRect, const WebCore::FloatBoxExtent& contentInsets, const WebCore::FloatRect& unobscuredRectInScrollViewCoordinates, const WebCore::FloatRect& unobscuredContentRectRespectingInputViewBounds, const WebCore::FloatRect& layoutViewportRect, const WebCore::FloatBoxExtent& obscuredInsets, const WebCore::FloatBoxExtent& unobscuredSafeAreaInsets, double scale, OptionSet<ViewStabilityFlag> viewStability, bool isFirstUpdateForNewViewSize, bool allowShrinkToFit, bool enclosedInScrollableAncestorView, const WebCore::VelocityData& scrollVelocity, TransactionID lastLayerTreeTransactionId)
        : m_exposedContentRect(exposedContentRect)
        , m_unobscuredContentRect(unobscuredContentRect)
        , m_contentInsets(contentInsets)
        , m_unobscuredContentRectRespectingInputViewBounds(unobscuredContentRectRespectingInputViewBounds)
        , m_unobscuredRectInScrollViewCoordinates(unobscuredRectInScrollViewCoordinates)
        , m_layoutViewportRect(layoutViewportRect)
        , m_obscuredInsets(obscuredInsets)
        , m_unobscuredSafeAreaInsets(unobscuredSafeAreaInsets)
        , m_scrollVelocity(scrollVelocity)
        , m_lastLayerTreeTransactionID(lastLayerTreeTransactionId)
        , m_scale(scale)
        , m_viewStability(viewStability)
        , m_isFirstUpdateForNewViewSize(isFirstUpdateForNewViewSize)
        , m_allowShrinkToFit(allowShrinkToFit)
        , m_enclosedInScrollableAncestorView(enclosedInScrollableAncestorView)
    {
    }

    const WebCore::FloatRect& exposedContentRect() const { return m_exposedContentRect; }
    const WebCore::FloatRect& unobscuredContentRect() const { return m_unobscuredContentRect; }
    const WebCore::FloatBoxExtent& contentInsets() const { return m_contentInsets; }
    const WebCore::FloatRect& unobscuredRectInScrollViewCoordinates() const { return m_unobscuredRectInScrollViewCoordinates; }
    const WebCore::FloatRect& unobscuredContentRectRespectingInputViewBounds() const { return m_unobscuredContentRectRespectingInputViewBounds; }
    const WebCore::FloatRect& layoutViewportRect() const { return m_layoutViewportRect; }
    const WebCore::VelocityData& scrollVelocity() const { return m_scrollVelocity; }
    const WebCore::FloatBoxExtent& obscuredInsets() const { return m_obscuredInsets; }
    const WebCore::FloatBoxExtent& unobscuredSafeAreaInsets() const { return m_unobscuredSafeAreaInsets; }

    double scale() const { return m_scale; }
    bool inStableState() const { return m_viewStability.isEmpty(); }
    OptionSet<ViewStabilityFlag> viewStability() const { return m_viewStability; }
    bool isFirstUpdateForNewViewSize() const { return m_isFirstUpdateForNewViewSize; }
    bool allowShrinkToFit() const { return m_allowShrinkToFit; }
    bool enclosedInScrollableAncestorView() const { return m_enclosedInScrollableAncestorView; }
    TransactionID lastLayerTreeTransactionID() const { return m_lastLayerTreeTransactionID; }

    MonotonicTime timestamp() const { return m_scrollVelocity.lastUpdateTime; }

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, VisibleContentRectUpdateInfo&);

    String dump() const;

private:
    WebCore::FloatRect m_exposedContentRect;
    WebCore::FloatRect m_unobscuredContentRect;
    WebCore::FloatBoxExtent m_contentInsets;
    WebCore::FloatRect m_unobscuredContentRectRespectingInputViewBounds;
    WebCore::FloatRect m_unobscuredRectInScrollViewCoordinates;
    WebCore::FloatRect m_layoutViewportRect;
    WebCore::FloatBoxExtent m_obscuredInsets;
    WebCore::FloatBoxExtent m_unobscuredSafeAreaInsets;
    WebCore::VelocityData m_scrollVelocity;
    TransactionID m_lastLayerTreeTransactionID;
    double m_scale { -1 };
    OptionSet<ViewStabilityFlag> m_viewStability;
    bool m_isFirstUpdateForNewViewSize { false };
    bool m_allowShrinkToFit { false };
    bool m_enclosedInScrollableAncestorView { false };
};

inline bool operator==(const VisibleContentRectUpdateInfo& a, const VisibleContentRectUpdateInfo& b)
{
    // Note: the comparison doesn't include timestamp and velocity since we care about equality based on the other data.
    return a.scale() == b.scale()
        && a.exposedContentRect() == b.exposedContentRect()
        && a.unobscuredContentRect() == b.unobscuredContentRect()
        && a.contentInsets() == b.contentInsets()
        && a.unobscuredContentRectRespectingInputViewBounds() == b.unobscuredContentRectRespectingInputViewBounds()
        && a.layoutViewportRect() == b.layoutViewportRect()
        && a.obscuredInsets() == b.obscuredInsets()
        && a.unobscuredSafeAreaInsets() == b.unobscuredSafeAreaInsets()
        && a.scrollVelocity().equalIgnoringTimestamp(b.scrollVelocity())
        && a.viewStability() == b.viewStability()
        && a.isFirstUpdateForNewViewSize() == b.isFirstUpdateForNewViewSize()
        && a.allowShrinkToFit() == b.allowShrinkToFit()
        && a.enclosedInScrollableAncestorView() == b.enclosedInScrollableAncestorView();
}

WTF::TextStream& operator<<(WTF::TextStream&, ViewStabilityFlag);
WTF::TextStream& operator<<(WTF::TextStream&, const VisibleContentRectUpdateInfo&);

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::ViewStabilityFlag> {
    using values = EnumValues<
        WebKit::ViewStabilityFlag,
        WebKit::ViewStabilityFlag::ScrollViewInteracting,
        WebKit::ViewStabilityFlag::ScrollViewAnimatedScrollOrZoom,
        WebKit::ViewStabilityFlag::ScrollViewRubberBanding,
        WebKit::ViewStabilityFlag::ChangingObscuredInsetsInteractively,
        WebKit::ViewStabilityFlag::UnstableForTesting
    >;
};

} // namespace WTF

#endif // ENABLE(UI_SIDE_COMPOSITING)
