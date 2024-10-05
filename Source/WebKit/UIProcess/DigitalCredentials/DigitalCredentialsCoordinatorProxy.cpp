/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "DigitalCredentialsCoordinatorProxy.h"

#if ENABLE(WEB_AUTHN)

#include "DigitalCredentialsCoordinatorProxyMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/DigitalCredentialRequestOptions.h>
#include <WebCore/ExceptionData.h>
#include <wtf/CompletionHandler.h>

namespace WebKit {
using namespace WebCore;

DigitalCredentialsCoordinatorProxy::DigitalCredentialsCoordinatorProxy(WebPageProxy& page)
    : m_page(page)
{
    Ref pageRef = m_page.get();
    pageRef->protectedLegacyMainFrameProcess()->addMessageReceiver(Messages::DigitalCredentialsCoordinatorProxy::messageReceiverName(), pageRef->webPageIDInMainFrameProcess(), *this);
}

DigitalCredentialsCoordinatorProxy::~DigitalCredentialsCoordinatorProxy()
{
    Ref page = m_page.get();
    page->protectedLegacyMainFrameProcess()->removeMessageReceiver(Messages::DigitalCredentialsCoordinatorProxy::messageReceiverName(), page->webPageIDInMainFrameProcess());
}

std::optional<SharedPreferencesForWebProcess> DigitalCredentialsCoordinatorProxy::sharedPreferencesForWebProcess() const
{
    return protectedPage()->protectedLegacyMainFrameProcess()->sharedPreferencesForWebProcess();
}

Ref<WebPageProxy> DigitalCredentialsCoordinatorProxy::protectedPage() const
{
    return m_page.get();
}

void DigitalCredentialsCoordinatorProxy::requestDigitalCredential(FrameIdentifier frameId, FrameInfoData&& frameInfo, DigitalCredentialRequestOptions&& options, DigitalRequestCompletionHandler&& handler)
{
    // FIXME: Handle the request for a digital credential.
    // For now, we will simply call the handler with a dummy exception data.
    handler({ ExceptionCode::NotSupportedError, "Not implemented"_s });
}

void DigitalCredentialsCoordinatorProxy::cancel(CompletionHandler<void()>&& completionHandler)
{
    // FIXME: Handle cancellation properly.
    completionHandler();
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
