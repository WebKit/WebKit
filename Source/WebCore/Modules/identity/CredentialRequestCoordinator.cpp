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
#include "DigitalCredentialsResponseData.h"
#include "DocumentSecurityOrigin.h"
#include "ExceptionData.h"
#include "ExceptionOr.h"
#include "JSDigitalCredential.h"
#include "Page.h"
#include "SecurityOriginData.h"
#include <JavaScriptCore/JSObject.h>
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

CredentialRequestCoordinator::PickerStateGuard::PickerStateGuard(CredentialRequestCoordinator& coordinator)
    : m_coordinator(coordinator)
{
    ASSERT(coordinator.currentState() == PickerState::Presenting);
}

CredentialRequestCoordinator::PickerStateGuard::~PickerStateGuard()
{
    if (!m_active)
        return;

    ASSERT(m_coordinator->currentState() == PickerState::Presenting
        || m_coordinator->currentState() == PickerState::Aborting);

    m_coordinator->setState(PickerState::Idle);
}

CredentialRequestCoordinator::PickerState CredentialRequestCoordinator::currentState() const
{
    return m_state;
}

bool CredentialRequestCoordinator::canTransitionTo(PickerState newState) const
{
    switch (m_state) {
    case PickerState::Idle:
        return newState == PickerState::Presenting;
    case PickerState::Presenting:
        return newState == PickerState::Aborting || newState == PickerState::Idle;
    case PickerState::Aborting:
        return newState == PickerState::Idle;
    }
    ASSERT_NOT_REACHED();
    return false;
}

void CredentialRequestCoordinator::setState(PickerState newState)
{
    if (m_state == newState)
        return;

    ASSERT(canTransitionTo(newState));
    m_state = newState;
}

void CredentialRequestCoordinator::setCurrentPromise(CredentialPromise&& promise)
{
    ASSERT(!m_currentPromise.has_value());
    m_currentPromise = WTF::move(promise);
}

CredentialPromise* CredentialRequestCoordinator::currentPromise()
{
    return m_currentPromise ? &m_currentPromise.value() : nullptr;
}

void CredentialRequestCoordinator::prepareCredentialRequest(const Document& document, CredentialPromise&& promise, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests, RefPtr<AbortSignal> signal)
{
    if (m_state != PickerState::Idle)
        return promise.reject(ExceptionCode::InvalidStateError, "A credential picker operation is already in progress."_s);

    if (!m_page)
        return promise.reject(ExceptionCode::AbortError, "Page was destroyed."_s);

    auto validatedRequestsOrException = m_client->validateAndParseDigitalCredentialRequests(
        protect(document.topOrigin()),
        document,
        unvalidatedRequests);

    if (validatedRequestsOrException.hasException())
        return promise.reject(validatedRequestsOrException.releaseException());

    if (signal) {
        // CredentialsContainer handled rejecting pre-aborted signal
        ASSERT(!signal->aborted());

        signal->addAlgorithm([weakThis = WeakPtr { *this }, signal = RefPtr { signal }](JSC::JSValue reason) {
            if (!weakThis)
                return;
            LOG(DigitalCredentials, "Credential picker was aborted by AbortSignal");
            weakThis->abortPicker(WTF::move(reason));
        });
    }

    setState(PickerState::Presenting);
    setCurrentPromise(WTF::move(promise));
    observeContext(document.protectedScriptExecutionContext().get());

    auto validatedCredentialRequests = validatedRequestsOrException.releaseReturnValue();
    DigitalCredentialsRequestData requestData {
        WTF::move(validatedCredentialRequests),
        protect(document.topOrigin())->data(),
        protect(document.securityOrigin())->data(),
    };

    m_client->showDigitalCredentialsPicker(
        WTF::move(unvalidatedRequests),
        requestData,
        [weakThis = WeakPtr { *this }, signal](Expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->handleDigitalCredentialsPickerResult(WTF::move(responseOrException), signal);
        });
}

void CredentialRequestCoordinator::handleDigitalCredentialsPickerResult(Expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException, RefPtr<AbortSignal> signal)
{
    // Abort flow already owns dismiss + settle-after-teardown.
    if (signal && signal->aborted())
        return;

    PickerStateGuard guard(*this);

    if (!m_currentPromise) {
        LOG(DigitalCredentials, "No current promise in coordinator.");
        ASSERT_NOT_REACHED();
        return;
    }

    guard.deactivate();

    if (!responseOrException)
        return dismissPickerAndSettle(responseOrException.error().toException());

    auto& responseData = responseOrException.value();

    if (responseData.responseDataJSON.isEmpty())
        return dismissPickerAndSettle(Exception { ExceptionCode::AbortError, "User aborted the operation."_s });

    auto parsedObject = parseDigitalCredentialsResponseData(responseData.responseDataJSON);

    if (parsedObject.hasException())
        return dismissPickerAndSettle(parsedObject.releaseException());

    if (!parsedObject.returnValue())
        return dismissPickerAndSettle(Exception { ExceptionCode::TypeError, "No parsed object."_s });

    auto returnValue = parsedObject.releaseReturnValue();
    Ref credential = DigitalCredential::create({ returnValue->vm(), returnValue }, responseData.protocol);

    dismissPickerAndSettle(credential.ptr());
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

void CredentialRequestCoordinator::dismissPickerAndSettle(ExceptionOr<RefPtr<BasicCredential>>&& result)
{
    auto promise = WTF::move(m_currentPromise);
    m_currentPromise.reset();

    ASSERT(m_state == PickerState::Presenting || m_state == PickerState::Aborting);

    m_client->dismissDigitalCredentialsPicker([weakThis = WeakPtr { *this }, promise = WTF::move(promise), result = WTF::move(result)](bool success) mutable {
        if (!success)
            LOG(DigitalCredentials, "Failed to dismiss the credentials picker.");

        if (RefPtr protectedThis = weakThis.get())
            protectedThis->setState(PickerState::Idle);

        if (!promise)
            return;

        if (result.hasException())
            promise->reject(result.releaseException());
        else
            promise->resolve(result.releaseReturnValue().get());
    });
}

void CredentialRequestCoordinator::abortPicker(ExceptionOr<JSC::JSValue>&& reason)
{
    if (m_state == PickerState::Idle) {
        // No UI teardown needed. Settle (defensively) and return.
        if (m_currentPromise) {
            if (reason.hasException())
                m_currentPromise->reject(reason.releaseException());
            else
                m_currentPromise->rejectType<IDLAny>(reason.releaseReturnValue());
            m_currentPromise.reset();
        }
        return;
    }

    if (m_state == PickerState::Aborting) {
        ASSERT(!m_currentPromise);
        return;
    }

    if (m_state != PickerState::Presenting) {
        LOG(DigitalCredentials, "Cannot abort the credentials picker when it is not presenting.");
        return;
    }

    setState(PickerState::Aborting);

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

    m_client->dismissDigitalCredentialsPicker(
        [weakThis = WeakPtr { *this }, promise = WTF::move(promise), abortException = WTF::move(abortException), protectedReason = WTF::move(protectedReason)](bool success) mutable {
            if (!success)
                LOG(DigitalCredentials, "Failed to dismiss the credentials picker.");

            if (RefPtr protectedThis = weakThis.get())
                protectedThis->setState(PickerState::Idle);

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
    abortPicker(Exception { ExceptionCode::AbortError, "script execution context was destroyed."_s });
};

CredentialRequestCoordinator::~CredentialRequestCoordinator()
{
    if (m_currentPromise) {
        m_currentPromise->reject(ExceptionCode::AbortError);
        m_currentPromise.reset();
    }
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
