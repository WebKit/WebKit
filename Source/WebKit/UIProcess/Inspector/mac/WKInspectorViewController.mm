/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#import "WKInspectorViewController.h"

#if PLATFORM(MAC)

#import "APINavigation.h"
#import <WebKit/WKFrameInfo.h>
#import "WKInspectorResourceURLSchemeHandler.h"
#import "WKInspectorWKWebView.h"
#import <WebKit/WKNavigationAction.h>
#import <WebKit/WKNavigationDelegate.h>
#import "WKOpenPanelParameters.h"
#import <WebKit/WKPreferencesPrivate.h>
#import "WKProcessPoolInternal.h"
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import "WebInspectorUIProxy.h"
#import "WebInspectorUtilities.h"
#import "WebPageProxy.h"
#import "_WKInspectorConfigurationInternal.h"
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

static NSString * const WKInspectorResourceScheme = @"inspector-resource";

@interface WKInspectorViewController () <WKUIDelegate, WKNavigationDelegate, WKInspectorWKWebViewDelegate>
@end

@implementation WKInspectorViewController {
    NakedPtr<WebKit::WebPageProxy> _inspectedPage;
    RetainPtr<WKInspectorWKWebView> _webView;
    WeakObjCPtr<id <WKInspectorViewControllerDelegate>> _delegate;
    RetainPtr<_WKInspectorConfiguration> _configuration;
}

- (instancetype)initWithConfiguration:(_WKInspectorConfiguration *)configuration inspectedPage:(NakedPtr<WebKit::WebPageProxy>)inspectedPage
{
    if (!(self = [super init]))
        return nil;

    _configuration = adoptNS([configuration copy]);

    // The (local) inspected page is nil if the controller is hosting a Remote Web Inspector view.
    _inspectedPage = inspectedPage;

    return self;
}

- (void)dealloc
{
    if (_webView) {
        [_webView setUIDelegate:nil];
        [_webView setNavigationDelegate:nil];
        [_webView setInspectorWKWebViewDelegate:nil];
        _webView = nil;
    }

    [super dealloc];
}

- (id <WKInspectorViewControllerDelegate>)delegate
{
    return _delegate.getAutoreleased();
}

