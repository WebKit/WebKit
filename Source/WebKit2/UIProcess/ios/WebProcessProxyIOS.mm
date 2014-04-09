/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WebProcessProxy.h"

#if PLATFORM(IOS)

#import <AssertionServices/BKSProcessAssertion.h>
#import <WebCore/NotImplemented.h>

const BKSProcessAssertionFlags BackgroundTabFlags = (BKSProcessAssertionAllowIdleSleep);
const BKSProcessAssertionFlags ForegroundTabFlags = (BKSProcessAssertionAllowIdleSleep | BKSProcessAssertionPreventTaskSuspend | BKSProcessAssertionWantsForegroundResourcePriority | BKSProcessAssertionPreventTaskThrottleDown);

namespace WebKit {

bool WebProcessProxy::fullKeyboardAccessEnabled()
{
    notImplemented();
    return false;
}

static bool shouldUseXPC()
{
    if (id value = [[NSUserDefaults standardUserDefaults] objectForKey:@"WebKit2UseXPCServiceForWebProcess"])
        return [value boolValue];

#if USE(XPC_SERVICES)
    return true;
#else
    return false;
#endif
}

void WebProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
    // We want the web process to match the architecture of the UI process.
    launchOptions.architecture = ProcessLauncher::LaunchOptions::MatchCurrentArchitecture;
    launchOptions.executableHeap = false;

    launchOptions.useXPC = shouldUseXPC();
}

bool WebProcessProxy::allPagesAreProcessSuppressible() const
{
    notImplemented();
    return false;
}

void WebProcessProxy::updateProcessSuppressionState()
{
    notImplemented();
}

void WebProcessProxy::updateProcessState()
{
#if USE(XPC_SERVICES)
    if (state() != State::Running)
        return;

    xpc_connection_t xpcConnection = connection()->xpcConnection();
    if (!xpcConnection)
        return;

    ProcessAssertionState assertionState = AssertionBackground;
    for (const auto& page : m_pageMap.values()) {
        if (page->isInWindow()) {
            assertionState = AssertionForeground;
            break;
        }
    }
    BKSProcessAssertionFlags flags = (assertionState == AssertionForeground) ? ForegroundTabFlags : BackgroundTabFlags;
    
    if (!m_assertion) {
        pid_t pid = xpc_connection_get_pid(xpcConnection);
        BKSProcessAssertionAcquisitionHandler handler = ^(BOOL acquired) {
            if (!acquired) {
                LOG_ERROR("Unable to acquire assertion for WebContent process %d", pid);
                ASSERT_NOT_REACHED();
            }
        };
        m_assertion = adoptNS([[BKSProcessAssertion alloc] initWithPID:pid flags:flags reason:BKSProcessAssertionReasonExtension name:@"Web content visible" withHandler:handler]);
    } else if (m_assertionState != assertionState)
        [m_assertion setFlags:flags];
    
    m_assertionState = assertionState;
#endif
}

} // namespace WebKit

#endif // PLATFORM(IOS)
