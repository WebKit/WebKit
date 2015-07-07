/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WindowServerConnection.h"

#import "WebContext.h"
#import "WebKitSystemInterface.h"

namespace WebKit {

#if HAVE(WINDOW_SERVER_OCCLUSION_NOTIFICATIONS)

void WindowServerConnection::applicationWindowModificationsStopped(bool stopped)
{
    if (m_applicationWindowModificationsHaveStopped == stopped)
        return;
    m_applicationWindowModificationsHaveStopped = stopped;
    windowServerConnectionStateChanged();
}

void WindowServerConnection::applicationWindowModificationsStarted(uint32_t, void*, uint32_t, void*, uint32_t)
{
    WindowServerConnection::shared().applicationWindowModificationsStopped(false);
}

void WindowServerConnection::applicationWindowModificationsStopped(uint32_t, void*, uint32_t, void*, uint32_t)
{
    WindowServerConnection::shared().applicationWindowModificationsStopped(true);
}

void WindowServerConnection::windowServerConnectionStateChanged()
{
    for (auto* context : WebContext::allContexts())
        context->windowServerConnectionStateChanged();
}

#endif

WindowServerConnection& WindowServerConnection::shared()
{
    static WindowServerConnection& windowServerConnection = *new WindowServerConnection;
    return windowServerConnection;
}

WindowServerConnection::WindowServerConnection()
    : m_applicationWindowModificationsHaveStopped(false)
{
#if HAVE(WINDOW_SERVER_OCCLUSION_NOTIFICATIONS)
    struct OcclusionNotificationHandler {
        WKOcclusionNotificationType notificationType;
        WKOcclusionNotificationHandler handler;
        const char* name;
    };

    static const OcclusionNotificationHandler occlusionNotificationHandlers[] = {
        { WKOcclusionNotificationTypeApplicationWindowModificationsStarted, applicationWindowModificationsStarted, "Application Window Modifications Started" },
        { WKOcclusionNotificationTypeApplicationWindowModificationsStopped, applicationWindowModificationsStopped, "Application Window Modifications Stopped" },
    };

    for (const auto& occlusionNotificationHandler : occlusionNotificationHandlers) {
        bool result = WKRegisterOcclusionNotificationHandler(occlusionNotificationHandler.notificationType, occlusionNotificationHandler.handler);
        UNUSED_PARAM(result);
        ASSERT_WITH_MESSAGE(result, "Registration of \"%s\" notification handler failed.\n", occlusionNotificationHandler.name);
    }
#endif
}

} // namespace WebKit

