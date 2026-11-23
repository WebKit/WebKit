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
#include "CommonVM.h"
#include "CredentialRequestCoordinatorClient.h"
#include "DigitalCredential.h"
#include "DigitalCredentialsRequestData.h"
#include "DigitalCredentialsRequestDataBuilder.h"
#include "DigitalCredentialsResponseData.h"
#include "DigitalCredentialsSession.h"
#include "Document.h"
#include "DocumentSecurityOrigin.h"
#include "EventLoop.h"
#include "ExceptionData.h"
#include "ExceptionOr.h"
#include "FrameDestructionObserverInlines.h"
#include "JSDOMConvertAny.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertJSON.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMPromiseDeferred.h"
#include "JSDigitalCredential.h"
#include "JSValueInWrappedObjectInlines.h"
#include "LocalFrame.h"
#include "Page.h"
#include "SecurityOriginData.h"
#include "TaskSource.h"
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
    : m_client(WTF::move(client))
    , m_page(page)
{
}

CredentialRequestCoordinator::~CredentialRequestCoordinator() = default;

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

void CredentialRequestCoordinator::sessionDidFinish(const DigitalCredentialsSession& session)
{
    if (m_activeSession.get() != &session)
        return;

    m_activeSession = nullptr;
    setInteractionState(InteractionState::Idle);
}

void CredentialRequestCoordinator::dismissChooser()
{
    m_client->dismissDigitalCredentialsChooser([](bool success) {
        if (!success)
            LOG(DigitalCredentials, "Failed to dismiss the credential chooser.");
    });
}

void CredentialRequestCoordinator::prepareCredentialRequests(const Document& document, CredentialPromise&& promise, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests, RefPtr<AbortSignal> signal)
{
    RefPtr context = document.scriptExecutionContext();
    if (!context)
        return promise.reject(Exception { ExceptionCode::AbortError, "Document has no script execution context."_s });

    if (m_interactionState != InteractionState::Idle) {
        // The spec queues this on the *requesting* document's global, which is not the
        // in-flight request's, so it cannot go through the active session.
        CheckedRef eventLoop = context->eventLoop();
        eventLoop->queueTask(TaskSource::DOMManipulation, [promise = makeUnique<CredentialPromise>(WTF::move(promise))]() mutable {
            promise->reject(ExceptionCode::NotAllowedError, "A credential request is already in progress."_s);
        });
        return;
    }

    ASSERT(!m_activeSession);
    setInteractionState(InteractionState::Requesting);
    Ref session = DigitalCredentialsSession::create(*context, *this, WTF::move(promise));
    m_activeSession = session.ptr();

    if (!m_page)
        return rejectTheCredentialRequestWith(Exception { ExceptionCode::AbortError, "Page was destroyed."_s });

    auto validatedRequestsOrException = m_client->validateAndParseDigitalCredentialRequests(
        protect(document.topOrigin()),
        document,
        unvalidatedRequests);

    if (validatedRequestsOrException.hasException())
        return rejectTheCredentialRequestWith(validatedRequestsOrException.releaseException());

    auto validatedCredentialRequests = validatedRequestsOrException.releaseReturnValue();

    bool hasOpenID4VPRequest = unvalidatedRequests.containsIf([](auto& request) {
        return std::holds_alternative<OpenID4VPSignedRequest>(request)
            || std::holds_alternative<OpenID4VPMultisignedRequest>(request)
            || std::holds_alternative<OpenID4VPUnsignedRequest>(request);
    });
    bool hasValidatedRequest = validatedCredentialRequests.containsIf([](auto& request) {
        return request.has_value();
    });
    if (!hasValidatedRequest && !hasOpenID4VPRequest)
        return rejectTheCredentialRequestWith(Exception { ExceptionCode::TypeError, "No valid credential requests remain after validation"_s });

    if (signal) {
        if (signal->aborted())
            return rejectTheCredentialRequestWith(signal->reason().getValue());

        auto identifier = signal->addAlgorithm([weakThis = WeakPtr { *this }](JSC::JSValue reason) {
            RefPtr protectedThis = weakThis;
            if (!protectedThis)
                return;
            LOG(DigitalCredentials, "Credential request was aborted by AbortSignal");
            protectedThis->abortTheCredentialRequest(reason);
        });
        session->setAbortAlgorithm(RefPtr { signal }, identifier);
    }

    initiateTheCredentialRequest(document, WTF::move(validatedCredentialRequests), WTF::move(unvalidatedRequests), signal);
}

