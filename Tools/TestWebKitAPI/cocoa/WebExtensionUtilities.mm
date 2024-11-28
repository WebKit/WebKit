/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebExtensionTabConfiguration.h>
#import <WebKit/WKWebExtensionWindowConfiguration.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@interface TestWebExtensionManager () <WKWebExtensionControllerDelegatePrivate>
@end

@implementation TestWebExtensionManager {
    bool _done;
    NSMutableArray *_windows;
}

- (instancetype)initForExtension:(WKWebExtension *)extension
{
    return [self initForExtension:extension extensionControllerConfiguration:nil];
}

- (instancetype)initForExtension:(WKWebExtension *)extension extensionControllerConfiguration:(WKWebExtensionControllerConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    _yieldMessage = @"";
    _extension = extension;
    _context = [[WKWebExtensionContext alloc] initForExtension:extension];
    _controller = [[WKWebExtensionController alloc] initWithConfiguration:configuration ?: WKWebExtensionControllerConfiguration.nonPersistentConfiguration];

    // Grant all requested API permissions.
    for (WKWebExtensionPermission permission in _extension.requestedPermissions)
        [_context setPermissionStatus:WKWebExtensionContextPermissionStatusGrantedExplicitly forPermission:permission];

    _controller._testingMode = YES;

    // This should always be self. If you need the delegate, use the controllerDelegate property.
    // Delegate method calls will be forwarded to the controllerDelegate.
    _controller.delegate = self;

    _internalDelegate = [[TestWebExtensionsDelegate alloc] init];

    auto *window = [[TestWebExtensionWindow alloc] initWithExtensionController:_controller usesPrivateBrowsing:NO];
    auto *windows = [NSMutableArray arrayWithObject:window];

    _internalDelegate.openWindows = ^NSArray<id<WKWebExtensionWindow>> *(WKWebExtensionContext *) {
        return [windows copy];
    };

    _internalDelegate.focusedWindow = ^id<WKWebExtensionWindow>(WKWebExtensionContext *) {
        return window;
    };

#if PLATFORM(MAC)
    __weak TestWebExtensionManager *weakSelf = self;

    _internalDelegate.openNewWindow = ^(WKWebExtensionWindowConfiguration *configuration, WKWebExtensionContext *, void (^completionHandler)(id<WKWebExtensionWindow>, NSError *)) {
        auto *newWindow = [weakSelf openNewWindowUsingPrivateBrowsing:configuration.shouldBePrivate];

        newWindow.windowType = configuration.windowType;
        newWindow.windowState = configuration.windowState;

        CGRect currentFrame = newWindow.frame;
        CGRect desiredFrame = configuration.frame;

        if (std::isnan(desiredFrame.size.width))
            desiredFrame.size.width = currentFrame.size.width;

        if (std::isnan(desiredFrame.size.height))
            desiredFrame.size.height = currentFrame.size.height;

        if (std::isnan(desiredFrame.origin.x))
            desiredFrame.origin.x = currentFrame.origin.x;

        CGRect screenFrame = newWindow.screenFrame;
        CGFloat screenTop = screenFrame.size.height + screenFrame.origin.y;
        if (std::isnan(desiredFrame.origin.y)) {
            // Calculate the current top to keep the top-left corner of the window at the same position if the height changed.
            CGFloat currentTop = screenTop - currentFrame.size.height - currentFrame.origin.y;
            desiredFrame.origin.y = screenTop - desiredFrame.size.height - currentTop;
        }

        newWindow.frame = desiredFrame;

        completionHandler(newWindow, nil);
    };
#endif // PLATFORM(MAC)

    _internalDelegate.openNewTab = ^(WKWebExtensionTabConfiguration *configuration, WKWebExtensionContext *context, void (^completionHandler)(id<WKWebExtensionTab>, NSError *)) {
        auto *desiredWindow = dynamic_objc_cast<TestWebExtensionWindow>(configuration.window) ?: window;
        auto *newTab = [desiredWindow openNewTabAtIndex:configuration.index];

        if (configuration.url) {
            [newTab changeWebViewIfNeededForURL:configuration.url forExtensionContext:context];
            [newTab.webView loadRequest:[NSURLRequest requestWithURL:configuration.url]];
        }

        newTab.parentTab = configuration.parentTab;
        newTab.pinned = configuration.shouldBePinned;
        newTab.muted = configuration.shouldBeMuted;
        newTab.showingReaderMode = configuration.shouldReaderModeBeActive;
        newTab.selected = configuration.shouldAddToSelection;

        if (configuration.shouldBeActive)
            desiredWindow.activeTab = newTab;

        completionHandler(newTab, nil);
    };

    _windows = windows;
    _defaultWindow = window;
    _defaultTab = window.tabs.firstObject;
    _controllerDelegate = _internalDelegate;

    return self;
}