- (WKWebView *)webView
{
    // Construct lazily so the client can set the delegate before the WebView is created.
    if (!_webView) {
        NSRect initialFrame = NSMakeRect(0, 0, WebKit::WebInspectorUIProxy::initialWindowWidth, WebKit::WebInspectorUIProxy::initialWindowHeight);
        _webView = adoptNS([[WKInspectorWKWebView alloc] initWithFrame:initialFrame configuration:self.webViewConfiguration]);
        [_webView setUIDelegate:self];
        [_webView setNavigationDelegate:self];
        [_webView setInspectorWKWebViewDelegate:self];
        [_webView _setAutomaticallyAdjustsContentInsets:NO];
        [_webView _setUseSystemAppearance:YES];
        [_webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    }

    return _webView.get();
}

- (void)setDelegate:(id <WKInspectorViewControllerDelegate>)delegate
{
    _delegate = delegate;
}

- (WKWebViewConfiguration *)webViewConfiguration
{
    RetainPtr<WKWebViewConfiguration> configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr<WKInspectorResourceURLSchemeHandler> inspectorSchemeHandler = adoptNS([WKInspectorResourceURLSchemeHandler new]);
    RetainPtr<NSMutableSet<NSString *>> allowedURLSchemes = adoptNS([[NSMutableSet alloc] initWithObjects:WKInspectorResourceScheme, nil]);
    for (auto& pair : _configuration->_configuration->urlSchemeHandlers())
        [allowedURLSchemes addObject:pair.second];

    [inspectorSchemeHandler setAllowedURLSchemesForCSP:allowedURLSchemes.get()];
    [configuration setURLSchemeHandler:inspectorSchemeHandler.get() forURLScheme:WKInspectorResourceScheme];

    WKPreferences *preferences = configuration.get().preferences;
    preferences._allowFileAccessFromFileURLs = YES;
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    [configuration _setAllowTopNavigationToDataURLs:YES];
    preferences._storageBlockingPolicy = _WKStorageBlockingPolicyAllowAll;
    preferences._javaScriptRuntimeFlags = 0;

#ifndef NDEBUG
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences._developerExtrasEnabled = YES;
    preferences._logsPageMessagesToSystemConsoleEnabled = YES;
#endif

    preferences._diagnosticLoggingEnabled = YES;

    [_configuration applyToWebViewConfiguration:configuration.get()];
    
    if (!!_delegate && [_delegate respondsToSelector:@selector(inspectorViewControllerInspectorIsUnderTest:)]) {
        if ([_delegate inspectorViewControllerInspectorIsUnderTest:self]) {
            preferences._hiddenPageDOMTimerThrottlingEnabled = NO;
            preferences._pageVisibilityBasedProcessSuppressionEnabled = NO;
        }
    }

    // WKInspectorConfiguration allows the client to specify a process pool to use.
    // If not specified or the inspection level is >1, use the default strategy.
    // This ensures that Inspector^2 cannot be affected by client (mis)configuration.
    auto* customProcessPool = configuration.get().processPool;
    auto inspectorLevel = WebKit::inspectorLevelForPage(_inspectedPage);
    auto useDefaultProcessPool = inspectorLevel > 1 || !customProcessPool;
    if (customProcessPool && !useDefaultProcessPool)
        WebKit::prepareProcessPoolForInspector(*customProcessPool->_processPool.get());

    if (useDefaultProcessPool)
        [configuration setProcessPool:wrapper(WebKit::defaultInspectorProcessPool(inspectorLevel))];

    // Ensure that a page group identifier is set. This is for computing inspection levels.
    if (!configuration.get()._groupIdentifier)
        [configuration _setGroupIdentifier:WebKit::defaultInspectorPageGroupIdentifierForPage(_inspectedPage)];
    
    return configuration.autorelease();
}

+ (BOOL)viewIsInspectorWebView:(NSView *)view
{
    return [view isKindOfClass:[WKInspectorWKWebView class]];
}

+ (NSURL *)URLForInspectorResource:(NSString *)resource
{
    return [NSURL URLWithString:[NSString stringWithFormat:@"%@:///%@", WKInspectorResourceScheme, resource]].URLByStandardizingPath;
}

// MARK: WKUIDelegate methods

- (void)_webView:(WKWebView *)webView getWindowFrameWithCompletionHandler:(void (^)(CGRect))completionHandler
{
    if (!_webView.get().window)
        completionHandler(CGRectZero);
    else
        completionHandler(NSRectToCGRect([webView frame]));
}

- (void)_webView:(WKWebView *)webView setWindowFrame:(CGRect)frame
{
    if (!_webView.get().window)
        return;

    [_webView.get().window setFrame:NSRectFromCGRect(frame) display:YES];
}

- (void)webView:(WKWebView *)webView runOpenPanelWithParameters:(WKOpenPanelParameters *)parameters initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSArray<NSURL *> *URLs))completionHandler
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    openPanel.allowsMultipleSelection = parameters.allowsMultipleSelection;

    auto reportSelectedFiles = ^(NSInteger result) {
        if (result == NSModalResponseOK)
            completionHandler(openPanel.URLs);
        else
            completionHandler(nil);
    };

    if (_webView.get().window)
        [openPanel beginSheetModalForWindow:_webView.get().window completionHandler:reportSelectedFiles];
    else
        reportSelectedFiles([openPanel runModal]);
}

- (void)_webView:(WKWebView *)webView decideDatabaseQuotaForSecurityOrigin:(WKSecurityOrigin *)securityOrigin currentQuota:(unsigned long long)currentQuota currentOriginUsage:(unsigned long long)currentOriginUsage currentDatabaseUsage:(unsigned long long)currentUsage expectedUsage:(unsigned long long)expectedUsage decisionHandler:(void (^)(unsigned long long newQuota))decisionHandler
{
    decisionHandler(std::max<unsigned long long>(expectedUsage, currentUsage * 1.25));
}

