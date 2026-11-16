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
#import <WebCore/SecurityOriginData.h>
#import <WebKit/WKWebView.h>

namespace WebKit {

static PhoneNumberPossessionController createController(const std::optional<WebCore::MockPhoneNumberPossessionConfiguration>& mockConfiguration)
{
    if (mockConfiguration)
        return PhoneNumberPossessionController::init(*mockConfiguration);

    // FIXME: The controller reports every request as unavailable until the platform's manager is added.
    return PhoneNumberPossessionController::init();
}

void WebPhoneNumberPossessionCoordinatorProxy::platformIsAvailable(CompletionHandler<void(bool)>&& completionHandler)
{
    createController(m_mockConfiguration).canRequest(PhoneNumberPossessionCanRequestCompletionHandler::create(WTF::move(completionHandler)).ptr());
}

void WebPhoneNumberPossessionCoordinatorProxy::platformRequestPhoneNumberPossession(WebCore::PhoneNumberPossessionRequest&& request, const WebCore::SecurityOriginData& origin, WebPageProxy& page, CompletionHandler<void(PhoneNumberPossessionResult&&)>&& completionHandler)
{
    ASSERT(m_pendingRequest);
    auto controller = createController(m_mockConfiguration);
    m_pendingRequest->cancel = [controller](CompletionHandler<void()>&& completionHandler) mutable {
        controller.cancel(PhoneNumberPossessionCancelCompletionHandler::create(WTF::move(completionHandler)).ptr());
    };

    RetainPtr webView = page.cocoaView();
    controller.request(RefCountedPhoneNumberPossessionRequest::create(WTF::move(request)).ptr(), origin.toString(), [webView window], PhoneNumberPossessionRequestCompletionHandler::create([completionHandler = WTF::move(completionHandler)](PhoneNumberPossessionResult result) mutable {
        completionHandler(WTF::move(result));
    }).ptr());
}

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