- (BOOL)respondsToSelector:(SEL)selector
{
    return [_controllerDelegate respondsToSelector:selector] || [super respondsToSelector:selector];
}

- (id)forwardingTargetForSelector:(SEL)selector
{
    return [_controllerDelegate respondsToSelector:selector] ? _controllerDelegate : [super forwardingTargetForSelector:selector];
}

- (TestWebExtensionWindow *)openNewWindow
{
    return [self openNewWindowUsingPrivateBrowsing:NO];
}

- (TestWebExtensionWindow *)openNewWindowUsingPrivateBrowsing:(BOOL)usesPrivateBrowsing
{
    auto *newWindow = [[TestWebExtensionWindow alloc] initWithExtensionController:_controller usesPrivateBrowsing:usesPrivateBrowsing];

    __weak TestWebExtensionManager *weakSelf = self;
    __weak TestWebExtensionWindow *weakWindow = newWindow;

    newWindow.didFocus = ^{
        [weakSelf focusWindow:weakWindow];
    };

    newWindow.didClose = ^{
        [weakSelf closeWindow:weakWindow];
    };

    [_windows addObject:newWindow];
    [_controller didOpenWindow:newWindow];

    return newWindow;
}

- (void)focusWindow:(TestWebExtensionWindow *)window
{
    if (window) {
        [_windows removeObject:window];
        [_windows insertObject:window atIndex:0];
    }

    [_controller didFocusWindow:window];
}

- (void)closeWindow:(TestWebExtensionWindow *)window
{
    for (TestWebExtensionTab *tab in window.tabs)
        [window closeTab:tab windowIsClosing:YES];

    [_windows removeObject:window];
    _defaultWindow = _windows.firstObject;

    [_controller didCloseWindow:window];
    [_controller didFocusWindow:_defaultWindow];
}

- (void)load
{
    NSError *error;
    EXPECT_TRUE([_controller loadExtensionContext:_context error:&error]);
    EXPECT_NULL(error);
}

- (void)run
{
    _done = false;
    _yieldMessage = @"";

    TestWebKitAPI::Util::run(&_done);
}

- (void)runForTimeInterval:(NSTimeInterval)interval
{
    _done = false;
    _yieldMessage = @"";

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(interval * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        self->_done = true;
    });

    TestWebKitAPI::Util::run(&_done);
}

- (void)loadAndRun
{
    [self load];
    [self run];
}

- (void)done
{
    _done = true;
}

- (void)_webExtensionController:(WKWebExtensionController *)controller recordTestAssertionResult:(BOOL)result withMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber
{
    if (result)
        return;

    if (!message.length)
        message = @"Assertion failed with no message.";

    ::testing::internal::AssertHelper(::testing::TestPartResult::kNonFatalFailure, sourceURL.UTF8String, lineNumber, message.UTF8String) = ::testing::Message();
}

- (void)_webExtensionController:(WKWebExtensionController *)controller recordTestEqualityResult:(BOOL)result expectedValue:(NSString *)expectedValue actualValue:(NSString *)actualValue withMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber
{
    if (result)
        return;

    if (!message.length)
        message = @"Expected equality of these values";

    ::testing::internal::AssertHelper(::testing::TestPartResult::kNonFatalFailure, sourceURL.UTF8String, lineNumber, "") = ::testing::Message()
        << message.UTF8String << ":\n"
        << "  Actual: " << actualValue.UTF8String << "\n"
        << "Expected: " << expectedValue.UTF8String;
}

