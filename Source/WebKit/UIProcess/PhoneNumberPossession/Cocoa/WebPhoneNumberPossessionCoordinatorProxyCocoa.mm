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

#import "config.h"
#import "WebPhoneNumberPossessionCoordinatorProxy.h"

#if ENABLE(PHONE_NUMBER_POSSESSION)

#import "WebKit-Swift.h"
#import "WebPageProxy.h"
#import "WebPreferences.h"
#import "WebPreferencesKeys.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <WebKit/WKWebView.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

#import "WebKitSwiftSoftLink.h"

namespace WebKit {

id<WKPhoneNumberPossessionPlatformManaging> phoneNumberPossessionPlatformManager()
{
    return adoptNS([getWKPhoneNumberPossessionPlatformManagerClassSingleton() new]).autorelease();
}

NODELETE static bool& cachedPhoneNumberPossessionPlatformEnabled()
{
    static bool enabled;
    return enabled;
}

NODELETE bool isPhoneNumberPossessionEnabledByPlatform()
{
    return cachedPhoneNumberPossessionPlatformEnabled();
}

bool canPlatformEnablePhoneNumberPossession()
{
    return [getWKPhoneNumberPossessionPlatformManagerClassSingleton() respondsToSelector:@selector(isEnabledWithCompletionHandler:)];
}

void fetchPhoneNumberPossessionPlatformEnabledIfNeeded()
{
    ASSERT(RunLoop::isMain());
    static bool didFetch;
    if (std::exchange(didFetch, true))
        return;

    [getWKPhoneNumberPossessionPlatformManagerClassSingleton() isEnabledWithCompletionHandler:makeBlockPtr([](BOOL enabled) {
        if (!enabled)
            return;

        RunLoop::mainSingleton().dispatch([] {
            cachedPhoneNumberPossessionPlatformEnabled() = true;
            for (Ref processPool : WebProcessPool::allProcessPools()) {
                for (Ref process : processPool->processes()) {
                    for (Ref page : process->pages()) {
                        protect(page->preferences())->removeOverriddenDefaultValueForKey(WebPreferencesKey::phoneNumberPossessionEnabledKey());
                        page->preferencesDidChange();
                    }
                }
            }
        });
    }).get()];
}

static PhoneNumberPossessionController createController(const std::optional<WebCore::MockPhoneNumberPossessionConfiguration>& mockConfiguration)
{
    if (mockConfiguration)
        return PhoneNumberPossessionController::init(*mockConfiguration);
    return PhoneNumberPossessionController::init();
}

void WebPhoneNumberPossessionCoordinatorProxy::platformIsAvailable(CompletionHandler<void(bool)>&& completionHandler)
{
    createController(m_mockConfiguration).canRequest(PhoneNumberPossessionCanRequestCompletionHandler::create(WTF::move(completionHandler)).ptr());
}

void WebPhoneNumberPossessionCoordinatorProxy::platformRequestPhoneNumberPossession(WebCore::PhoneNumberPossessionRequest&& request, WebPageProxy& page, CompletionHandler<void(PhoneNumberPossessionResult&&)>&& completionHandler)
{
    ASSERT(m_pendingRequest);
    auto controller = createController(m_mockConfiguration);
    m_pendingRequest->cancel = [controller](CompletionHandler<void()>&& completionHandler) mutable {
        controller.cancel(PhoneNumberPossessionCancelCompletionHandler::create(WTF::move(completionHandler)).ptr());
    };

    RetainPtr webView = page.cocoaView();
    controller.request(RefCountedPhoneNumberPossessionRequest::create(WTF::move(request)).ptr(), [webView window], PhoneNumberPossessionRequestCompletionHandler::create([completionHandler = WTF::move(completionHandler)](PhoneNumberPossessionResult result) mutable {
        completionHandler(WTF::move(result));
    }).ptr());
}

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
