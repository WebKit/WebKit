/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#import "AutomationClient.h"

#if ENABLE(REMOTE_INSPECTOR)

#import "WKProcessPool.h"
#import "_WKAutomationDelegate.h"
#import "_WKAutomationSessionConfiguration.h"
#import <JavaScriptCore/RemoteInspector.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cf/CFBundleSPI.h>
#import <wtf/text/WTFString.h>

using namespace Inspector;

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(AutomationClient);

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
AutomationClient::AutomationClient(WKProcessPool *processPool, id <_WKAutomationDelegate> delegate)
    : m_processPool(processPool)
    , m_delegate(delegate)
{
    m_delegateMethods.allowsRemoteAutomation = [delegate respondsToSelector:@selector(_processPoolAllowsRemoteAutomation:)];
    m_delegateMethods.requestAutomationSession = [delegate respondsToSelector:@selector(_processPool:didRequestAutomationSessionWithIdentifier:configuration:)];
    m_delegateMethods.requestedDebuggablesToWakeUp = [delegate respondsToSelector:@selector(_processPoolDidRequestInspectorDebuggablesToWakeUp:)];
    m_delegateMethods.browserNameForAutomation = [delegate respondsToSelector:@selector(_processPoolBrowserNameForAutomation:)];
    m_delegateMethods.browserVersionForAutomation = [delegate respondsToSelector:@selector(_processPoolBrowserVersionForAutomation:)];

    RemoteInspector::singleton().setClient(this);
}
ALLOW_DEPRECATED_DECLARATIONS_END

AutomationClient::~AutomationClient()
{
    RemoteInspector::singleton().setClient(nullptr);
}

// MARK: RemoteInspector::Client

bool AutomationClient::remoteAutomationAllowed() const
{
    if (m_delegateMethods.allowsRemoteAutomation)
        return [m_delegate.get() _processPoolAllowsRemoteAutomation:m_processPool.get().get()];

    return false;
}

void AutomationClient::requestAutomationSession(const String& sessionIdentifier, const RemoteInspector::Client::SessionCapabilities& sessionCapabilities)
{
    auto configuration = adoptNS([[_WKAutomationSessionConfiguration alloc] init]);
    [configuration setAcceptInsecureCertificates:sessionCapabilities.acceptInsecureCertificates];
    
    if (sessionCapabilities.allowInsecureMediaCapture)
        [configuration setAllowsInsecureMediaCapture:sessionCapabilities.allowInsecureMediaCapture.value()];
    if (sessionCapabilities.suppressICECandidateFiltering)
        [configuration setSuppressesICECandidateFiltering:sessionCapabilities.suppressICECandidateFiltering.value()];
    if (sessionCapabilities.alwaysAllowAutoplay)
        [configuration setAlwaysAllowAutoplay:sessionCapabilities.alwaysAllowAutoplay.value()];

    // Force clients to create and register a session asynchronously. Otherwise,
    // RemoteInspector will try to acquire its lock to register the new session and
    // deadlock because it's already taken while handling XPC messages.
    RunLoop::mainSingleton().dispatch([this, requestedSessionIdentifier = sessionIdentifier.createNSString(), configuration = WTFMove(configuration)] {
        if (m_delegateMethods.requestAutomationSession)
            [m_delegate.get() _processPool:m_processPool.get().get() didRequestAutomationSessionWithIdentifier:requestedSessionIdentifier.get() configuration:configuration.get()];
    });
}

// FIXME: Consider renaming AutomationClient and _WKAutomationDelegate to _WKInspectorDelegate since it isn't only used for automation now.
// http://webkit.org/b/221933
void AutomationClient::requestedDebuggablesToWakeUp()
{
    RunLoop::mainSingleton().dispatch([this] {
        if (m_delegateMethods.requestedDebuggablesToWakeUp)
            [m_delegate.get() _processPoolDidRequestInspectorDebuggablesToWakeUp:m_processPool.get().get()];
    });
}

String AutomationClient::browserName() const
{
    if (m_delegateMethods.browserNameForAutomation)
        return [m_delegate _processPoolBrowserNameForAutomation:m_processPool.get().get()];

    // Fall back to using the unlocalized app name (i.e., 'Safari').
    RetainPtr appBundle = [NSBundle mainBundle];
    if (RetainPtr<NSString> displayName = appBundle.get().infoDictionary[bridge_cast(_kCFBundleDisplayNameKey)])
        return displayName.get();
    return appBundle.get().infoDictionary[bridge_cast(kCFBundleNameKey)];
}

String AutomationClient::browserVersion() const
{
    if (m_delegateMethods.browserVersionForAutomation)
        return [m_delegate _processPoolBrowserVersionForAutomation:m_processPool.get().get()];

    // Fall back to using the app short version (i.e., '11.1.1').
    RetainPtr appBundle = [NSBundle mainBundle];
    return appBundle.get().infoDictionary[bridge_cast(_kCFBundleShortVersionStringKey)];
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
