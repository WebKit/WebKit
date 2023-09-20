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

#include "config.h"
#include "RemoteScrollbarsController.h"

#if PLATFORM(MAC)

#include <WebCore/ScrollableArea.h>
#include <WebCore/ScrollingCoordinator.h>
#include <pal/spi/mac/NSScrollerImpSPI.h>

namespace WebKit {

RemoteScrollbarsController::RemoteScrollbarsController(WebCore::ScrollableArea& scrollableArea, WebCore::ScrollingCoordinator* coordinator)
    : ScrollbarsController(scrollableArea)
    , m_coordinator(ThreadSafeWeakPtr<WebCore::ScrollingCoordinator>(coordinator))
{
}

void RemoteScrollbarsController::mouseEnteredContentArea()
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setMouseIsOverContentArea(scrollableArea(), true);
}

void RemoteScrollbarsController::mouseExitedContentArea()
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setMouseIsOverContentArea(scrollableArea(), false);
}

void RemoteScrollbarsController::mouseMovedInContentArea()
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setMouseMovedInContentArea(scrollableArea());
}

void RemoteScrollbarsController::mouseEnteredScrollbar(WebCore::Scrollbar* scrollbar) const
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setMouseIsOverScrollbar(scrollbar, true);
}

void RemoteScrollbarsController::mouseExitedScrollbar(WebCore::Scrollbar* scrollbar) const
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setMouseIsOverScrollbar(scrollbar, false);
}

bool RemoteScrollbarsController::shouldScrollbarParticipateInHitTesting(WebCore::Scrollbar* scrollbar)
{
    // Non-overlay scrollbars should always participate in hit testing.    
    ASSERT(scrollbar->isOverlayScrollbar());

    // Overlay scrollbars should participate in hit testing whenever they are at all visible.
    return scrollbar->orientation() == WebCore::ScrollbarOrientation::Horizontal ? m_horizontalOverlayScrollbarIsVisible :  m_verticalOverlayScrollbarIsVisible;
}

void RemoteScrollbarsController::setScrollbarVisibilityState(WebCore::ScrollbarOrientation orientation, bool isVisible)
{
    if (orientation == WebCore::ScrollbarOrientation::Horizontal)
        m_horizontalOverlayScrollbarIsVisible = isVisible;
    else
        m_verticalOverlayScrollbarIsVisible = isVisible;
}

bool RemoteScrollbarsController::shouldDrawIntoScrollbarLayer(WebCore::Scrollbar& scrollbar) const
{
    // For UI-side compositing we only draw scrollbars in the web process
    // for custom scrollbars
    return scrollbar.isCustomScrollbar() || scrollbar.isMockScrollbar();
}

bool RemoteScrollbarsController::shouldRegisterScrollbars() const
{
    return scrollableArea().isListBox();
}

void RemoteScrollbarsController::setScrollbarMinimumThumbLength(WebCore::ScrollbarOrientation orientation, int minimumThumbLength)
{
    if (orientation == WebCore::ScrollbarOrientation::Horizontal)
        m_horizontalMinimumThumbLength = minimumThumbLength;
    else
        m_verticalMinimumThumbLength = minimumThumbLength;
}

int RemoteScrollbarsController::minimumThumbLength(WebCore::ScrollbarOrientation orientation)
{
    return orientation == WebCore::ScrollbarOrientation::Horizontal ? m_horizontalMinimumThumbLength : m_verticalMinimumThumbLength;
}

void RemoteScrollbarsController::updateScrollbarEnabledState(WebCore::Scrollbar& scrollbar)
{
    if (auto scrollingCoordinator = m_coordinator.get())
        scrollingCoordinator->setScrollbarEnabled(scrollbar);
}

}
#endif // PLATFORM(MAC)
