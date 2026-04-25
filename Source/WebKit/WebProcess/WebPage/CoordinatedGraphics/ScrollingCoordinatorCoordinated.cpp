/*
 * Copyright (C) 2018, 2024, 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingCoordinatorCoordinated.h"

#if ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ScrollingTreeCoordinated.h>

namespace WebKit {

ScrollingCoordinatorCoordinated::ScrollingCoordinatorCoordinated(WebPage* page)
    : WebCore::ThreadedScrollingCoordinator(page->corePage())
#if HAVE(DISPLAY_LINK)
    , m_destinationID(page->identifier().toUInt64())
    , m_observerID(DisplayLinkObserverID::generate())
#endif
{
    setScrollingTree(WebCore::ScrollingTreeCoordinated::create(*this));
}

ScrollingCoordinatorCoordinated::~ScrollingCoordinatorCoordinated()
{
    ASSERT(!scrollingTree());
}

void ScrollingCoordinatorCoordinated::pageDestroyed()
{
    WebCore::ThreadedScrollingCoordinator::pageDestroyed();
}

#if HAVE(DISPLAY_LINK)
void ScrollingCoordinatorCoordinated::hasNodeWithAnimatedScrollChanged(bool haveAnimatedScrollingNodes)
{
    ASSERT(scrollingTree());
    RefPtr connection = WebProcess::singleton().parentProcessConnection();
    if (!connection)
        return;
    connection->send(Messages::WebPageProxy::SetHasActiveAnimatedScrollsForAsyncScrolling(m_observerID, haveAnimatedScrollingNodes), m_destinationID);
}
#endif

void ScrollingCoordinatorCoordinated::didCompletePlatformRenderingUpdate()
{
    downcast<WebCore::ScrollingTreeCoordinated>(scrollingTree())->didCompletePlatformRenderingUpdate();
}

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
