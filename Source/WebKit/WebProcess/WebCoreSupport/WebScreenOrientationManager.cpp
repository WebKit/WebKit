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
#include "WebScreenOrientationManager.h"

#include "MessageSenderInlines.h"
#include "WebPage.h"
#include "WebProcess.h"
#include "WebScreenOrientationManagerMessages.h"
#include "WebScreenOrientationManagerProxyMessages.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebScreenOrientationManager);

WebScreenOrientationManager::WebScreenOrientationManager(WebPage& page)
    : m_page(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::WebScreenOrientationManager::messageReceiverName(), m_page->identifier(), *this);
}

WebScreenOrientationManager::~WebScreenOrientationManager()
{
    WebProcess::singleton().removeMessageReceiver(Messages::WebScreenOrientationManager::messageReceiverName(), m_page->identifier());
}

Ref<WebPage> WebScreenOrientationManager::protectedPage() const
{
    return m_page.get();
}

WebCore::ScreenOrientationType WebScreenOrientationManager::currentOrientation()
{
    if (m_currentOrientation)
        return *m_currentOrientation;

    auto sendResult = protectedPage()->sendSync(Messages::WebScreenOrientationManagerProxy::CurrentOrientation { });
    auto [currentOrientation] = sendResult.takeReplyOr(WebCore::naturalScreenOrientationType());
    if (!m_observers.isEmptyIgnoringNullReferences())
        m_currentOrientation = currentOrientation;
    return currentOrientation;
}

void WebScreenOrientationManager::orientationDidChange(WebCore::ScreenOrientationType orientation)
{
    m_currentOrientation = orientation;
    for (auto& observer : m_observers)
        observer.screenOrientationDidChange(orientation);
}

void WebScreenOrientationManager::lock(WebCore::ScreenOrientationLockType lockType, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&& completionHandler)
{
    protectedPage()->sendWithAsyncReply(Messages::WebScreenOrientationManagerProxy::Lock { lockType }, WTFMove(completionHandler));
}

void WebScreenOrientationManager::unlock()
{
    protectedPage()->send(Messages::WebScreenOrientationManagerProxy::Unlock { });
}

void WebScreenOrientationManager::addObserver(WebCore::ScreenOrientationManagerObserver& observer)
{
    bool wasEmpty = m_observers.isEmptyIgnoringNullReferences();
    m_observers.add(observer);
    if (wasEmpty)
        protectedPage()->send(Messages::WebScreenOrientationManagerProxy::SetShouldSendChangeNotification { true });
}

void WebScreenOrientationManager::removeObserver(WebCore::ScreenOrientationManagerObserver& observer)
{
    m_observers.remove(observer);
    if (m_observers.isEmptyIgnoringNullReferences()) {
        m_currentOrientation = std::nullopt;
        protectedPage()->send(Messages::WebScreenOrientationManagerProxy::SetShouldSendChangeNotification { false });
    }
}

} // namespace WebKit
