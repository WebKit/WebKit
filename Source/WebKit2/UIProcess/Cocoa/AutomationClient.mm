/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if WK_API_ENABLED

#if ENABLE(REMOTE_INSPECTOR)

#import "WKProcessPool.h"
#import "_WKAutomationDelegate.h"
#import <JavaScriptCore/RemoteInspector.h>
#import <wtf/text/WTFString.h>

using namespace Inspector;

namespace WebKit {

AutomationClient::AutomationClient(WKProcessPool *processPool, id <_WKAutomationDelegate> delegate)
    : m_processPool(processPool)
    , m_delegate(delegate)
{
    m_delegateMethods.allowsRemoteAutomation = [delegate respondsToSelector:@selector(_processPoolAllowsRemoteAutomation:)];
    m_delegateMethods.requestAutomationSession = [delegate respondsToSelector:@selector(_processPool:didRequestAutomationSessionWithIdentifier:)];

    RemoteInspector::singleton().setRemoteInspectorClient(this);
}

AutomationClient::~AutomationClient()
{
    RemoteInspector::singleton().setRemoteInspectorClient(nullptr);
}

void AutomationClient::didRequestAutomationSession(WebKit::WebProcessPool*, const String& sessionIdentifier)
{
    requestAutomationSession(sessionIdentifier);
}

bool AutomationClient::remoteAutomationAllowed() const
{
    if (m_delegateMethods.allowsRemoteAutomation)
        return [m_delegate.get() _processPoolAllowsRemoteAutomation:m_processPool];

    return false;
}

void AutomationClient::requestAutomationSession(const String& sessionIdentifier)
{
    // Force clients to create and register a session asynchronously. Otherwise,
    // RemoteInspector will try to acquire its lock to register the new session and
    // deadlock because it's already taken while handling XPC messages.

    NSString *retainedIdentifier = sessionIdentifier;
    dispatch_async(dispatch_get_main_queue(), ^{
        if (m_delegateMethods.requestAutomationSession)
            [m_delegate.get() _processPool:m_processPool didRequestAutomationSessionWithIdentifier:retainedIdentifier];
    });
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)

#endif // WK_API_ENABLED
