/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)

#import "WebProcessPool.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebKit {

#if PLATFORM(MAC)
void WindowServerConnection::applicationWindowModificationsStopped(bool stopped)
{
    if (m_applicationWindowModificationsHaveStopped == stopped)
        return;
    m_applicationWindowModificationsHaveStopped = stopped;
    windowServerConnectionStateChanged();
}

void WindowServerConnection::windowServerConnectionStateChanged()
{
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->windowServerConnectionStateChanged();
}
#endif

void WindowServerConnection::hardwareConsoleStateChanged(HardwareConsoleState state)
{
    if (state == m_connectionState)
        return;

    m_connectionState = state;
    for (auto& processPool : WebProcessPool::allProcessPools())
        processPool->hardwareConsoleStateChanged();
}

WindowServerConnection& WindowServerConnection::singleton()
{
    static WindowServerConnection& windowServerConnection = *new WindowServerConnection;
    return windowServerConnection;
}

#if PLATFORM(MAC)
static bool registerOcclusionNotificationHandler(CGSNotificationType type, CGSNotifyConnectionProcPtr handler)
{
    CGSConnectionID mainConnection = CGSMainConnectionID();
    static bool notificationsEnabled;
    if (!notificationsEnabled) {
        if (CGSPackagesEnableConnectionOcclusionNotifications(mainConnection, true, nullptr) != kCGErrorSuccess)
            return false;
        if (CGSPackagesEnableConnectionWindowModificationNotifications(mainConnection, true, nullptr) != kCGErrorSuccess)
            return false;
        notificationsEnabled = true;
    }

    return CGSRegisterConnectionNotifyProc(mainConnection, handler, type, nullptr) == kCGErrorSuccess;
}
#endif

WindowServerConnection::WindowServerConnection()
    : m_applicationWindowModificationsHaveStopped(false)
{
#if PLATFORM(MAC)
    struct OcclusionNotificationHandler {
        CGSNotificationType notificationType;
        CGSNotifyConnectionProcPtr handler;
        const char* name;
    };

    static auto windowModificationsStarted = [](CGSNotificationType, void*, uint32_t, void*, CGSConnectionID) {
        WindowServerConnection::singleton().applicationWindowModificationsStopped(false);
    };

    static auto windowModificationsStopped = [](CGSNotificationType, void*, uint32_t, void*, CGSConnectionID) {
        WindowServerConnection::singleton().applicationWindowModificationsStopped(true);
    };

    static const OcclusionNotificationHandler occlusionNotificationHandlers[] = {
        { kCGSConnectionWindowModificationsStarted, windowModificationsStarted, "Application Window Modifications Started" },
        { kCGSConnectionWindowModificationsStopped, windowModificationsStopped, "Application Window Modifications Stopped" },
    };

    for (const auto& occlusionNotificationHandler : occlusionNotificationHandlers) {
        bool result = registerOcclusionNotificationHandler(occlusionNotificationHandler.notificationType, occlusionNotificationHandler.handler);
        UNUSED_PARAM(result);
        ASSERT_WITH_MESSAGE(result, "Registration of \"%s\" notification handler failed.\n", occlusionNotificationHandler.name);
    }
#endif

    static auto consoleWillDisconnect = [](CGSNotificationType, void*, uint32_t, void*) {
        WindowServerConnection::singleton().hardwareConsoleStateChanged(HardwareConsoleState::Disconnected);
    };
    static auto consoleWillConnect = [](CGSNotificationType, void*, uint32_t, void*) {
        WindowServerConnection::singleton().hardwareConsoleStateChanged(HardwareConsoleState::Connected);
    };

    struct ConnectionStateNotificationHandler {
        CGSNotificationType notificationType;
        CGSNotifyProcPtr handler;
        const char* name;
    };

    static const ConnectionStateNotificationHandler connectionStateNotificationHandlers[] = {
        { kCGSessionConsoleWillDisconnect, consoleWillDisconnect, "Console Disconnected" },
        { kCGSessionConsoleConnect, consoleWillConnect, "Console Connected" },
    };

    for (const auto& connectionStateNotificationHandler : connectionStateNotificationHandlers) {
        auto error = CGSRegisterNotifyProc(connectionStateNotificationHandler.handler, connectionStateNotificationHandler.notificationType, nullptr);
        UNUSED_PARAM(error);
        ASSERT_WITH_MESSAGE(!error, "Registration of \"%s\" notification handler failed.\n", connectionStateNotificationHandler.name);
    }
}

} // namespace WebKit

#endif // PLATFORM(MAC) || PLATFORM(MACCATALYST)
