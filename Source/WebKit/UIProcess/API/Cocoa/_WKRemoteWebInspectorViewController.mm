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
#import "_WKRemoteWebInspectorViewController.h"

#if PLATFORM(MAC)

#import "APIDebuggableInfo.h"
#import "DebuggableInfoData.h"
#import "RemoteWebInspectorProxy.h"
#import "WKWebViewInternal.h"
#import "_WKInspectorDebuggableInfoInternal.h"

@interface _WKRemoteWebInspectorViewController ()
- (void)sendMessageToBackend:(NSString *)message;
- (void)closeFromFrontend;
@end

namespace WebKit {

class _WKRemoteWebInspectorProxyClient final : public RemoteWebInspectorProxyClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    _WKRemoteWebInspectorProxyClient(_WKRemoteWebInspectorViewController *controller)
        : m_controller(controller)
    {
    }

    virtual ~_WKRemoteWebInspectorProxyClient()
    {
    }

    void sendMessageToBackend(const String& message) override
    {
        [m_controller sendMessageToBackend:message];
    }

    void closeFromFrontend() override
    {
        [m_controller closeFromFrontend];
    }

private:
    _WKRemoteWebInspectorViewController *m_controller;
};

} // namespace WebKit

@implementation _WKRemoteWebInspectorViewController {
    RefPtr<WebKit::RemoteWebInspectorProxy> m_remoteInspectorProxy;
    std::unique_ptr<WebKit::_WKRemoteWebInspectorProxyClient> m_remoteInspectorClient;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    m_remoteInspectorProxy = WebKit::RemoteWebInspectorProxy::create();
    m_remoteInspectorClient = makeUnique<WebKit::_WKRemoteWebInspectorProxyClient>(self);
    m_remoteInspectorProxy->setClient(m_remoteInspectorClient.get());

    return self;
}

- (NSWindow *)window
{
    return m_remoteInspectorProxy->window();
}

- (WKWebView *)webView
{
    return m_remoteInspectorProxy->webView();
}

static _WKInspectorDebuggableType legacyDebuggableTypeToModernDebuggableType(WKRemoteWebInspectorDebuggableType debuggableType)
{
    switch (debuggableType) {
    case WKRemoteWebInspectorDebuggableTypeITML:
        return _WKInspectorDebuggableTypeITML;
    case WKRemoteWebInspectorDebuggableTypeJavaScript:
        return _WKInspectorDebuggableTypeJavaScript;
    case WKRemoteWebInspectorDebuggableTypePage:
        return _WKInspectorDebuggableTypePage;
    case WKRemoteWebInspectorDebuggableTypeServiceWorker:
        return _WKInspectorDebuggableTypeServiceWorker;
    case WKRemoteWebInspectorDebuggableTypeWebPage:
        return _WKInspectorDebuggableTypeWebPage;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    case WKRemoteWebInspectorDebuggableTypeWeb:
        return _WKInspectorDebuggableTypeWebPage;
ALLOW_DEPRECATED_DECLARATIONS_END
    }
}

// FIXME: remove this variant when all callers have moved off of it.
- (void)loadForDebuggableType:(WKRemoteWebInspectorDebuggableType)debuggableType backendCommandsURL:(NSURL *)backendCommandsURL
{
    _WKInspectorDebuggableInfo *debuggableInfo = [_WKInspectorDebuggableInfo new];
    debuggableInfo.debuggableType = legacyDebuggableTypeToModernDebuggableType(debuggableType);
    debuggableInfo.targetPlatformName = @"macOS";
    debuggableInfo.targetBuildVersion = @"Unknown";
    debuggableInfo.targetProductVersion = @"Unknown";
    debuggableInfo.targetIsSimulator = false;
    [self loadForDebuggable:debuggableInfo backendCommandsURL:backendCommandsURL];
}

- (void)loadForDebuggable:(_WKInspectorDebuggableInfo *)debuggableInfo backendCommandsURL:(NSURL *)backendCommandsURL
{
    m_remoteInspectorProxy->load(static_cast<API::DebuggableInfo&>([debuggableInfo _apiObject]), backendCommandsURL.absoluteString);
}

- (void)close
{
    m_remoteInspectorProxy->closeFromBackend();
}

- (void)show
{
    m_remoteInspectorProxy->show();
}

- (void)sendMessageToFrontend:(NSString *)message
{
    m_remoteInspectorProxy->sendMessageToFrontend(message);
}

- (void)sendMessageToBackend:(NSString *)message
{
    if (_delegate && [_delegate respondsToSelector:@selector(inspectorViewController:sendMessageToBackend:)])
        [_delegate inspectorViewController:self sendMessageToBackend:message];
}

- (void)closeFromFrontend
{
    if (_delegate && [_delegate respondsToSelector:@selector(inspectorViewControllerInspectorDidClose:)])
        [_delegate inspectorViewControllerInspectorDidClose:self];
}

// MARK: _WKRemoteWebInspectorViewControllerPrivate methods

- (void)_setDiagnosticLoggingDelegate:(id<_WKDiagnosticLoggingDelegate>)delegate
{
    self.webView._diagnosticLoggingDelegate = delegate;
    m_remoteInspectorProxy->setDiagnosticLoggingAvailable(!!delegate);
}

@end

#endif // PLATFORM(MAC)
