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

#include "WebPage.h"
#include <WebCore/IntRect.h>
#include <wtf/TZoneMalloc.h>
#if PLATFORM(IOS_FAMILY)
#include "FindIndicatorOverlayClientIOS.h"
#endif

namespace WebCore {
class LocalFrame;
};

namespace WebKit {

class FindIndicator {
    WTF_MAKE_TZONE_ALLOCATED(FindIndicator);
public:
    explicit FindIndicator(WebPage* webPage)
        : m_webPage(webPage) { };
    virtual ~FindIndicator() = default;
    virtual bool update(WebCore::LocalFrame* selectedFrame, bool isShowingOverlay, bool shouldAnimate = true);
    virtual void hide();
    virtual void didFindString(WebCore::LocalFrame* selectedFrame);
    constexpr virtual bool shouldHideOnScroll() const { return true; }
    bool isShowing() const { return m_isShowingFindIndicator; }
    WebCore::IntRect rect() const { return m_findIndicatorRect; }
protected:
    // Whether the UI process is showing the find indicator. Note that this can be true even if
    // the find indicator isn't showing, but it will never be false when it is showing.
    bool m_isShowingFindIndicator { false };
    WebCore::IntRect m_findIndicatorRect;
    const WeakPtr<WebPage> m_webPage;
};

#if PLATFORM(IOS_FAMILY)
class FindIndicatorIOS final : public FindIndicator {
    WTF_MAKE_TZONE_ALLOCATED(FindIndicatorIOS);
public:
    explicit FindIndicatorIOS(WebPage* webPage)
        : FindIndicator(webPage) { };
    bool update(WebCore::LocalFrame* selectedFrame, bool isShowingOverlay, bool shouldAnimate = true) override;
    void hide() override;
    void didFindString(WebCore::LocalFrame* selectedFrame) override;
    constexpr bool shouldHideOnScroll() const override { return false; }
private:
    RefPtr<WebCore::PageOverlay> m_findIndicatorOverlay;
    std::unique_ptr<FindIndicatorOverlayClientIOS> m_findIndicatorOverlayClient;
};
#endif

}
