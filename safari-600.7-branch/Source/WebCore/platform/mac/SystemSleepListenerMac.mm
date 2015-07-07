/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "SystemSleepListenerMac.h"

#if PLATFORM(MAC)

#import <wtf/MainThread.h>

namespace WebCore {

std::unique_ptr<SystemSleepListener> SystemSleepListener::create(Client& client)
{
    return std::unique_ptr<SystemSleepListener>(new SystemSleepListenerMac(client));
}

SystemSleepListenerMac::SystemSleepListenerMac(Client& client)
    : SystemSleepListener(client)
    , m_weakPtrFactory(this)
    , m_sleepObserver(nil)
    , m_wakeObserver(nil)
{
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    NSOperationQueue *queue = [NSOperationQueue mainQueue];

    auto weakThis = m_weakPtrFactory.createWeakPtr();

    m_sleepObserver = [center addObserverForName:NSWorkspaceWillSleepNotification object:nil queue:queue usingBlock:^(NSNotification *) {
        callOnMainThread([weakThis] {
            if (weakThis)
                weakThis->m_client.systemWillSleep();
        });
    }];

    m_wakeObserver = [center addObserverForName:NSWorkspaceDidWakeNotification object:nil queue:queue usingBlock:^(NSNotification *) {
        callOnMainThread([weakThis] {
            if (weakThis)
                weakThis->m_client.systemDidWake();
        });
    }];
}

SystemSleepListenerMac::~SystemSleepListenerMac()
{
    NSNotificationCenter* center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center removeObserver:m_sleepObserver];
    [center removeObserver:m_wakeObserver];
}

}

#endif // PLATFORM(MAC)
