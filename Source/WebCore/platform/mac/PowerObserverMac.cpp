/*
 * Copyright (C) 2006, 2010, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#if PLATFORM(MAC)
#import "PowerObserverMac.h"

namespace WebCore {

PowerObserver::PowerObserver(WTF::Function<void()>&& powerOnHander)
    : m_powerOnHander(WTFMove(powerOnHander))
    , m_powerConnection(0)
    , m_notificationPort(nullptr)
    , m_notifierReference(0)
    , m_dispatchQueue(dispatch_queue_create("com.apple.WebKit.PowerObserver", 0))
{
    m_powerConnection = IORegisterForSystemPower(this, &m_notificationPort, [](void* context, io_service_t service, uint32_t messageType, void* messageArgument) {
        static_cast<PowerObserver*>(context)->didReceiveSystemPowerNotification(service, messageType, messageArgument);
    }, &m_notifierReference);
    if (!m_powerConnection)
        return;

    IONotificationPortSetDispatchQueue(m_notificationPort, m_dispatchQueue);
}

PowerObserver::~PowerObserver()
{
    if (!m_powerConnection)
        return;

    dispatch_release(m_dispatchQueue);

    IODeregisterForSystemPower(&m_notifierReference);
    IOServiceClose(m_powerConnection);
    IONotificationPortDestroy(m_notificationPort);
}

void PowerObserver::didReceiveSystemPowerNotification(io_service_t, uint32_t messageType, void* messageArgument)
{
    IOAllowPowerChange(m_powerConnection, reinterpret_cast<long>(messageArgument));

    // We only care about the "wake from sleep" message.
    if (messageType != kIOMessageSystemWillPowerOn)
        return;

    // We need to restart the timer on the main thread.
    CFRunLoopPerformBlock(CFRunLoopGetMain(), kCFRunLoopCommonModes, ^() {
        m_powerOnHander();
    });
}

} // namespace WebCore

#endif