- (NSMenu *)_webView:(WKWebView *)webView contextMenu:(NSMenu *)menu forElement:(_WKContextMenuElementInfo *)element
{
    for (NSInteger i = menu.numberOfItems - 1; i >= 0; --i) {
        NSMenuItem *item = [menu itemAtIndex:i];
        switch (item.tag) {
        case kWKContextMenuItemTagOpenLinkInNewWindow:
        case kWKContextMenuItemTagOpenImageInNewWindow:
        case kWKContextMenuItemTagOpenFrameInNewWindow:
        case kWKContextMenuItemTagOpenMediaInNewWindow:
        case kWKContextMenuItemTagCopyImageUrlToClipboard:
        case kWKContextMenuItemTagCopyImageToClipboard:
        case kWKContextMenuItemTagDownloadLinkToDisk:
        case kWKContextMenuItemTagDownloadImageToDisk:
            [menu removeItemAtIndex:i];
            break;
        }
    }

    return menu;
}

// MARK: WKNavigationDelegate methods

- (void)webViewWebContentProcessDidTerminate:(WKWebView *)webView
{
    if (!!_delegate && [_delegate respondsToSelector:@selector(inspectorViewControllerInspectorDidCrash:)])
        [_delegate inspectorViewControllerInspectorDidCrash:self];
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler
{
    // Allow non-main frames to navigate anywhere.
    if (!navigationAction.targetFrame.isMainFrame) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    // Allow loading of the main inspector file.
    if ([navigationAction.request.URL.scheme isEqualToString:WKInspectorResourceScheme]) {
        decisionHandler(WKNavigationActionPolicyAllow);
        return;
    }

    // Prevent everything else.
    decisionHandler(WKNavigationActionPolicyCancel);

    if (!!_delegate && [_delegate respondsToSelector:@selector(inspectorViewController:openURLExternally:)]) {
        [_delegate inspectorViewController:self openURLExternally:navigationAction.request.URL];
        return;
    }

    // Try to load the request in the inspected page if the delegate can't handle it.
    if (_inspectedPage)
        _inspectedPage->loadRequest(navigationAction.request);
}

// MARK: WKInspectorWKWebViewDelegate methods

- (void)inspectorWKWebViewDidBecomeActive:(WKInspectorWKWebView *)webView
{
    if ([_delegate respondsToSelector:@selector(inspectorViewControllerDidBecomeActive:)])
        [_delegate inspectorViewControllerDidBecomeActive:self];
}

- (void)inspectorWKWebViewReload:(WKInspectorWKWebView *)webView
{
    if (!_inspectedPage)
        return;

    OptionSet<WebCore::ReloadOption> reloadOptions;
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ExpiredOnlyReloadBehavior))
        reloadOptions.add(WebCore::ReloadOption::ExpiredOnly);

    _inspectedPage->reload(reloadOptions);
}

- (void)inspectorWKWebViewReloadFromOrigin:(WKInspectorWKWebView *)webView
{
    if (!_inspectedPage)
        return;

    _inspectedPage->reload(WebCore::ReloadOption::FromOrigin);
}

- (void)inspectorWKWebView:(WKInspectorWKWebView *)webView willMoveToWindow:(NSWindow *)newWindow
{
    if (!!_delegate && [_delegate respondsToSelector:@selector(inspectorViewController:willMoveToWindow:)])
        [_delegate inspectorViewController:self willMoveToWindow:newWindow];
}

- (void)inspectorWKWebViewDidMoveToWindow:(WKInspectorWKWebView *)webView
{
    if (!!_delegate && [_delegate respondsToSelector:@selector(inspectorViewControllerDidMoveToWindow:)])
        [_delegate inspectorViewControllerDidMoveToWindow:self];
}

@end

#endif // PLATFORM(MAC)
