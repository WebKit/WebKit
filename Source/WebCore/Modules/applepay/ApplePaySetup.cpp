/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
#include "ApplePaySetupWebCore.h"

#if ENABLE(APPLE_PAY)

#include "Document.h"
#include "JSApplePaySetupFeature.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "PaymentCoordinator.h"
#include "PaymentCoordinatorClient.h"
#include "PaymentSession.h"
#include "Settings.h"
#include <wtf/CompletionHandler.h>

namespace WebCore {

static bool shouldDiscloseFeatures(Document& document)
{
    auto* page = document.page();
    if (!page || page->usesEphemeralSession())
        return false;

    return document.settings().applePayCapabilityDisclosureAllowed();
}

void ApplePaySetup::getSetupFeatures(Document& document, SetupFeaturesPromise&& promise)
{
    auto canCall = PaymentSession::canCreateSession(document);
    if (canCall.hasException()) {
        promise.reject(canCall.releaseException());
        return;
    }

    auto page = document.page();
    if (!page) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    if (m_setupFeaturesPromise) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    // Resolve with an empty sequence of features if Apple Pay capability disclosure is not allowed.
    if (!shouldDiscloseFeatures(document)) {
        promise.resolve({ });
        return;
    }

    m_setupFeaturesPromise = WTFMove(promise);

    page->paymentCoordinator().getSetupFeatures(m_configuration, document.url(), [this, pendingActivity = makePendingActivity(*this)](Vector<Ref<ApplePaySetupFeature>>&& setupFeatures) {
        if (m_setupFeaturesPromise)
            std::exchange(m_setupFeaturesPromise, std::nullopt)->resolve(WTFMove(setupFeatures));
    });
}

void ApplePaySetup::begin(Document& document, Vector<RefPtr<ApplePaySetupFeature>>&& features, BeginPromise&& promise)
{
    auto canCall = PaymentSession::canCreateSession(document);
    if (canCall.hasException()) {
        promise.reject(canCall.releaseException());
        return;
    }

    if (!UserGestureIndicator::processingUserGesture()) {
        promise.reject(Exception { ExceptionCode::InvalidAccessError, "Must call ApplePaySetup.begin from a user gesture handler."_s });
        return;
    }

    RefPtr page = document.page();
    if (!page) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    if (m_beginPromise) {
        promise.reject(Exception { ExceptionCode::InvalidStateError });
        return;
    }

    m_beginPromise = WTFMove(promise);
    m_pendingActivity = makePendingActivity(*this);

    page->paymentCoordinator().beginApplePaySetup(m_configuration, page->mainFrameURL(), WTFMove(features), [this](bool result) {
        if (m_beginPromise)
            std::exchange(m_beginPromise, std::nullopt)->resolve(result);
    });
}

Ref<ApplePaySetup> ApplePaySetup::create(ScriptExecutionContext& context, ApplePaySetupConfiguration&& configuration)
{
    auto setup = adoptRef(*new ApplePaySetup(context, WTFMove(configuration)));
    setup->suspendIfNeeded();
    return setup;
}

ApplePaySetup::ApplePaySetup(ScriptExecutionContext& context, ApplePaySetupConfiguration&& configuration)
    : ActiveDOMObject(&context)
    , m_configuration(WTFMove(configuration))
{
}

void ApplePaySetup::stop()
{
    if (m_setupFeaturesPromise)
        std::exchange(m_setupFeaturesPromise, std::nullopt)->reject(Exception { ExceptionCode::AbortError });

    if (m_beginPromise)
        std::exchange(m_beginPromise, std::nullopt)->reject(Exception { ExceptionCode::AbortError });

    if (auto page = downcast<Document>(*scriptExecutionContext()).page())
        page->paymentCoordinator().endApplePaySetup();

    m_pendingActivity = nullptr;
}

void ApplePaySetup::suspend(ReasonForSuspension)
{
    stop();
}

} // namespace WebCore

#endif // ENABLE(APPLE_PAY)
