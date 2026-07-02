/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "CredentialRequestCoordinator.h"

#if ENABLE(WEB_AUTHN)

#include "AbortSignal.h"
#include "Chrome.h"
#include "CredentialRequestCoordinatorClient.h"
#include "DigitalCredential.h"
#include "DigitalCredentialsRequestData.h"
#include "DigitalCredentialsRequestDataBuilder.h"
#include "DigitalCredentialsResponseData.h"
#include "DocumentSecurityOrigin.h"
#include "ExceptionData.h"
#include "ExceptionOr.h"
#include "JSDOMConvertAny.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertJSON.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDigitalCredential.h"
#include "JSValueInWrappedObjectInlines.h"
#include "Page.h"
#include "SecurityOriginData.h"
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/StrongInlines.h>
#include <Logging.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CredentialRequestCoordinator);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CredentialRequestCoordinatorClient);

Ref<CredentialRequestCoordinator> CredentialRequestCoordinator::create(Ref<CredentialRequestCoordinatorClient>&& client, Page& page)
{
    return adoptRef(*new CredentialRequestCoordinator(WTF::move(client), page));
}

CredentialRequestCoordinator::CredentialRequestCoordinator(Ref<CredentialRequestCoordinatorClient>&& client, Page& page)
    : ActiveDOMObject(page.localTopDocument())
    , m_client(WTF::move(client))
    , m_page(page)
{
}

CredentialRequestCoordinator::InteractionStateGuard::InteractionStateGuard(CredentialRequestCoordinator& coordinator)
    : m_coordinator(coordinator)
{
    ASSERT(coordinator.interactionState() == InteractionState::Requesting);
}

CredentialRequestCoordinator::InteractionStateGuard::~InteractionStateGuard()
{
    if (!m_active)
        return;

    ASSERT(m_coordinator->interactionState() == InteractionState::Requesting
        || m_coordinator->interactionState() == InteractionState::Aborting);

    m_coordinator->setInteractionState(InteractionState::Idle);
}

CredentialRequestCoordinator::InteractionState CredentialRequestCoordinator::interactionState() const
{
    return m_interactionState;
}

