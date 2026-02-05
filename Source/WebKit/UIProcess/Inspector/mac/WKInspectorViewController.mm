/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
#import "AppKitSPI.h"
#import "WKContextMenuItemTypes.h"
#import "WKInspectorResourceURLSchemeHandler.h"
#import "WKInspectorWKWebView.h"
#import "WKOpenPanelParameters.h"
#import "WKProcessPoolInternal.h"
#import "WKWebViewInternal.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebInspectorUIProxy.h"
#import "WebInspectorUtilities.h"
#import "WebPageProxy.h"
#import "WebsiteDataStore.h"
#import "_WKInspectorConfigurationInternal.h"
#import <WebKit/WKFrameInfo.h>
#import <WebKit/WKNavigationAction.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
#import "WKWebExtensionController.h"
#import "WebExtensionController.h"
#endif

static NSString * const WKInspectorResourceScheme = @"inspector-resource";

static NSString * const safeAreaInsetsKVOKey = @"safeAreaInsets";
static void* const safeAreaInsetsKVOContext = (void*)&safeAreaInsetsKVOContext;

@interface WKInspectorViewController () <WKUIDelegate, WKNavigationDelegate, WKInspectorWKWebViewDelegate>
@end

@implementation WKInspectorViewController {
    WeakPtr<WebKit::WebPageProxy> _inspectedPage;
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
    _inspectedPage = inspectedPage.get();

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
        [_webView setInspectable:YES];
        [_webView setUIDelegate:self];
        [_webView setNavigationDelegate:self];
        [_webView setInspectorWKWebViewDelegate:self];
        [_webView _setAutomaticallyAdjustsContentInsets:NO];
        [_webView _setUseSystemAppearance:YES];
        [_webView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

        [_webView _setObscuredContentInsets:_webView.get().safeAreaInsets immediate:NO];
        [_webView addObserver:self forKeyPath:safeAreaInsetsKVOKey options:0 context:safeAreaInsetsKVOContext];
    }

    return _webView.get();
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void*)context
{
    if (context == safeAreaInsetsKVOContext)
        [_webView _setObscuredContentInsets:_webView.get().safeAreaInsets immediate:NO];
    else
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
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
        [allowedURLSchemes addObject:pair.second.createNSString().get()];

    [inspectorSchemeHandler setAllowedURLSchemesForCSP:allowedURLSchemes.get()];
    [configuration setURLSchemeHandler:inspectorSchemeHandler.get() forURLScheme:WKInspectorResourceScheme];

    RefPtr inspectedPage = _inspectedPage.get();
#if ENABLE(WK_WEB_EXTENSIONS) && ENABLE(INSPECTOR_EXTENSIONS)
    if (inspectedPage) {
        if (RefPtr webExtensionController = inspectedPage->webExtensionController())
            configuration.get().webExtensionController = protect(webExtensionController->wrapper()).get();
    }
#endif

    RetainPtr<WKPreferences> preferences = configuration.get().preferences;
    preferences.get()._allowFileAccessFromFileURLs = YES;
    [configuration _setAllowUniversalAccessFromFileURLs:YES];
    [configuration _setAllowTopNavigationToDataURLs:YES];
    preferences.get()._storageBlockingPolicy = _WKStorageBlockingPolicyAllowAll;
    preferences.get()._javaScriptRuntimeFlags = 0;

#ifndef NDEBUG
    // Allow developers to inspect the Web Inspector in debug builds without changing settings.
    preferences.get()._developerExtrasEnabled = YES;
    preferences.get()._logsPageMessagesToSystemConsoleEnabled = YES;
#endif

    preferences.get()._diagnosticLoggingEnabled = YES;

    // Disable Site Isolation for Web Inspector View.
    preferences.get()._siteIsolationEnabled = NO;

    [_configuration applyToWebViewConfiguration:configuration.get()];

    RetainPtr delegate = _delegate.get();
    if (!!delegate && [delegate respondsToSelector:@selector(inspectorViewControllerInspectorIsUnderTest:)]) {
        if ([delegate inspectorViewControllerInspectorIsUnderTest:self]) {
            preferences.get()._hiddenPageDOMTimerThrottlingEnabled = NO;
            preferences.get()._pageVisibilityBasedProcessSuppressionEnabled = NO;
            preferences.get().inactiveSchedulingPolicy = WKInactiveSchedulingPolicyNone;
        }
    }

    // WKInspectorConfiguration allows the client to specify a process pool to use.
    // If not specified or the inspection level is >1, use the default strategy.
    // This ensures that Inspector^2 cannot be affected by client (mis)configuration.
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<WKProcessPool> customProcessPool = configuration.get().processPool;
    ALLOW_DEPRECATED_DECLARATIONS_END
    auto inspectorLevel = WebKit::inspectorLevelForPage(inspectedPage.get());
    auto useDefaultProcessPool = inspectorLevel > 1 || !customProcessPool;
    if (customProcessPool && !useDefaultProcessPool)
        WebKit::prepareProcessPoolForInspector(Ref { *customProcessPool->_processPool.get() });

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (useDefaultProcessPool)
        [configuration setProcessPool:protectedWrapper(Ref { WebKit::defaultInspectorProcessPool(inspectorLevel) }.get()).get()];
    ALLOW_DEPRECATED_DECLARATIONS_END

    // Ensure that a page group identifier is set. This is for computing inspection levels.
    if (!configuration.get()._groupIdentifier)
        [configuration _setGroupIdentifier:WebKit::defaultInspectorPageGroupIdentifierForPage(inspectedPage.get()).createNSString().get()];

    // Prefer using a custom persistent data store if one exists.
    RetainPtr<WKWebsiteDataStore> targetDataStore;
    WebKit::WebsiteDataStore::forEachWebsiteDataStore([&targetDataStore](WebKit::WebsiteDataStore& dataStore) {
        if (dataStore.sessionID() != PAL::SessionID::defaultSessionID() && dataStore.resolvedDirectories().resourceLoadStatisticsDirectory == WebKit::WebsiteDataStore::defaultResourceLoadStatisticsDirectory()) {
            ASSERT(!targetDataStore);
            targetDataStore = WebKit::wrapper(dataStore);
        }
    });
    if (targetDataStore)
        [configuration setWebsiteDataStore:targetDataStore.get()];

    return configuration.autorelease();
}

