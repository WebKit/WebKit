/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "_WKRemoteWebInspectorViewControllerInternal.h"

#if PLATFORM(MAC)

#import "APIDebuggableInfo.h"
#import "APIInspectorConfiguration.h"
#import "DebuggableInfoData.h"
#import "RemoteWebInspectorUIProxy.h"
#import "WKWebViewInternal.h"
#import "_WKInspectorConfigurationInternal.h"
#import "_WKInspectorDebuggableInfoInternal.h"
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WeakObjCPtr.h>

#if ENABLE(INSPECTOR_EXTENSIONS)
#import "APIInspectorExtension.h"
#import "WKError.h"
#import "WebInspectorUIExtensionControllerProxy.h"
#import "_WKInspectorExtensionInternal.h"
#import <wtf/BlockPtr.h>
#endif


@interface _WKRemoteWebInspectorViewController ()
- (void)sendMessageToBackend:(NSString *)message;
- (void)closeFromFrontend;
@end

namespace WebKit {

class _WKRemoteWebInspectorUIProxyClient final : public RemoteWebInspectorUIProxyClient {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(_WKRemoteWebInspectorUIProxyClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(_WKRemoteWebInspectorUIProxyClient);
public:
    _WKRemoteWebInspectorUIProxyClient(_WKRemoteWebInspectorViewController *controller)
        : m_controller(controller)
    {
    }

    virtual ~_WKRemoteWebInspectorUIProxyClient()
    {
    }

    void sendMessageToBackend(const String& message) override
    {
        [m_controller.get() sendMessageToBackend:message.createNSString().get()];
    }

    void closeFromFrontend() override
    {
        [m_controller.get() closeFromFrontend];
    }
    
    Ref<API::InspectorConfiguration> configurationForRemoteInspector(RemoteWebInspectorUIProxy& inspector) override
    {
        return downcast<API::InspectorConfiguration>([[m_controller.get() configuration] _apiObject]);
    }

private:
    WeakObjCPtr<_WKRemoteWebInspectorViewController> m_controller;
};

} // namespace WebKit

@implementation _WKRemoteWebInspectorViewController {
    const std::unique_ptr<WebKit::_WKRemoteWebInspectorUIProxyClient> m_remoteInspectorClient;
    _WKInspectorConfiguration *_configuration;
}

- (instancetype)initWithConfiguration:(_WKInspectorConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    _configuration = [configuration copy];

    lazyInitialize(m_remoteInspectorProxy, WebKit::RemoteWebInspectorUIProxy::create());
    lazyInitialize(m_remoteInspectorClient, makeUnique<WebKit::_WKRemoteWebInspectorUIProxyClient>(self));
    m_remoteInspectorProxy->setClient(m_remoteInspectorClient.get());

    return self;
}

- (void)dealloc
{
    m_remoteInspectorProxy->setClient(nullptr);
    SUPPRESS_UNRETAINED_ARG [_configuration release];
    [super dealloc];
}

- (NSWindow *)window
{
    return m_remoteInspectorProxy->window();
}

- (WKWebView *)webView
{
    return m_remoteInspectorProxy->webView();
}

- (void)loadForDebuggable:(_WKInspectorDebuggableInfo *)debuggableInfo backendCommandsURL:(NSURL *)backendCommandsURL
{
    m_remoteInspectorProxy->initialize(downcast<API::DebuggableInfo>([debuggableInfo _apiObject]), backendCommandsURL.absoluteString);
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

// MARK: RemoteWebInspectorUIProxyClient methods

- (void)sendMessageToBackend:(NSString *)message
{
    RetainPtr delegate = _delegate;
    if (delegate && [delegate respondsToSelector:@selector(inspectorViewController:sendMessageToBackend:)])
        [delegate inspectorViewController:self sendMessageToBackend:message];
}

- (void)closeFromFrontend
{
    RetainPtr delegate = _delegate;
    if (delegate && [delegate respondsToSelector:@selector(inspectorViewControllerInspectorDidClose:)])
        [delegate inspectorViewControllerInspectorDidClose:self];
}

// MARK: _WKRemoteWebInspectorViewControllerPrivate methods

- (void)_setDiagnosticLoggingDelegate:(id<_WKDiagnosticLoggingDelegate>)delegate
{
    self.webView._diagnosticLoggingDelegate = delegate;
    m_remoteInspectorProxy->setDiagnosticLoggingAvailable(!!delegate);
}

// MARK: _WKInspectorExtensionHost methods

- (WKWebView *)extensionHostWebView
{
    return self.webView;
}

- (void)registerExtensionWithID:(NSString *)extensionID extensionBundleIdentifier:(NSString *)extensionBundleIdentifier displayName:(NSString *)displayName completionHandler:(void(^)(NSError *, _WKInspectorExtension *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // If this method is called prior to creating a frontend with -loadForDebuggable:backendCommandsURL:, it will not succeed.
    if (!m_remoteInspectorProxy->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get(), nil);
        return;
    }

    protect(m_remoteInspectorProxy->extensionController())->registerExtension(extensionID, extensionBundleIdentifier, displayName, [protectedExtensionID = retainPtr(extensionID), protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<Ref<API::InspectorExtension>, Inspector::ExtensionError> result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get(), nil);
            return;
        }

        capturedBlock(nil, protectedWrapper(result.value()).get());
    });
#else
    completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]).get(), nil);
#endif
}

- (void)unregisterExtension:(_WKInspectorExtension *)extension completionHandler:(void(^)(NSError *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // If this method is called prior to creating a frontend with -loadForDebuggable:backendCommandsURL:, it will not succeed.
    if (!m_remoteInspectorProxy->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get());
        return;
    }
    protect(m_remoteInspectorProxy->extensionController())->unregisterExtension(extension.extensionID, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<void, Inspector::ExtensionError> result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
#else
    completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:nil]).get());
#endif
}

- (void)showConsole
{
    m_remoteInspectorProxy->showConsole();
}

- (void)showResources
{
    m_remoteInspectorProxy->showResources();
}

- (void)showExtensionTabWithIdentifier:(NSString *)extensionTabIdentifier completionHandler:(void(^)(NSError *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // It is an error to call this method prior to creating a frontend (i.e., with -connect or -show).
    if (!m_remoteInspectorProxy->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get());
        return;
    }

    protect(m_remoteInspectorProxy->extensionController())->showExtensionTab(extensionTabIdentifier, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (Expected<void, Inspector::ExtensionError>&& result) mutable {
        if (!result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.error()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
#endif
}

- (void)navigateExtensionTabWithIdentifier:(NSString *)extensionTabIdentifier toURL:(NSURL *)url completionHandler:(void(^)(NSError *))completionHandler
{
#if ENABLE(INSPECTOR_EXTENSIONS)
    // It is an error to call this method prior to creating a frontend (i.e., with -connect or -show).
    if (!m_remoteInspectorProxy->extensionController()) {
        completionHandler(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(Inspector::ExtensionError::InvalidRequest).createNSString().get() }]).get());
        return;
    }

    protect(m_remoteInspectorProxy->extensionController())->navigateTabForExtension(extensionTabIdentifier, url, [protectedSelf = retainPtr(self), capturedBlock = makeBlockPtr(completionHandler)] (const std::optional<Inspector::ExtensionError> result) mutable {
        if (result) {
            capturedBlock(adoptNS([[NSError alloc] initWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSLocalizedFailureReasonErrorKey: Inspector::extensionErrorToString(result.value()).createNSString().get() }]).get());
            return;
        }

        capturedBlock(nil);
    });
#endif
}

@end

#endif // PLATFORM(MAC)