- (void)_webExtensionController:(WKWebExtensionController *)controller recordTestMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber
{
    printf("\n%s:%u\n%s\n\n", sourceURL.UTF8String, lineNumber, message.UTF8String);
}

- (void)_webExtensionController:(WKWebExtensionController *)controller recordTestYieldedWithMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber
{
    _done = true;
    _yieldMessage = [message copy] ?: @"";
}

- (void)_webExtensionController:(WKWebExtensionController *)controller recordTestFinishedWithResult:(BOOL)result message:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber
{
    _done = true;

    if (result)
        return;

    if (!message.length)
        message = @"Test failed with no message.";

    ::testing::internal::AssertHelper(::testing::TestPartResult::kNonFatalFailure, sourceURL.UTF8String, lineNumber, message.UTF8String) = ::testing::Message();
}

@end

static WKUserContentController *userContentController(BOOL usingPrivateBrowsing)
{
    static WKUserContentController *privateController = [[WKUserContentController alloc] init];
    static WKUserContentController *normalController = [[WKUserContentController alloc] init];
    return usingPrivateBrowsing ? privateController : normalController;
}

@interface TestWebExtensionTab () <WKNavigationDelegate>
@end

@implementation TestWebExtensionTab {
    __weak WKWebExtensionController *_extensionController;
}

- (instancetype)init
{
    return [self initWithWindow:nil extensionController:nil];
}

- (instancetype)initWithWindow:(TestWebExtensionWindow *)window extensionController:(WKWebExtensionController *)extensionController
{
    if (!(self = [super init]))
        return nil;

    _window = window;

    if (extensionController) {
        BOOL usingPrivateBrowsing = _window.usingPrivateBrowsing;

        WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
        configuration.webExtensionController = extensionController;
        configuration.websiteDataStore = usingPrivateBrowsing ? WKWebsiteDataStore.nonPersistentDataStore : WKWebsiteDataStore.defaultDataStore;
        configuration.userContentController = userContentController(usingPrivateBrowsing);
        configuration.preferences._developerExtrasEnabled = YES;

        _webView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
        _webView.navigationDelegate = self;

        _extensionController = extensionController;
    }

    return self;
}

- (void)assignWindow:(TestWebExtensionWindow *)window
{
    _window = window;
}

- (void)setWindow:(TestWebExtensionWindow *)newWindow
{
    auto *oldWindow = _window;

    _window = newWindow;

    NSUInteger oldIndex = oldWindow ? [oldWindow removeTab:self] : NSNotFound;
    if (newWindow)
        [newWindow insertTab:self atIndex:newWindow.tabs.count];

    [_extensionController didMoveTab:self fromIndex:oldIndex inWindow:oldWindow];
}

- (id<WKWebExtensionWindow>)windowForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _window;
}

- (NSUInteger)indexInWindowForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _window ? [_window.tabs indexOfObject:self] : NSNotFound;
}

- (void)changeWebViewIfNeededForURL:(NSURL *)url forExtensionContext:(WKWebExtensionContext *)context
{
    BOOL usingPrivateBrowsing = _window.usingPrivateBrowsing;

    if ([_webView.URL.scheme isEqualToString:url.scheme])
        return;

    WKWebViewConfiguration *configuration = [url.scheme hasPrefix:@"http"] ? [[WKWebViewConfiguration alloc] init] : context.webViewConfiguration;
    configuration.webExtensionController = _extensionController;
    configuration.websiteDataStore = usingPrivateBrowsing ? WKWebsiteDataStore.nonPersistentDataStore : WKWebsiteDataStore.defaultDataStore;
    configuration.userContentController = userContentController(usingPrivateBrowsing);

    _webView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
    _webView.navigationDelegate = self;
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    [_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (void)_webView:(WKWebView *)webView navigationDidFinishDocumentLoad:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesTitle | WKWebExtensionTabChangedPropertiesURL forTab:self];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (WKWebView *)webViewForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _webView;
}