bool CredentialRequestCoordinator::canTransitionTo(InteractionState newState) const
{
    switch (m_interactionState) {
    case InteractionState::Idle:
        return newState == InteractionState::Requesting;
    case InteractionState::Requesting:
        return newState == InteractionState::Aborting || newState == InteractionState::Idle;
    case InteractionState::Aborting:
        return newState == InteractionState::Idle;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void CredentialRequestCoordinator::setInteractionState(InteractionState newState)
{
    if (m_interactionState == newState)
        return;

    ASSERT(canTransitionTo(newState));
    m_interactionState = newState;
}

void CredentialRequestCoordinator::setCurrentPromise(CredentialPromise&& promise)
{
    ASSERT(m_interactionState == InteractionState::Requesting);
    ASSERT(!m_currentPromise);
    m_currentPromise = makeUnique<CredentialPromise>(WTF::move(promise));
}

CredentialPromise* CredentialRequestCoordinator::currentPromise()
{
    return m_currentPromise.get();
}

void CredentialRequestCoordinator::prepareCredentialRequests(const Document& document, CredentialPromise&& promise, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests, RefPtr<AbortSignal> signal)
{
    if (m_interactionState != InteractionState::Idle)
        return promise.reject(ExceptionCode::NotAllowedError, "A credential request is already in progress."_s);

    ASSERT(!m_currentPromise);
    setInteractionState(InteractionState::Requesting);
    setCurrentPromise(WTF::move(promise));

    if (!m_page)
        return rejectTheCredentialRequestWith(Exception { ExceptionCode::AbortError, "Page was destroyed."_s });

    auto validatedRequestsOrException = m_client->validateAndParseDigitalCredentialRequests(
        protect(document.topOrigin()),
        document,
        unvalidatedRequests);

    if (validatedRequestsOrException.hasException())
        return rejectTheCredentialRequestWith(validatedRequestsOrException.releaseException());

    auto validatedCredentialRequests = validatedRequestsOrException.releaseReturnValue();

    if (signal) {
        ASSERT(!signal->aborted());
        m_abortSignal = signal;
        m_abortAlgorithmIdentifier = signal->addAlgorithm([weakThis = WeakPtr { *this }, signal = protect(signal)](JSC::JSValue reason) {
            if (!weakThis)
                return;
            LOG(DigitalCredentials, "Credential request was aborted by AbortSignal");
            weakThis->abortTheCredentialRequest(ExceptionOr<JSC::JSValue> { WTF::move(reason) });
        });
    }

    if (signal && signal->aborted())
        return rejectTheCredentialRequestWith(Exception { ExceptionCode::AbortError, "Signal was already aborted."_s });

    initiateTheCredentialRequest(document, WTF::move(validatedCredentialRequests), WTF::move(unvalidatedRequests), signal);
}

void CredentialRequestCoordinator::initiateTheCredentialRequest(const Document& document, Vector<ValidatedDigitalCredentialRequest>&& validatedRequests, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests, RefPtr<AbortSignal> signal)
{
    auto requestDataAndRawRequests = DigitalCredentialsRequestDataBuilder::build(validatedRequests, document, WTF::move(unvalidatedRequests));
    if (requestDataAndRawRequests.hasException())
        return rejectTheCredentialRequestWith(requestDataAndRawRequests.releaseException());

    observeContext(protect(document.scriptExecutionContext()).get());

    auto [requestData, rawRequests] = requestDataAndRawRequests.releaseReturnValue();

    m_client->showDigitalCredentialsChooser(
        WTF::move(rawRequests),
        requestData,
        [weakThis = WeakPtr { *this }, signal](Expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->processCredentialChooserResponse(WTF::move(responseOrException), signal);
        });
}

void CredentialRequestCoordinator::processCredentialChooserResponse(Expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException, RefPtr<AbortSignal> signal)
{
    if (signal && signal->aborted()) {
        LOG(DigitalCredentials, "Credential chooser response received after AbortSignal aborted");
        abortTheCredentialRequest(ExceptionOr<JSC::JSValue> { signal->reason().getValue() });
        return;
    }

    InteractionStateGuard guard(*this);

    if (!m_currentPromise) {
        LOG(DigitalCredentials, "No current promise in coordinator.");
        ASSERT_NOT_REACHED();
        return;
    }

    guard.deactivate();

    if (!responseOrException)
        return settleTheCredentialRequest(responseOrException.error().toException());

    auto& responseData = responseOrException.value();

    if (responseData.responseDataJSON.isEmpty())
        return settleTheCredentialRequest(Exception { ExceptionCode::NotAllowedError, "The user cancelled the credential request."_s });

    auto parsedObject = parseDigitalCredentialsResponseData(responseData.responseDataJSON);

    if (parsedObject.hasException())
        return settleTheCredentialRequest(parsedObject.releaseException());

    if (!parsedObject.returnValue())
        return settleTheCredentialRequest(Exception { ExceptionCode::TypeError, "No parsed object."_s });

    auto returnValue = parsedObject.releaseReturnValue();
    Ref credential = DigitalCredential::create({ returnValue->vm(), returnValue }, responseData.protocol);

    settleTheCredentialRequest(credential.ptr());
}

ExceptionOr<JSC::JSObject*> CredentialRequestCoordinator::parseDigitalCredentialsResponseData(const String& responseDataJSON) const
{
    RefPtr page = m_page.get();
    if (!page)
        return Exception { ExceptionCode::AbortError, "Page was destroyed."_s };

    RefPtr document = page->localTopDocument();
    if (!document)
        return Exception { ExceptionCode::AbortError, "No Document."_s };

    auto* globalObject = document->globalObject();
    if (!globalObject)
        return Exception { ExceptionCode::AbortError, "No JavaScript global object available."_s };

    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSC::JSLockHolder lock(globalObject);
    auto parsedJSON = JSC::JSONParse(globalObject, responseDataJSON);

    if (!parsedJSON)
        return Exception { ExceptionCode::SyntaxError, "Failed to parse response JSON data."_s };

    if (scope.exception()) [[unlikely]] {
        LOG(DigitalCredentials, "Failed to parse response JSON data");
        bool cleared = scope.tryClearException();
        // We're on the main thread so we can't get a termination exception.
        ASSERT_UNUSED(cleared, cleared);
        return Exception { ExceptionCode::SyntaxError, "Failed to parse response JSON data."_s };
    }

    if (!parsedJSON.isObject())
        return Exception { ExceptionCode::TypeError, "Parsed JSON data is not an object."_s };

    return parsedJSON.getObject();
}

// https://w3c-fedid.github.io/digital-credentials/#dfn-reject-the-credential-request-with
void CredentialRequestCoordinator::rejectTheCredentialRequestWith(Exception&& exception)
{
    ASSERT(m_interactionState != InteractionState::Idle);
    ASSERT(m_currentPromise);
    clearAbortAlgorithm();
    m_currentPromise->reject(WTF::move(exception));
    m_currentPromise.reset();
    setInteractionState(InteractionState::Idle);
}

void CredentialRequestCoordinator::settleTheCredentialRequest(ExceptionOr<RefPtr<BasicCredential>>&& result)
{
    clearAbortAlgorithm();

    auto promise = WTF::move(m_currentPromise);
    m_currentPromise.reset();

    ASSERT(m_interactionState == InteractionState::Requesting || m_interactionState == InteractionState::Aborting);

    m_client->dismissDigitalCredentialsChooser([weakThis = WeakPtr { *this }, promise = WTF::move(promise), result = WTF::move(result)](bool success) mutable {
        if (!success)
            LOG(DigitalCredentials, "Failed to dismiss the credential chooser.");

        if (auto* rawThis = weakThis.get())
            rawThis->setInteractionState(InteractionState::Idle);

        if (!promise)
            return;

        if (result.hasException())
            promise->reject(result.releaseException());
        else
            promise->resolve(result.releaseReturnValue().get());
    });
}

void CredentialRequestCoordinator::clearAbortAlgorithm()
{
    if (!m_abortAlgorithmIdentifier)
        return;

    RefPtr signal = m_abortSignal;
    if (signal)
        signal->removeAlgorithm(*m_abortAlgorithmIdentifier);

    m_abortAlgorithmIdentifier.reset();
    m_abortSignal = nullptr;
}

void CredentialRequestCoordinator::abortTheCredentialRequest(ExceptionOr<JSC::JSValue>&& reason)
{
    clearAbortAlgorithm();
    if (m_interactionState == InteractionState::Idle) {
        ASSERT(!m_currentPromise);
        return;
    }

    if (m_interactionState == InteractionState::Aborting) {
        ASSERT(!m_currentPromise);
        return;
    }

    if (m_interactionState != InteractionState::Requesting) {
        LOG(DigitalCredentials, "Cannot abort the credential request when it is not presenting.");
        return;
    }

    setInteractionState(InteractionState::Aborting);

    auto promise = WTF::move(m_currentPromise);
    m_currentPromise.reset();

    std::optional<Exception> abortException;
    std::optional<JSC::Strong<JSC::Unknown>> protectedReason;

    if (reason.hasException())
        abortException = reason.releaseException();
    else {
        auto jsReason = reason.releaseReturnValue();
        if (RefPtr page = m_page.get()) {
            if (RefPtr document = page->localTopDocument()) {
                if (auto* globalObject = document->globalObject()) {
                    JSC::VM& vm = globalObject->vm();
                    JSC::JSLockHolder lock(globalObject);
                    protectedReason.emplace(vm, WTF::move(jsReason));
                }
            }
        }
    }

    m_client->dismissDigitalCredentialsChooser(
        [weakThis = WeakPtr { *this }, promise = WTF::move(promise), abortException = WTF::move(abortException), protectedReason = WTF::move(protectedReason)](bool success) mutable {
            if (!success)
                LOG(DigitalCredentials, "Failed to dismiss the credential chooser.");

            if (auto* rawThis = weakThis.get())
                rawThis->setInteractionState(InteractionState::Idle);

            if (!promise)
                return;

            if (abortException)
                return promise->reject(WTF::move(*abortException));

            if (protectedReason)
                return promise->rejectType<IDLAny>(protectedReason->get());

            promise->reject(ExceptionCode::AbortError);
        });
}

void CredentialRequestCoordinator::contextDestroyed()
{
    LOG(DigitalCredentials, "The context we were observing got destroyed");
    abortTheCredentialRequest(Exception { ExceptionCode::AbortError, "script execution context was destroyed."_s });
};

CredentialRequestCoordinator::~CredentialRequestCoordinator()
{
    clearAbortAlgorithm();

    if (m_currentPromise) {
        m_currentPromise->reject(ExceptionCode::AbortError);
        m_currentPromise.reset();
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
