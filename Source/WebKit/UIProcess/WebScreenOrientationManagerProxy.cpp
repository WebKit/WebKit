/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebScreenOrientationManagerProxy.h"

#include "APIUIClient.h"
#include "WebPageProxy.h"
#include "WebScreenOrientationManagerMessages.h"
#include "WebScreenOrientationManagerProxyMessages.h"
#include <WebCore/Exception.h>
#include <WebCore/ScreenOrientationProvider.h>

namespace WebKit {

WebScreenOrientationManagerProxy::WebScreenOrientationManagerProxy(WebPageProxy& page)
    : m_page(page)
    , m_provider(WebCore::ScreenOrientationProvider::create())
{
    m_page.process().addMessageReceiver(Messages::WebScreenOrientationManagerProxy::messageReceiverName(), m_page.webPageID(), *this);
    platformInitialize();
}

WebScreenOrientationManagerProxy::~WebScreenOrientationManagerProxy()
{
    m_page.process().removeMessageReceiver(Messages::WebScreenOrientationManagerProxy::messageReceiverName(), m_page.webPageID());
    m_provider->removeObserver(*this);
    platformDestroy();
}

void WebScreenOrientationManagerProxy::currentOrientation(CompletionHandler<void(WebCore::ScreenOrientationType)>&& completionHandler)
{
    completionHandler(m_provider->currentOrientation());
}

void WebScreenOrientationManagerProxy::lock(WebCore::ScreenOrientationLockType lockType, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    if (!m_page.uiClient().lockScreenOrientation(lockType)) {
        completionHandler(WebCore::Exception { WebCore::NotSupportedError, "Screen orientation locking is not supported"_s });
        return;
    }
    completionHandler(std::nullopt);
}

void WebScreenOrientationManagerProxy::unlock()
{
    m_page.uiClient().unlockScreenOrientation();
}

void WebScreenOrientationManagerProxy::screenOrientationDidChange(WebCore::ScreenOrientationType orientation)
{
    m_page.send(Messages::WebScreenOrientationManager::OrientationDidChange(orientation));
}

void WebScreenOrientationManagerProxy::setShouldSendChangeNotification(bool shouldSend)
{
    if (shouldSend)
        m_provider->addObserver(*this);
    else
        m_provider->removeObserver(*this);
}

#if !PLATFORM(IOS_FAMILY)
void WebScreenOrientationManagerProxy::platformInitialize()
{
}

void WebScreenOrientationManagerProxy::platformDestroy()
{
}
#endif

} // namespace WebKit
