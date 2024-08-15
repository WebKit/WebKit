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

#pragma once

#include "MessageReceiver.h"
#include <WebCore/ScreenOrientationLockType.h>
#include <WebCore/ScreenOrientationType.h>
#include <wtf/CompletionHandler.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class Exception;
}

namespace WebKit {

class WebPageProxy;

class WebScreenOrientationManagerProxy final : public IPC::MessageReceiver {
    WTF_MAKE_TZONE_ALLOCATED(WebScreenOrientationManagerProxy);
public:
    WebScreenOrientationManagerProxy(WebPageProxy&, WebCore::ScreenOrientationType);
    ~WebScreenOrientationManagerProxy();

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void unlockIfNecessary();

    void setCurrentOrientation(WebCore::ScreenOrientationType);

private:
    std::optional<WebCore::Exception> platformShouldRejectLockRequest() const;

    // IPC message handlers.
    void currentOrientation(CompletionHandler<void(WebCore::ScreenOrientationType)>&&);
    void lock(WebCore::ScreenOrientationLockType, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&);
    void unlock();
    void setShouldSendChangeNotification(bool);

    WebPageProxy& m_page;
    WebCore::ScreenOrientationType m_currentOrientation;
    std::optional<WebCore::ScreenOrientationType> m_currentlyLockedOrientation;
    CompletionHandler<void(std::optional<WebCore::Exception>&&)> m_currentLockRequest;
    bool m_shouldSendChangeNotifications { false };
};

} // namespace WebKit
