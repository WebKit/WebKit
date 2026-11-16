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
#include "WebPhoneNumberPossessionCoordinator.h"

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include "MessageSenderInlines.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPhoneNumberPossessionCoordinatorProxyMessages.h"
#include <WebCore/LocalFrame.h>
#include <WebCore/MockPhoneNumberPossessionConfiguration.h>
#include <WebCore/PhoneNumberPossessionRequest.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPhoneNumberPossessionCoordinator);

WebPhoneNumberPossessionCoordinator::WebPhoneNumberPossessionCoordinator(WebPage& webPage)
    : m_webPage(webPage)
{
}

void WebPhoneNumberPossessionCoordinator::requestPhoneNumberPossession(const LocalFrame& frame, const PhoneNumberPossessionRequest& request, RequestCompletionHandler&& completionHandler)
{
    RefPtr webFrame = WebFrame::fromCoreFrame(frame);
    if (!webFrame) {
        completionHandler(makeUnexpected(ExceptionData { ExceptionCode::NotAllowedError, "The requesting frame is not available."_s }));
        return;
    }

    auto frameID = webFrame->frameID();
    m_pendingRequestFrameID = frameID;
    protect(m_webPage)->sendWithAsyncReply(Messages::WebPhoneNumberPossessionCoordinatorProxy::RequestPhoneNumberPossession(frameID, request), [weakThis = WeakPtr { *this }, frameID, completionHandler = WTF::move(completionHandler)](auto&& result) mutable {
        if (CheckedPtr checkedThis = weakThis; checkedThis && checkedThis->m_pendingRequestFrameID == frameID)
            checkedThis->m_pendingRequestFrameID = std::nullopt;
        completionHandler(WTF::move(result));
    });
}

void WebPhoneNumberPossessionCoordinator::isAvailable(const LocalFrame& frame, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr webFrame = WebFrame::fromCoreFrame(frame);
    if (!webFrame) {
        completionHandler(false);
        return;
    }

    protect(m_webPage)->sendWithAsyncReply(Messages::WebPhoneNumberPossessionCoordinatorProxy::IsAvailable(webFrame->frameID()), WTF::move(completionHandler));
}

void WebPhoneNumberPossessionCoordinator::cancel(CompletionHandler<void()>&& completionHandler)
{
    if (!m_pendingRequestFrameID) {
        completionHandler();
        return;
    }

    protect(m_webPage)->sendWithAsyncReply(Messages::WebPhoneNumberPossessionCoordinatorProxy::Cancel(*m_pendingRequestFrameID), WTF::move(completionHandler));
}

void WebPhoneNumberPossessionCoordinator::setMockConfigurationForTesting(const MockPhoneNumberPossessionConfiguration& configuration)
{
    protect(m_webPage)->send(Messages::WebPhoneNumberPossessionCoordinatorProxy::SetMockConfiguration(configuration));
}

} // namespace WebKit

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
