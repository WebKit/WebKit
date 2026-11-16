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

#include "config.h"
#include "WebPhoneNumberPossessionCoordinatorProxy.h"

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include "SharedPreferencesForWebProcess.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebPhoneNumberPossessionCoordinatorProxyMessages.h"
#include "WebProcessProxy.h"
#include <WebCore/Exception.h>
#include <WebCore/PhoneNumberPossessionRequest.h>
#include <WebCore/SecurityOrigin.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPhoneNumberPossessionCoordinatorProxy);

static ExceptionData exceptionForError(PhoneNumberPossessionError error)
{
    switch (error) {
    case PhoneNumberPossessionError::Unavailable:
        return { ExceptionCode::NotSupportedError, "Phone number possession is not available."_s };
    case PhoneNumberPossessionError::Cancelled:
        return { ExceptionCode::AbortError, "The request was cancelled."_s };
    case PhoneNumberPossessionError::UserCancelled:
    case PhoneNumberPossessionError::Failed:
        return { ExceptionCode::NotAllowedError, "The request was not allowed."_s };
    }
    ASSERT_NOT_REACHED();
    return { ExceptionCode::NotAllowedError, "The request was not allowed."_s };
}

Ref<WebPhoneNumberPossessionCoordinatorProxy> WebPhoneNumberPossessionCoordinatorProxy::create(WebPageProxy& webPageProxy)
{
    return adoptRef(*new WebPhoneNumberPossessionCoordinatorProxy(webPageProxy));
}

WebPhoneNumberPossessionCoordinatorProxy::WebPhoneNumberPossessionCoordinatorProxy(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_process(webPageProxy.legacyMainFrameProcess())
    , m_webPageID(webPageProxy.webPageIDInMainFrameProcess())
{
    // FIXME: Also receive messages from cross-origin iframes in other processes, as RemotePageWebAuthenticatorCoordinatorProxy does.
    m_process->addMessageReceiver(Messages::WebPhoneNumberPossessionCoordinatorProxy::messageReceiverName(), m_webPageID, *this);
}

WebPhoneNumberPossessionCoordinatorProxy::~WebPhoneNumberPossessionCoordinatorProxy()
{
    cancelPendingRequest();
    m_process->removeMessageReceiver(Messages::WebPhoneNumberPossessionCoordinatorProxy::messageReceiverName(), m_webPageID);
}

std::optional<SharedPreferencesForWebProcess> WebPhoneNumberPossessionCoordinatorProxy::sharedPreferencesForWebProcess(IPC::Connection& connection) const
{
    return WebProcessProxy::fromConnection(connection)->sharedPreferencesForWebProcess();
}

std::expected<Ref<WebFrameProxy>, ExceptionData> WebPhoneNumberPossessionCoordinatorProxy::frameForMessage(IPC::Connection& connection, FrameIdentifier frameID) const
{
    // The frame can be gone, or be in another process, because of a navigation. Deny rather than treat it as a bad message.
    RefPtr webPageProxy = m_webPageProxy;
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!webPageProxy || !frame || frame->page() != webPageProxy.get() || &frame->process() != WebProcessProxy::fromConnection(connection).ptr())
        return makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "The requesting frame is not available."_s });

    auto origin = frame->documentSecurityOriginData();
    if (origin.isOpaque() || !origin.securityOrigin()->isPotentiallyTrustworthy())
        return makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "The document's origin cannot request phone number possession."_s });

    return frame.releaseNonNull();
}

