/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "ScrollerMac.h"
#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>

OBJC_CLASS NSScrollerImpPair;
OBJC_CLASS WKScrollerImpPairDelegate;

namespace WebCore {
class PlatformMouseEvent;
class PlatformWheelEvent;
class ScrollingTreeScrollingNode;
}

namespace WebKit {

class ScrollerPairMac {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ScrollerPairMac(WebCore::ScrollingTreeScrollingNode&);

    ~ScrollerPairMac();

    ScrollerMac& verticalScroller() { return m_verticalScroller; }
    ScrollerMac& horizontalScroller() { return m_horizontalScroller; }

    void handleWheelEvent(const WebCore::PlatformWheelEvent&);
    void handleMouseEvent(const WebCore::PlatformMouseEvent&);

    void updateValues();

    WebCore::FloatSize visibleSize() const;
    WebCore::IntPoint lastKnownMousePosition() const { return m_lastKnownMousePosition; }
    bool useDarkAppearance() const;

    struct Values {
        float value;
        float proportion;
    };
    Values valuesForOrientation(ScrollerMac::Orientation);

    NSScrollerImpPair *scrollerImpPair() { return m_scrollerImpPair.get(); }

private:
    WebCore::ScrollingTreeScrollingNode& m_scrollingNode;

    ScrollerMac m_verticalScroller;
    ScrollerMac m_horizontalScroller;

    WebCore::FloatSize m_contentSize;
    WebCore::FloatRect m_visibleContentRect;

    WebCore::IntPoint m_lastKnownMousePosition;
    Optional<WebCore::FloatPoint> m_lastScrollPosition;

    RetainPtr<NSScrollerImpPair> m_scrollerImpPair;
    RetainPtr<WKScrollerImpPairDelegate> m_scrollerImpPairDelegate;
};

}

#endif