void CredentialRequestCoordinator::initiateTheCredentialRequest(const Document& document, Vector<std::optional<ValidatedDigitalCredentialRequest>>&& validatedRequests, Vector<UnvalidatedDigitalCredentialRequest>&& unvalidatedRequests, RefPtr<AbortSignal> signal)
{
    DigitalCredentialsSecurityOriginData origins {
        .topOrigin = document.topOrigin().data(),
        .documentOrigin = document.securityOrigin().data(),
    };
    auto requestDataAndRawRequests = DigitalCredentialsRequestDataBuilder::build(WTF::move(validatedRequests), origins, WTF::move(unvalidatedRequests));
    if (requestDataAndRawRequests.hasException())
        return rejectTheCredentialRequestWith(requestDataAndRawRequests.releaseException());

    auto [requestData, rawRequests] = requestDataAndRawRequests.releaseReturnValue();

    std::optional<FrameIdentifier> requestingFrameID;
    if (RefPtr frame = document.frame())
        requestingFrameID = frame->frameID();

    m_client->showDigitalCredentialsChooser(
        requestingFrameID,
        WTF::move(rawRequests),
        requestData,
        [weakThis = WeakPtr { *this }, session = m_activeSession, signal](std::expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException) {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->processCredentialChooserResponse(WTF::move(responseOrException), *session, signal);
        });
}

void CredentialRequestCoordinator::processCredentialChooserResponse(std::expected<DigitalCredentialsResponseData, ExceptionData>&& responseOrException, DigitalCredentialsSession& session, RefPtr<AbortSignal> signal)
{
    if (m_activeSession.get() != &session) {
        LOG(DigitalCredentials, "Ignoring credential chooser response for a request that has already settled.");
        return;
    }

    if (signal && signal->aborted()) {
        LOG(DigitalCredentials, "Credential chooser response received after AbortSignal aborted");
        abortTheCredentialRequest(signal->reason().getValue());
        return;
    }

    if (m_interactionState != InteractionState::Requesting) {
        LOG(DigitalCredentials, "Ignoring credential chooser response received while not in the Requesting state.");
        return;
    }

    if (!responseOrException)
        return rejectTheCredentialRequestWith(responseOrException.error().toException());

    auto& responseData = responseOrException.value();

    if (responseData.responseDataJSON.isEmpty())
        return rejectTheCredentialRequestWith(Exception { ExceptionCode::NotAllowedError, "The user cancelled the credential request."_s });

    protect(session)->queueSettlement([weakThis = WeakPtr { *this }, responseDataJSON = responseData.responseDataJSON, protocol = responseData.protocol](auto& promise) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis) {
            promise.reject(Exception { ExceptionCode::AbortError, "Page was destroyed."_s });
            return;
        }

        auto parsedObject = protectedThis->parseDigitalCredentialsResponseData(responseDataJSON);
        if (parsedObject.hasException())
            promise.reject(parsedObject.releaseException());
        else if (!parsedObject.returnValue())
            promise.reject(Exception { ExceptionCode::TypeError, "Parsed JSON data is not an object."_s });
        else {
            auto returnValue = parsedObject.releaseReturnValue();
            Ref credential = DigitalCredential::create({ returnValue->vm(), returnValue }, protocol);
            promise.resolve(credential.ptr());
        }
    });
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

    RefPtr session = m_activeSession;
    if (!session)
        return;

    session->queueSettlement([exception = WTF::move(exception)](auto& promise) mutable {
        promise.reject(WTF::move(exception));
    });
}

void CredentialRequestCoordinator::abortTheCredentialRequest(JSC::JSValue reason)
{
    RefPtr session = m_activeSession;
    if (!session || !session->hasPromise())
        return;

    if (m_interactionState != InteractionState::Requesting)
        return;

    setInteractionState(InteractionState::Aborting);
    dismissChooser();
    rejectTheCredentialRequestWith(reason);
}

void CredentialRequestCoordinator::rejectTheCredentialRequestWith(JSC::JSValue reason)
{
    ASSERT(m_interactionState != InteractionState::Idle);

    RefPtr session = m_activeSession;
    if (!session)
        return;

    Ref vm = commonVM();
    JSC::JSLockHolder lock(vm);
    session->queueSettlement([reason = JSC::Strong<JSC::Unknown> { vm, reason }](auto& promise) {
        promise.template rejectType<IDLAny>(reason.get());
    });
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
