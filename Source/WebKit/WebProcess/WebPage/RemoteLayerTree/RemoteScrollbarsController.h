/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/NSScrollerImpDetails.h>
#include <WebCore/ScrollbarsController.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeWeakPtr.h>

namespace WebCore {
class ScrollingCoordinator;
}

namespace WebKit {

class RemoteScrollbarsController final : public WebCore::ScrollbarsController {
public:
    RemoteScrollbarsController(WebCore::ScrollableArea&, WebCore::ScrollingCoordinator*);
    ~RemoteScrollbarsController() = default;
    void mouseEnteredContentArea() final;
    void mouseExitedContentArea()  final;
    void mouseMovedInContentArea() final;
    void mouseEnteredScrollbar(WebCore::Scrollbar*) const final;
    void mouseExitedScrollbar(WebCore::Scrollbar*) const final;
    bool shouldScrollbarParticipateInHitTesting(WebCore::Scrollbar*) final;

    void setScrollbarMinimumThumbLength(WebCore::ScrollbarOrientation, int) final;
    void setScrollbarVisibilityState(WebCore::ScrollbarOrientation, bool) final;
    bool shouldDrawIntoScrollbarLayer(WebCore::Scrollbar&) const final;
    bool shouldRegisterScrollbars() const final { return scrollableArea().isListBox(); }
    int minimumThumbLength(WebCore::ScrollbarOrientation) final;
    void updateScrollbarEnabledState(WebCore::Scrollbar&) final;

private:
    bool m_horizontalOverlayScrollbarIsVisible { false };
    bool m_verticalOverlayScrollbarIsVisible { false };

    int m_horizontalMinimumThumbLength { 0 };
    int m_verticalMinimumThumbLength { 0 };
    ThreadSafeWeakPtr<WebCore::ScrollingCoordinator> m_coordinator;
};

} // namespace WebKit

#endif // PLATFORM(MAC)
