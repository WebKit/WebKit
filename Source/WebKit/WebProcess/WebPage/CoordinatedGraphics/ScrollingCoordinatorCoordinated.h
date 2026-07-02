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

#pragma once

#if ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
#include <WebCore/ThreadedScrollingCoordinator.h>

#if HAVE(DISPLAY_LINK)
#include "DisplayLinkObserverID.h"
#endif

namespace WebKit {

class WebPage;

class ScrollingCoordinatorCoordinated final : public WebCore::ThreadedScrollingCoordinator {
public:
    static Ref<ScrollingCoordinatorCoordinated> create(WebPage* page)
    {
        return adoptRef(*new ScrollingCoordinatorCoordinated(page));
    }

private:
    explicit ScrollingCoordinatorCoordinated(WebPage*);
    ~ScrollingCoordinatorCoordinated() final;

    void didCompletePlatformRenderingUpdate() final;
    void pageDestroyed() final;
#if HAVE(DISPLAY_LINK)
    void hasNodeWithAnimatedScrollChanged(bool) final;

    uint64_t m_destinationID { 0 };
    DisplayLinkObserverID m_observerID;
#endif
};

} // namespace WebKit

#endif // ENABLE(ASYNC_SCROLLING) && USE(COORDINATED_GRAPHICS)