- (BOOL)isReaderModeActiveForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _showingReaderMode;
}

- (void)setReaderModeActive:(BOOL)showing forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_setReaderModeShowing)
            self->_setReaderModeShowing(showing);

        self->_showingReaderMode = showing;

        [self->_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesReaderMode forTab:self];

        completionHandler(nil);
    });
}

- (void)detectWebpageLocaleForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSLocale *, NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_webpageLocale)
            completionHandler(self->_webpageLocale(), nil);
        else
            completionHandler(nil, nil);
    });
}

- (void)reloadFromOrigin:(BOOL)fromOrigin forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_reload)
            self->_reload(fromOrigin);
        else if (fromOrigin)
            [self->_webView reloadFromOrigin];
        else
            [self->_webView reload];

        completionHandler(nil);
    });
}

- (void)goBackForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_goBack)
            self->_goBack();
        else
            [self->_webView goBack];

        completionHandler(nil);
    });
}

- (void)goForwardForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_goForward)
            self->_goForward();
        else
            [self->_webView goForward];

        completionHandler(nil);
    });
}

- (id<WKWebExtensionTab>)parentTabForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _parentTab;
}

- (void)setParentTab:(id<WKWebExtensionTab>)parentTab forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_parentTab = dynamic_objc_cast<TestWebExtensionTab>(parentTab);

        completionHandler(nil);
    });
}

- (BOOL)isPinnedForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _pinned;
}

- (void)setPinned:(BOOL)pinned forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_pinned = pinned;

        [self->_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesPinned forTab:self];

        completionHandler(nil);
    });
}

- (BOOL)isMutedForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _muted;
}

- (void)setMuted:(BOOL)muted forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_muted = muted;

        [self->_extensionController didChangeTabProperties:WKWebExtensionTabChangedPropertiesMuted forTab:self];

        completionHandler(nil);
    });
}

- (void)duplicateUsingConfiguration:(WKWebExtensionTabConfiguration *)configuration forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(id<WKWebExtensionTab> duplicatedTab, NSError *error))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_duplicate)
            self->_duplicate(configuration, completionHandler);
        else
            completionHandler(nil, nil);
    });
}

- (void)activateForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        auto *previousActiveTab = self->_window.activeTab;
        self->_window.activeTab = self;

        self->_selected = YES;

        [self->_extensionController didActivateTab:self previousActiveTab:previousActiveTab];

        completionHandler(nil);
    });
}

- (BOOL)isSelectedForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _selected || _window.activeTab == self;
}

- (void)setSelected:(BOOL)selected forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_selected = selected;

        if (selected)
            [self->_extensionController didSelectTabs:@[ self ]];
        else
            [self->_extensionController didDeselectTabs:@[ self ]];

        completionHandler(nil);
    });
}

- (void)closeForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_window closeTab:self];

        completionHandler(nil);
    });
}

- (BOOL)shouldBypassPermissionsForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _shouldBypassPermissions;
}

@end

@implementation TestWebExtensionWindow {
    __weak WKWebExtensionController *_extensionController;
    CGRect _previousFrame;
    NSMutableArray<TestWebExtensionTab *> *_tabs;
}

- (instancetype)init
{
    return [self initWithExtensionController:nil usesPrivateBrowsing:NO];
}

