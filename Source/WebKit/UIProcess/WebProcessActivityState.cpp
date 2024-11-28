/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "WebProcessActivityState.h"

#include "APIPageConfiguration.h"
#include "RemotePageProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"

namespace WebKit {

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)

static Seconds webProcessSuspensionDelay(const WebPageProxy* page)
{
    if (!page)
        return WebProcessPool::defaultWebProcessSuspensionDelay();

    RefPtr processPool = page->protectedLegacyMainFrameProcess()->processPoolIfExists();
    if (!processPool)
        return WebProcessPool::defaultWebProcessSuspensionDelay();

    return processPool->webProcessSuspensionDelay();
}

#endif

WebProcessActivityState::WebProcessActivityState(WebPageProxy& page)
    : m_page(page)
#if PLATFORM(MAC)
    , m_wasRecentlyVisibleActivity(ProcessThrottlerTimedActivity::create(webProcessSuspensionDelay(&page)))
#endif
{
}

WebProcessActivityState::WebProcessActivityState(RemotePageProxy& page)
    : m_page(page)
#if PLATFORM(MAC)
    , m_wasRecentlyVisibleActivity(ProcessThrottlerTimedActivity::create(webProcessSuspensionDelay(page.protectedPage().get())))
#endif
{
}

void WebProcessActivityState::takeVisibleActivity()
{
    m_isVisibleActivity = process().throttler().foregroundActivity("View is visible"_s);
#if PLATFORM(MAC)
    m_wasRecentlyVisibleActivity->setActivity(nullptr);
#endif
}

void WebProcessActivityState::takeAudibleActivity()
{
    m_isAudibleActivity = process().throttler().foregroundActivity("View is playing audio"_s);
}

void WebProcessActivityState::takeCapturingActivity()
{
    m_isCapturingActivity = process().throttler().foregroundActivity("View is capturing media"_s);
}

void WebProcessActivityState::takeMutedCaptureAssertion()
{
    m_isMutedCaptureAssertion = ProcessAssertion::create(protectedProcess(), "WebKit Muted Media Capture"_s, ProcessAssertionType::Background);

    auto page = std::visit([](auto&& weakPageRef) -> std::variant<WeakPtr<WebPageProxy>, WeakPtr<RemotePageProxy>> {
        return weakPageRef.get();
    }, m_page);

    m_isMutedCaptureAssertion->setInvalidationHandler([weakPage = page] {
        auto invalidateCaptureAssertion = [](auto&& weakPage) {
            if (auto* page = weakPage.get()) {
                RELEASE_LOG(ProcessSuspension, "Muted capture assertion is invalidated");
                page->processActivityState().m_isMutedCaptureAssertion = nullptr;
            }
        };
        std::visit(invalidateCaptureAssertion, weakPage);
    });
}

void WebProcessActivityState::reset()
{
    m_isVisibleActivity = nullptr;
#if PLATFORM(MAC)
    m_wasRecentlyVisibleActivity->setActivity(nullptr);
#endif
    m_isAudibleActivity = nullptr;
    m_isCapturingActivity = nullptr;
    m_isMutedCaptureAssertion = nullptr;
#if PLATFORM(IOS_FAMILY)
    m_openingAppLinkActivity = nullptr;
#endif
}

void WebProcessActivityState::dropVisibleActivity()
{
#if PLATFORM(MAC)
    if (WTF::numberOfProcessorCores() > 4)
        m_wasRecentlyVisibleActivity->setActivity(process().throttler().backgroundActivity("View was recently visible"_s));
    else
        m_wasRecentlyVisibleActivity->setActivity(process().throttler().foregroundActivity("View was recently visible"_s));
#endif
    m_isVisibleActivity = nullptr;
}

void WebProcessActivityState::dropAudibleActivity()
{
    m_isAudibleActivity = nullptr;
}

void WebProcessActivityState::dropCapturingActivity()
{
    m_isCapturingActivity = nullptr;
}

void WebProcessActivityState::dropMutedCaptureAssertion()
{
    m_isMutedCaptureAssertion = nullptr;
}

bool WebProcessActivityState::hasValidVisibleActivity() const
{
    return m_isVisibleActivity && m_isVisibleActivity->isValid();
}

bool WebProcessActivityState::hasValidAudibleActivity() const
{
    return m_isAudibleActivity && m_isAudibleActivity->isValid();
}

bool WebProcessActivityState::hasValidCapturingActivity() const
{
    return m_isCapturingActivity && m_isCapturingActivity->isValid();
}

bool WebProcessActivityState::hasValidMutedCaptureAssertion() const
{
    return m_isMutedCaptureAssertion;
}

#if PLATFORM(IOS_FAMILY)
void WebProcessActivityState::takeOpeningAppLinkActivity()
{
    m_openingAppLinkActivity = process().throttler().backgroundActivity("Opening AppLink"_s);
}

void WebProcessActivityState::dropOpeningAppLinkActivity()
{
    m_openingAppLinkActivity = nullptr;
}

bool WebProcessActivityState::hasValidOpeningAppLinkActivity() const
{
    return m_openingAppLinkActivity && m_openingAppLinkActivity->isValid();
}
#endif

#if ENABLE(WEB_PROCESS_SUSPENSION_DELAY)

void WebProcessActivityState::updateWebProcessSuspensionDelay()
{
    Seconds timeout = std::visit(WTF::makeVisitor([&](const WeakRef<WebPageProxy>& page) {
        return webProcessSuspensionDelay(page.ptr());
    }, [&] (const WeakRef<RemotePageProxy>& page) {
        return webProcessSuspensionDelay(page->protectedPage().get());
    }), m_page);
    m_wasRecentlyVisibleActivity->setTimeout(timeout);
}

#endif

WebProcessProxy& WebProcessActivityState::process() const
{
    return std::visit([](auto& page) -> WebProcessProxy& {
        using T = std::decay_t<decltype(page)>;
        if constexpr (std::is_same_v<T, WeakRef<WebPageProxy>>)
            return page->legacyMainFrameProcess();
        else if constexpr (std::is_same_v<T, WeakRef<RemotePageProxy>>)
            return page->process();
        else
            static_assert(std::is_same_v<T, std::false_type>, "Unhandled page type in WebProcessActivityState::process");
    }, m_page);
}

Ref<WebProcessProxy> WebProcessActivityState::protectedProcess() const
{
    return process();
}

} // namespace WebKit
