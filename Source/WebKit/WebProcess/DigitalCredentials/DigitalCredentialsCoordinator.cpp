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
#include "DigitalCredentialsCoordinator.h"

#if HAVE(DIGITAL_CREDENTIALS_UI)
#include "DigitalCredentialsCoordinatorMessages.h"
#include "DigitalCredentialsRequestValidatorBridge.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/DigitalCredentialsProtocols.h>
#include <WebCore/DigitalCredentialsRequestData.h>
#include <WebCore/DigitalCredentialsResponseData.h>
#include <WebCore/ExceptionData.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(DigitalCredentialsCoordinator);

DigitalCredentialsCoordinator::DigitalCredentialsCoordinator(WebPage& page)
    : m_page(page)
    , m_pageIdentifier(page.identifier())
{
    WebProcess::singleton().addMessageReceiver(Messages::DigitalCredentialsCoordinator::messageReceiverName(), m_pageIdentifier, *this);
}

DigitalCredentialsCoordinator::~DigitalCredentialsCoordinator()
{
    WebProcess::singleton().removeMessageReceiver(Messages::DigitalCredentialsCoordinator::messageReceiverName(), m_pageIdentifier);
}

Ref<DigitalCredentialsCoordinator> DigitalCredentialsCoordinator::create(WebPage& webPage)
{
    return adoptRef(*new DigitalCredentialsCoordinator(webPage));
}

void DigitalCredentialsCoordinator::showDigitalCredentialsPicker(Vector<UnvalidatedDigitalCredentialRequest>&& rawRequests, const WebCore::DigitalCredentialsRequestData& request, WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler)
{
    ASSERT(m_rawRequests.isEmpty());
    m_rawRequests = WTFMove(rawRequests);

    if (RefPtr page = m_page.get()) {
        page->showDigitalCredentialsPicker(request, [this, completionHandler = WTFMove(completionHandler)](Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&& responseOrException) mutable {
            m_rawRequests.clear();
            completionHandler(WTFMove(responseOrException));
        });
    } else {
        m_rawRequests.clear();
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, "The page is not available."_s }));
    }
}

ExceptionOr<Vector<WebCore::ValidatedDigitalCredentialRequest>> DigitalCredentialsCoordinator::validateAndParseDigitalCredentialRequests(const SecurityOrigin& topOrigin, const Document& document, const Vector<UnvalidatedDigitalCredentialRequest>& unvalidatedRequests)
{
    auto results = DigitalCredentials::validateRequests(topOrigin, document, unvalidatedRequests);
    return WTFMove(results);
}

void DigitalCredentialsCoordinator::dismissDigitalCredentialsPicker(CompletionHandler<void(bool)>&& completionHandler)
{
    m_rawRequests.clear();
    if (RefPtr page = m_page.get())
        page->dismissDigitalCredentialsPicker(WTFMove(completionHandler));
    else
        completionHandler(false);
}

void DigitalCredentialsCoordinator::provideRawDigitalCredentialRequests(CompletionHandler<void(Vector<WebCore::UnvalidatedDigitalCredentialRequest>&&)>&& completionHandler)
{
    ASSERT(!m_rawRequests.isEmpty());
    completionHandler(WTFMove(m_rawRequests));
    m_rawRequests.clear();
}

} // namespace WebKit
#endif // HAVE(DIGITAL_CREDENTIALS_UI)