- (instancetype)initWithExtensionController:(WKWebExtensionController *)extensionController usesPrivateBrowsing:(BOOL)usesPrivateBrowsing
{
    if (!(self = [super init]))
        return nil;

    _extensionController = extensionController;

    _windowState = WKWebExtensionWindowStateNormal;
    _windowType = WKWebExtensionWindowTypeNormal;
    _usingPrivateBrowsing = usesPrivateBrowsing;
    _screenFrame = CGRectMake(0, 0, 1920, 1080);

#if PLATFORM(MAC)
    // This is 50pt from top on the screen in Mac screen coordinates.
    _frame = CGRectMake(100, _screenFrame.size.height - 600 - 50, 800, 600);
#else
    _frame = CGRectMake(100, 50, 800, 600);
#endif

    _previousFrame = CGRectNull;

    _tabs = [NSMutableArray array];
    _activeTab = [self openNewTab];

    return self;
}

- (NSArray<TestWebExtensionTab *> *)tabs
{
    return [_tabs copy];
}

- (void)setTabs:(NSArray<TestWebExtensionTab *> *)tabs
{
    auto *previousActiveTab = _activeTab;

    for (TestWebExtensionTab *tab in _tabs) {
        [tab.webView _close];
        tab.webView = nil;

        [_extensionController didCloseTab:tab windowIsClosing:NO];

        [tab assignWindow:nil];
    }

    _tabs = [tabs mutableCopy];
    _activeTab = _tabs.firstObject;

    for (TestWebExtensionTab *tab in _tabs)
        [_extensionController didOpenTab:tab];

    if (_activeTab)
        [_extensionController didActivateTab:_activeTab previousActiveTab:previousActiveTab];
}

- (NSUInteger)removeTab:(TestWebExtensionTab *)tab
{
    NSUInteger oldIndex = [_tabs indexOfObject:tab];
    if (oldIndex == NSNotFound)
        return oldIndex;

    [_tabs removeObjectAtIndex:oldIndex];

    if (_activeTab == tab)
        _activeTab = _tabs.firstObject;

    return oldIndex;
}

- (void)insertTab:(TestWebExtensionTab *)tab atIndex:(NSUInteger)index
{
    ASSERT(index <= _tabs.count);

    [_tabs insertObject:tab atIndex:index];
}

- (TestWebExtensionTab *)openNewTab
{
    return [self openNewTabAtIndex:_tabs.count];
}

- (TestWebExtensionTab *)openNewTabAtIndex:(NSUInteger)index
{
    ASSERT(index <= _tabs.count);

    auto *newTab = [[TestWebExtensionTab alloc] initWithWindow:self extensionController:_extensionController];

    __weak TestWebExtensionTab *weakTab = newTab;

    newTab.duplicate = ^(WKWebExtensionTabConfiguration *configuration, void (^completionHandler)(TestWebExtensionTab *, NSError *)) {
        auto *desiredWindow = dynamic_objc_cast<TestWebExtensionWindow>(configuration.window) ?: weakTab.window;
        auto *duplicatedTab = [desiredWindow openNewTabAtIndex:configuration.index];

        [duplicatedTab.webView loadRequest:[NSURLRequest requestWithURL:weakTab.webView.URL]];

        duplicatedTab.selected = configuration.shouldAddToSelection;

        if (configuration.shouldBeActive)
            desiredWindow.activeTab = duplicatedTab;

        completionHandler(duplicatedTab, nil);
    };

    if (!_activeTab)
        _activeTab = newTab;

    [_tabs insertObject:newTab atIndex:index];
    [_extensionController didOpenTab:newTab];

    return newTab;
}

- (void)closeTab:(TestWebExtensionTab *)tab
{
    [self closeTab:tab windowIsClosing:NO];
}

- (void)closeTab:(TestWebExtensionTab *)tab windowIsClosing:(BOOL)windowIsClosing
{
    [tab.webView _close];
    tab.webView = nil;

    [_tabs removeObject:tab];

    if (tab == _activeTab) {
        _activeTab = _tabs.firstObject;

        if (_activeTab)
            [_extensionController didActivateTab:_activeTab previousActiveTab:tab];
    }

    [_extensionController didCloseTab:tab windowIsClosing:windowIsClosing];

    [tab assignWindow:nil];
}