void WebPhoneNumberPossessionCoordinatorProxy::requestPhoneNumberPossession(IPC::Connection& connection, FrameIdentifier frameID, PhoneNumberPossessionRequest&& request, RequestCompletionHandler&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION_BASE(!validatePhoneNumberPossessionRequest(request), connection, completionHandler(makeUnexpected(ExceptionData { ExceptionCode::TypeError, "The request is invalid."_s })));

    auto frameOrError = frameForMessage(connection, frameID);
    if (!frameOrError) {
        completionHandler(makeUnexpected(WTF::move(frameOrError.error())));
        return;
    }
    Ref frame = WTF::move(*frameOrError);

    RefPtr page = frame->page();
    if (!page->isViewVisible() || !page->isViewFocused()) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "The document must be focused and visible."_s }));
        return;
    }

    if (!frame->hasTransientActivation() || frame->lastActivationTimestamp() <= m_lastUsedActivationTimestamp) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "Calling get() needs to be triggered by an activation triggering user event."_s }));
        return;
    }

    // FIXME: Support related origins.
    request.relyingPartyIdentifier = request.relyingPartyIdentifier.convertToASCIILowercase();
    Ref origin = frame->documentSecurityOriginData().securityOrigin();
    if (URL::hostIsIPAddress(origin->host()) || !origin->isMatchingRegistrableDomainSuffix(request.relyingPartyIdentifier)) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::SecurityError, "The origin of the document is not authorized for the provided RP ID."_s }));
        return;
    }

    if (m_pendingRequest) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "A phone number possession request is already in progress."_s }));
        return;
    }

    m_lastUsedActivationTimestamp = frame->lastActivationTimestamp();

    auto identifier = ++m_lastRequestIdentifier;
    m_pendingRequest = PendingRequest { identifier, frameID, WTF::move(completionHandler), { } };
    platformRequestPhoneNumberPossession(WTF::move(request), origin->data(), *page, [weakThis = WeakPtr { *this }, identifier](PhoneNumberPossessionResult&& result) mutable {
        if (RefPtr protectedThis = weakThis)
            protectedThis->didCompleteRequest(identifier, WTF::move(result));
    });
}

void WebPhoneNumberPossessionCoordinatorProxy::didCompleteRequest(uint64_t identifier, PhoneNumberPossessionResult&& result)
{
    if (!m_pendingRequest || m_pendingRequest->identifier != identifier)
        return;

    auto completionHandler = WTF::move(m_pendingRequest->completionHandler);
    m_pendingRequest = std::nullopt;

    if (!result) {
        completionHandler(makeUnexpected(exceptionForError(result.error())));
        return;
    }
    completionHandler(WTF::move(*result));
}

void WebPhoneNumberPossessionCoordinatorProxy::isAvailable(IPC::Connection& connection, FrameIdentifier frameID, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!frameForMessage(connection, frameID)) {
        completionHandler(false);
        return;
    }

    platformIsAvailable(WTF::move(completionHandler));
}

void WebPhoneNumberPossessionCoordinatorProxy::cancel(IPC::Connection& connection, FrameIdentifier frameID, CompletionHandler<void()>&& completionHandler)
{
    if (!frameForMessage(connection, frameID)) {
        completionHandler();
        return;
    }

    cancelRequestForFrame(frameID, WTF::move(completionHandler));
}

void WebPhoneNumberPossessionCoordinatorProxy::cancelRequestForFrame(FrameIdentifier frameID, CompletionHandler<void()>&& completionHandler)
{
    if (!m_pendingRequest || m_pendingRequest->frameID != frameID) {
        completionHandler();
        return;
    }

    cancelPendingRequest(WTF::move(completionHandler));
}

void WebPhoneNumberPossessionCoordinatorProxy::cancelPendingRequest(CompletionHandler<void()>&& completionHandler)
{
    auto pendingRequest = std::exchange(m_pendingRequest, std::nullopt);
    if (!pendingRequest) {
        completionHandler();
        return;
    }

    pendingRequest->cancel([requestCompletionHandler = WTF::move(pendingRequest->completionHandler), completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler();
        requestCompletionHandler(makeUnexpected(exceptionForError(PhoneNumberPossessionError::Cancelled)));
    });
}

void WebPhoneNumberPossessionCoordinatorProxy::setMockConfiguration(MockPhoneNumberPossessionConfiguration&& configuration)
{
    m_mockConfiguration = WTF::move(configuration);
}

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