+ (BOOL)viewIsInspectorWebView:(NSView *)view
{
    return [view isKindOfClass:[WKInspectorWKWebView class]];
}

+ (NSURL *)URLForInspectorResource:(NSString *)resource
{
    return [NSURL URLWithString:adoptNS([[NSString alloc] initWithFormat:@"%@:///%@", WKInspectorResourceScheme, resource]).get()].URLByStandardizingPath;
}

- (void)didAttachOrDetach
{
#if ENABLE(CONTENT_INSET_BACKGROUND_FILL)
    RetainPtr attachedView = [self _horizontallyAttachedInspectedWebView];
    [_webView _setOverrideTopScrollEdgeEffectColor:[attachedView _topScrollPocket].captureColor];

    if (attachedView)
        [_webView _addReasonToPreferSolidColorHardPocket:WebKit::PreferSolidColorHardPocketReason::AttachedInspector];
    else
        [_webView _removeReasonToPreferSolidColorHardPocket:WebKit::PreferSolidColorHardPocketReason::AttachedInspector];

    [_webView _setOverflowHeightForTopScrollEdgeEffect:[attachedView _overflowHeightForTopScrollEdgeEffect]];
    [_webView _updateHiddenScrollPocketEdges];
#endif // ENABLE(CONTENT_INSET_BACKGROUND_FILL)
}

- (WKWebView *)_horizontallyAttachedInspectedWebView
{
    if (![_delegate.get() inspectorViewControllerInspectorIsHorizontallyAttached:self])
        return nil;

    if (RefPtr inspectedPage = _inspectedPage.get())
        return inspectedPage->cocoaView().autorelease();

    return nil;
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
    openPanel.canChooseDirectories = parameters.allowsDirectories;

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
        RetainPtr<NSMenuItem> item = [menu itemAtIndex:i];
        switch (item.get().tag) {
        case kWKContextMenuItemTagOpenLinkInNewWindow:
        case kWKContextMenuItemTagOpenImageInNewWindow:
        case kWKContextMenuItemTagOpenFrameInNewWindow:
        case kWKContextMenuItemTagOpenMediaInNewWindow:
        case kWKContextMenuItemTagCopyImageURLToClipboard:
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
    [_webView removeObserver:self forKeyPath:safeAreaInsetsKVOKey];

    RetainPtr delegate = _delegate.get();
    if (!!delegate && [delegate respondsToSelector:@selector(inspectorViewControllerInspectorDidCrash:)])
        [delegate inspectorViewControllerInspectorDidCrash:self];
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

    RetainPtr delegate = _delegate.get();
    if (delegate && [delegate respondsToSelector:@selector(inspectorViewController:openURLExternally:)]) {
        [delegate inspectorViewController:self openURLExternally:navigationAction.request.URL];
        return;
    }

    // Try to load the request in the inspected page if the delegate can't handle it.
    if (RefPtr page = _inspectedPage.get())
        page->loadRequest(navigationAction.request);
}

// MARK: WKInspectorWKWebViewDelegate methods

- (void)inspectorWKWebViewDidBecomeActive:(WKInspectorWKWebView *)webView
{
    RetainPtr delegate = _delegate.get();
    if ([delegate respondsToSelector:@selector(inspectorViewControllerDidBecomeActive:)])
        [delegate inspectorViewControllerDidBecomeActive:self];
}

- (void)inspectorWKWebViewReload:(WKInspectorWKWebView *)webView
{
    RefPtr page = _inspectedPage.get();
    if (!page)
        return;

    OptionSet<WebCore::ReloadOption> reloadOptions;
    if (linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ExpiredOnlyReloadBehavior))
        reloadOptions.add(WebCore::ReloadOption::ExpiredOnly);

    page->reload(reloadOptions);
}

- (void)inspectorWKWebViewReloadFromOrigin:(WKInspectorWKWebView *)webView
{
    RefPtr page = _inspectedPage.get();
    if (!page)
        return;

    page->reload(WebCore::ReloadOption::FromOrigin);
}

- (void)inspectorWKWebView:(WKInspectorWKWebView *)webView willMoveToWindow:(NSWindow *)newWindow
{
    RetainPtr delegate = _delegate.get();
    if (delegate && [delegate respondsToSelector:@selector(inspectorViewController:willMoveToWindow:)])
        [delegate inspectorViewController:self willMoveToWindow:newWindow];
}

- (void)inspectorWKWebViewDidMoveToWindow:(WKInspectorWKWebView *)webView
{
    RetainPtr delegate = _delegate.get();
    if (!!delegate && [delegate respondsToSelector:@selector(inspectorViewControllerDidMoveToWindow:)])
        [delegate inspectorViewControllerDidMoveToWindow:self];
}

@end

#endif // PLATFORM(MAC)