- (void)replaceTab:(TestWebExtensionTab *)oldTab withTab:(TestWebExtensionTab *)newTab
{
    ASSERT([_tabs containsObject:oldTab]);
    ASSERT(![_tabs containsObject:newTab]);

    [oldTab.webView _close];
    oldTab.webView = nil;

    if (_activeTab == oldTab)
        _activeTab = newTab;

    [_tabs replaceObjectAtIndex:[_tabs indexOfObject:oldTab] withObject:newTab];
    [_extensionController didReplaceTab:oldTab withTab:newTab];

    [oldTab assignWindow:nil];
}

- (void)moveTab:(TestWebExtensionTab *)tab toIndex:(NSUInteger)newIndex
{
    if (tab.window != self) {
        TestWebExtensionWindow *oldWindow = tab.window;

        auto oldIndex = [oldWindow->_tabs indexOfObject:tab];
        ASSERT(oldIndex != NSNotFound);

        [oldWindow->_tabs removeObjectAtIndex:oldIndex];
        [_tabs insertObject:tab atIndex:newIndex];

        [tab assignWindow:self];

        [_extensionController didMoveTab:tab fromIndex:oldIndex inWindow:oldWindow];

        return;
    }

    auto oldIndex = [_tabs indexOfObject:tab];
    ASSERT(oldIndex != NSNotFound);

    [_tabs removeObjectAtIndex:oldIndex];
    [_tabs insertObject:tab atIndex:newIndex];

    [_extensionController didMoveTab:tab fromIndex:oldIndex inWindow:self];
}

- (NSArray<id<WKWebExtensionTab>> *)tabsForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _tabs;
}

- (id<WKWebExtensionTab>)activeTabForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _activeTab;
}

- (WKWebExtensionWindowType)windowTypeForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _windowType;
}

- (WKWebExtensionWindowState)windowStateForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _windowState;
}

- (void)setWindowState:(WKWebExtensionWindowState)state forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_windowState = state;

        if (state == WKWebExtensionWindowStateFullscreen) {
            self->_previousFrame = self->_frame;
            self->_frame = self->_screenFrame;
        } else if (!CGRectIsEmpty(self->_previousFrame)) {
            self->_frame = self->_previousFrame;
            self->_previousFrame = CGRectNull;
        }

        completionHandler(nil);
    });
}

- (BOOL)isPrivateForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _usingPrivateBrowsing;
}

- (CGRect)screenFrameForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _screenFrame;
}

- (CGRect)frameForWebExtensionContext:(WKWebExtensionContext *)context
{
    return _frame;
}

- (void)setFrame:(CGRect)frame forWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_frame = frame;

        completionHandler(nil);
    });
}

- (void)focusForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    if (_didFocus)
        _didFocus();

    completionHandler(nil);
}

- (void)closeForWebExtensionContext:(WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (self->_didClose)
            self->_didClose();

        completionHandler(nil);
    });
}

@end

