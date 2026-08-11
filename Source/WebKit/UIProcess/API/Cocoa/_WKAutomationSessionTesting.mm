/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "_WKAutomationSessionPrivateForTesting.h"

#if ENABLE(REMOTE_INSPECTOR)

#import "WKWebViewInternal.h"
#import "WebAutomationSession.h"
#import "WebPageProxy.h"
#import "_WKAutomationSessionInternal.h"
#import <JavaScriptCore/InspectorFrontendChannel.h>
#import <objc/runtime.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>

namespace WebKit {

// A FrontendChannel that funnels every outbound JSON-RPC message into a
// caller-supplied Objective-C block. Used by API tests as a stand-in for
// the real RemoteInspector transport so a test can observe what the
// automation backend would send back to the driver. Lifetime is managed
// by the Objective-C wrapper @_WKAutomationSessionTestChannel below; the
// channel disconnects from the session before deallocation.
class TestFrontendChannel final : public Inspector::FrontendChannel {
    WTF_MAKE_TZONE_ALLOCATED(TestFrontendChannel);
public:
    explicit TestFrontendChannel(void (^handler)(NSString *))
        : m_handler(makeBlockPtr(handler))
    {
    }

    ConnectionType connectionType() const final { return ConnectionType::Local; }

    void sendMessageToFrontend(const String& message) final
    {
        if (m_handler)
            m_handler(message.createNSString().get());
    }

private:
    BlockPtr<void(NSString *)> m_handler;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(TestFrontendChannel);

} // namespace WebKit

@interface _WKAutomationSessionTestChannel : NSObject {
@public
    std::unique_ptr<WebKit::TestFrontendChannel> _channel;
    WeakObjCPtr<_WKAutomationSession> _session;
}
@end

@implementation _WKAutomationSessionTestChannel
- (void)dealloc
{
    RetainPtr session = _session.get();
    if (_channel && session) {
        Ref automationSession = downcast<WebKit::WebAutomationSession>([session _apiObject]);
        automationSession->disconnect(*_channel);
    }
    [super dealloc];
}
@end

static void *kAutomationTestChannelKey = &kAutomationTestChannelKey;

@implementation _WKAutomationSession (PrivateForTesting)

- (NSString *)_registerWebViewForTesting:(WKWebView *)webView
{
    RefPtr page = [webView _page].get();
    if (!page)
        return nil;
    Ref session = downcast<WebKit::WebAutomationSession>([self _apiObject]);
    return session->handleForWebPageProxy(*page).createNSString().autorelease();
}

- (void)_setMessageToFrontendHandlerForTesting:(void (^)(NSString *))handler
{
    Ref session = downcast<WebKit::WebAutomationSession>([self _apiObject]);

    // Tear down any previously installed test channel first. The wrapper's
    // dealloc disconnects the channel from the session, so just dropping the
    // associated object would race; do the disconnect deterministically.
    RetainPtr existing = (_WKAutomationSessionTestChannel *)objc_getAssociatedObject(self, kAutomationTestChannelKey);
    if (existing) {
        if (existing->_channel)
            session->disconnect(*existing->_channel);
        existing->_channel = nullptr;
        existing->_session = nil;
        objc_setAssociatedObject(self, kAutomationTestChannelKey, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    if (!handler)
        return;

    RetainPtr wrapper = adoptNS([[_WKAutomationSessionTestChannel alloc] init]);
    wrapper->_channel = makeUnique<WebKit::TestFrontendChannel>(handler);
    wrapper->_session = self;

    session->connect(*wrapper->_channel);

    objc_setAssociatedObject(self, kAutomationTestChannelKey, wrapper.get(), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

- (void)_dispatchMessageFromRemoteForTesting:(NSString *)message
{
    Ref session = downcast<WebKit::WebAutomationSession>([self _apiObject]);
    session->dispatchMessageFromRemote(String(message));
}

@end

#endif // ENABLE(REMOTE_INSPECTOR)
