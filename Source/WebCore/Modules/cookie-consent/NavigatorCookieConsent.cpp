/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "NavigatorCookieConsent.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "CookieConsentDecisionResult.h"
#include "ExceptionCode.h"
#include "JSDOMPromiseDeferred.h"
#include "Navigator.h"
#include "Page.h"
#include "RequestCookieConsentOptions.h"

namespace WebCore {

void NavigatorCookieConsent::requestCookieConsent(Navigator& navigator, RequestCookieConsentOptions&& options, Ref<DeferredPromise>&& promise)
{
    from(navigator).requestCookieConsent(WTFMove(options), WTFMove(promise));
}

void NavigatorCookieConsent::requestCookieConsent(RequestCookieConsentOptions&& options, Ref<DeferredPromise>&& promise)
{
    // FIXME: Support the 'More info' option.
    UNUSED_PARAM(options);

    RefPtr frame = m_navigator.frame();
    if (!frame || !frame->isMainFrame() || !frame->page()) {
        promise->reject(NotAllowedError);
        return;
    }

    frame->page()->chrome().client().requestCookieConsent([promise = WTFMove(promise)] (CookieConsentDecisionResult result) {
        switch (result) {
        case CookieConsentDecisionResult::NotSupported:
            promise->reject(NotSupportedError);
            break;
        case CookieConsentDecisionResult::Consent:
            promise->resolve<IDLBoolean>(true);
            break;
        case CookieConsentDecisionResult::Dissent:
            promise->resolve<IDLBoolean>(false);
            break;
        }
    });
}

NavigatorCookieConsent& NavigatorCookieConsent::from(Navigator& navigator)
{
    if (auto supplement = static_cast<NavigatorCookieConsent*>(Supplement<Navigator>::from(&navigator, supplementName())))
        return *supplement;

    auto newSupplement = makeUnique<NavigatorCookieConsent>(navigator);
    auto supplement = newSupplement.get();
    provideTo(&navigator, supplementName(), WTFMove(newSupplement));
    return *supplement;
}

} // namespace WebCore