namespace TestWebKitAPI {
namespace Util {

RetainPtr<TestWebExtensionManager> loadAndRunExtension(WKWebExtension *extension, WKWebExtensionControllerConfiguration *configuration)
{
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension extensionControllerConfiguration:configuration]);
    [manager loadAndRun];
    return manager;
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *manifest, NSDictionary *resources, WKWebExtensionControllerConfiguration *configuration)
{
    return loadAndRunExtension([[WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources], configuration);
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *resources, WKWebExtensionControllerConfiguration *configuration)
{
    return loadAndRunExtension([[WKWebExtension alloc] _initWithResources:resources], configuration);
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSURL *baseURL, WKWebExtensionControllerConfiguration *configuration)
{
    return loadAndRunExtension([[WKWebExtension alloc] initWithResourceBaseURL:baseURL error:nullptr], configuration);
}

NSData *makePNGData(CGSize size, SEL colorSelector)
{
#if USE(APPKIT)
    auto image = adoptNS([[NSImage alloc] initWithSize:size]);

    [image lockFocus];

    [[NSColor performSelector:colorSelector] setFill];
    NSRectFill(NSMakeRect(0, 0, size.width, size.height));

    [image unlockFocus];

    auto cgImageRef = [image CGImageForProposedRect:NULL context:nil hints:nil];
    auto newImageRep = adoptNS([[NSBitmapImageRep alloc] initWithCGImage:cgImageRef]);
    newImageRep.get().size = size;

    return [newImageRep representationUsingType:NSBitmapImageFileTypePNG properties:@{ }];
#else
    UIGraphicsBeginImageContextWithOptions(size, NO, 1.0);

    [[UIColor performSelector:colorSelector] setFill];
    UIRectFill(CGRectMake(0, 0, size.width, size.height));

    auto *image = UIGraphicsGetImageFromCurrentImageContext();

    UIGraphicsEndImageContext();

    return UIImagePNGRepresentation(image);
#endif
}

void runScriptWithUserGesture(const String& script, WKWebView *webView)
{
    ASSERT(webView);

    bool callbackComplete = false;
    id evalResult;

    [webView callAsyncJavaScript:script arguments:nil inFrame:nil inContentWorld:WKContentWorld.pageWorld completionHandler:[&](id result, NSError *error) {
        evalResult = result;
        callbackComplete = true;

        EXPECT_NULL(error);

        if (error)
            NSLog(@"Encountered error: %@ while evaluating script: %@", error, static_cast<NSString *>(script));
    }];

    TestWebKitAPI::Util::run(&callbackComplete);
}

void performWithAppearance(Appearance appearance, void (^block)(void))
{
#if USE(APPKIT)
    auto *appearanceName = appearance == Appearance::Dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua;
    [[NSAppearance appearanceNamed:appearanceName] performAsCurrentDrawingAppearance:block];
#else
    auto *traitCollection = appearance == Appearance::Dark ? [UITraitCollection traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleDark]
        : [UITraitCollection traitCollectionWithUserInterfaceStyle:UIUserInterfaceStyleLight];
    [traitCollection performAsCurrentTraitCollection:block];
#endif
}

CocoaColor *pixelColor(CocoaImage *image, CGPoint point)
{
#if USE(APPKIT)
    auto imageRef = [image CGImageForProposedRect:nullptr context:nil hints:nil];
    auto *bitmap = [[NSBitmapImageRep alloc] initWithCGImage:imageRef];
    auto *color = [bitmap colorAtX:point.x y:point.y];
    return color;
#else
    image = [image.imageAsset imageWithTraitCollection:UITraitCollection.currentTraitCollection];

    UIGraphicsBeginImageContext(image.size);

    [image drawAtPoint:CGPointZero];

    auto context = UIGraphicsGetCurrentContext();
    auto *data = (unsigned char *)CGBitmapContextGetData(context);
    if (!data)
        return nil;

    unsigned offset = ((image.size.width * point.y) + point.x) * 4;
    auto *color = [UIColor colorWithRed:data[offset] / 255.0 green:data[offset + 1] / 255.0 blue:data[offset + 2] / 255.0 alpha:data[offset + 3] / 255.0];

    UIGraphicsEndImageContext();

    return color;
#endif
}

CocoaColor *toSRGBColor(CocoaColor *color)
{
#if USE(APPKIT)
    return [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
#else
    return color;
#endif
}

bool compareColors(CocoaColor *color1, CocoaColor *color2)
{
    if (color1 == color2 || [color1 isEqual:color2])
        return true;

    if (!color1 || !color2)
        return false;

    color1 = toSRGBColor(color1);
    color2 = toSRGBColor(color2);

    CGFloat red1, green1, blue1, alpha1;
    [color1 getRed:&red1 green:&green1 blue:&blue1 alpha:&alpha1];

    CGFloat red2, green2, blue2, alpha2;
    [color2 getRed:&red2 green:&green2 blue:&blue2 alpha:&alpha2];

    return fabs(red1 - red2) < 0.01 && fabs(green1 - green2) < 0.01 && fabs(blue1 - blue2) < 0.01 && fabs(alpha1 - alpha2) < 0.01;
}

} // namespace Util
} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
