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
#import "PhoneNumberPossessionProvider.h"

#if ENABLE(PHONE_NUMBER_POSSESSION)

#import "WebKit-Swift.h"
#import "WebPageProxy.h"
#import <WebCore/SecurityOriginData.h>
#import <WebKit/WKWebView.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PhoneNumberPossessionProvider);

class PhoneNumberPossessionProviderCocoa final : public PhoneNumberPossessionProvider {
    WTF_MAKE_TZONE_ALLOCATED(PhoneNumberPossessionProviderCocoa);
public:
    static Ref<PhoneNumberPossessionProviderCocoa> create(PhoneNumberPossessionController&& controller)
    {
        return adoptRef(*new PhoneNumberPossessionProviderCocoa(WTF::move(controller)));
    }

private:
    explicit PhoneNumberPossessionProviderCocoa(PhoneNumberPossessionController&& controller)
        : m_controller(WTF::move(controller))
    {
    }

    void canRequest(CompletionHandler<void(bool)>&& completionHandler) final
    {
        m_controller.canRequest(PhoneNumberPossessionCanRequestCompletionHandler::create([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](bool canRequest) mutable {
            completionHandler(canRequest);
        }).ptr());
    }

    void request(WebCore::PhoneNumberPossessionRequest&& request, const WebCore::SecurityOriginData& origin, WebPageProxy& page, RequestCompletionHandler&& completionHandler) final
    {
        RetainPtr webView = page.cocoaView();
        m_controller.request(RefCountedPhoneNumberPossessionRequest::create(WTF::move(request)).ptr(), origin.toString(), [webView window], PhoneNumberPossessionRequestCompletionHandler::create([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)](PhoneNumberPossessionResult result) mutable {
            completionHandler(WTF::move(result));
        }).ptr());
    }

    void cancel(CompletionHandler<void()>&& completionHandler) final
    {
        m_controller.cancel(PhoneNumberPossessionCancelCompletionHandler::create([protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] mutable {
            completionHandler();
        }).ptr());
    }

    PhoneNumberPossessionController m_controller;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(PhoneNumberPossessionProviderCocoa);

Ref<PhoneNumberPossessionProvider> PhoneNumberPossessionProvider::createPlatformProvider()
{
    // FIXME: The controller reports every request as unavailable until the platform's manager is added.
    return PhoneNumberPossessionProviderCocoa::create(PhoneNumberPossessionController::init());
}

Ref<PhoneNumberPossessionProvider> PhoneNumberPossessionProvider::createMock(const WebCore::MockPhoneNumberPossessionConfiguration& configuration)
{
    return PhoneNumberPossessionProviderCocoa::create(PhoneNumberPossessionController::init(configuration));
}

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
