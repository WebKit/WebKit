/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#import "AuxiliaryProcess.h"

#import "WKCrashReporter.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FloatingPointEnvironment.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <mach/task.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

namespace WebKit {

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static void initializeTimerCoalescingPolicy()
{
    // Set task_latency and task_throughput QOS tiers as appropriate for a visible application.
    struct task_qos_policy qosinfo = { LATENCY_QOS_TIER_0, THROUGHPUT_QOS_TIER_0 };
    kern_return_t kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
}
#endif

void AuxiliaryProcess::platformInitialize(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    initializeTimerCoalescingPolicy();
#endif

    FloatingPointEnvironment& floatingPointEnvironment = FloatingPointEnvironment::singleton();
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    floatingPointEnvironment.enableDenormalSupport();
#endif
    floatingPointEnvironment.saveMainThreadEnvironment();

    [[NSFileManager defaultManager] changeCurrentDirectoryPath:[[NSBundle mainBundle] bundlePath]];

    WebCore::setApplicationBundleIdentifier(parameters.clientBundleIdentifier);
    setApplicationSDKVersion(parameters.clientSDKVersion);
}

void AuxiliaryProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName)
{
    auto errorMessage = makeString("Received invalid message: '", description(messageName), "' (", messageName, ')');
    logAndSetCrashLogMessage(errorMessage.utf8().data());
    CRASH_WITH_INFO(WTF::enumToUnderlyingType(messageName));
}

bool AuxiliaryProcess::parentProcessHasEntitlement(const char* entitlement)
{
    return WTF::hasEntitlement(m_connection->xpcConnection(), entitlement);
}

void AuxiliaryProcess::platformStopRunLoop()
{
    XPCServiceExit(WTFMove(m_priorityBoostMessage));
}

}
