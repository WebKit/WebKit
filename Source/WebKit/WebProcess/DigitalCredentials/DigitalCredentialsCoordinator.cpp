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
#include "DigitalCredentialsCoordinator.h"

#if ENABLE(WEB_AUTHN)
#include "DigitalCredentialsCoordinatorProxyMessages.h"
#include "FrameInfoData.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/DigitalCredentialRequestOptions.h>
#include <WebCore/LocalFrame.h>
#include <wtf/Logger.h>

namespace WebKit {
using namespace WebCore;

DigitalCredentialsCoordinator::DigitalCredentialsCoordinator(WebPage& page)
    : m_page(page)
{
}

RefPtr<WebPage> DigitalCredentialsCoordinator::protectedPage() const
{
    return m_page.get();
}

void DigitalCredentialsCoordinator::requestDigitalCredential(const LocalFrame& frame, const DigitalCredentialRequestOptions& options, DigitalCredentialRequestCompletionHandler&& handler)
{
    RefPtr webFrame = WebFrame::fromCoreFrame(frame);
    RefPtr page = m_page.get();
    if (!webFrame || !page) {
        LOG_ERROR("Unable to get frame or page");
        handler(ExceptionData { ExceptionCode::InvalidStateError, "Unable to get frame or page"_s });
        return;
    }
    page->sendWithAsyncReply(Messages::DigitalCredentialsCoordinatorProxy::RequestDigitalCredential(webFrame->frameID(), webFrame->info(), options), WTFMove(handler));
}

void DigitalCredentialsCoordinator::cancel(CompletionHandler<void()>&& handler)
{
    RefPtr page = m_page.get();
    if (!page) {
        handler();
        return;
    }

    page->sendWithAsyncReply(Messages::DigitalCredentialsCoordinatorProxy::Cancel(), WTFMove(handler));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
