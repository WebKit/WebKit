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

#include <wtf/OSObjectPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/spi/darwin/XPCSPI.h>

#if PLATFORM(IOS_FAMILY)
#include <wtf/Ref.h>
#endif

namespace WebKit {

class ProcessAndUIAssertion;

// ConnectionTerminationWatchdog does two things:
// 1) It sets a watchdog timer to kill the peered process.
// 2) On iOS, make the process runnable for the duration of the watchdog
//    to ensure it has a chance to terminate cleanly.
class XPCConnectionTerminationWatchdog {
public:
    static void startConnectionTerminationWatchdog(OSObjectPtr<xpc_connection_t>, Seconds interval);

private:
    XPCConnectionTerminationWatchdog(OSObjectPtr<xpc_connection_t>&&, Seconds interval);
    void watchdogTimerFired();

    OSObjectPtr<xpc_connection_t> m_xpcConnection;
    RunLoop::Timer m_watchdogTimer;
#if PLATFORM(IOS_FAMILY)
    Ref<ProcessAndUIAssertion> m_assertion;
#endif
};

}
