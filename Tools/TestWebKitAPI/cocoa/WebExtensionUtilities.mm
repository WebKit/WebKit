/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKWebExtensionTabCreationOptions.h>
#import <WebKit/_WKWebExtensionWindowCreationOptions.h>

@interface TestWebExtensionManager () <_WKWebExtensionControllerDelegatePrivate>
@end

@implementation TestWebExtensionManager {
    bool _done;
    NSMutableArray *_windows;
}

- (instancetype)initForExtension:(_WKWebExtension *)extension
{
    return [self initForExtension:extension extensionControllerConfiguration:nil];
}

- (instancetype)initForExtension:(_WKWebExtension *)extension extensionControllerConfiguration:(_WKWebExtensionControllerConfiguration *)configuration
{
    if (!(self = [super init]))
        return nil;

    _yieldMessage = @"";
    _extension = extension;
    _context = [[_WKWebExtensionContext alloc] initForExtension:extension];
    _controller = [[_WKWebExtensionController alloc] initWithConfiguration:configuration ?: _WKWebExtensionControllerConfiguration.nonPersistentConfiguration];

    _context._testingMode = YES;

    // This should always be self. If you need the delegate, use the controllerDelegate property.
    // Delegate method calls will be forwarded to the controllerDelegate.
    _controller.delegate = self;

    _internalDelegate = [[TestWebExtensionsDelegate alloc] init];

    auto *window = [[TestWebExtensionWindow alloc] initWithExtensionController:_controller usesPrivateBrowsing:NO];
    auto *windows = [NSMutableArray arrayWithObject:window];

    __weak TestWebExtensionManager *weakSelf = self;

    _internalDelegate.openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *) {
        return [windows copy];
    };

    _internalDelegate.focusedWindow = ^id<_WKWebExtensionWindow>(_WKWebExtensionContext *) {
        return window;
    };

    _internalDelegate.openNewWindow = ^(_WKWebExtensionWindowCreationOptions *options, _WKWebExtensionContext *, void (^completionHandler)(id<_WKWebExtensionWindow>, NSError *)) {
        auto *newWindow = [weakSelf openNewWindowUsingPrivateBrowsing:options.shouldUsePrivateBrowsing];

        newWindow.windowType = options.desiredWindowType;
        newWindow.windowState = options.desiredWindowState;

        CGRect currentFrame = newWindow.frame;
        CGRect desiredFrame = options.desiredFrame;

        if (std::isnan(desiredFrame.size.width))
            desiredFrame.size.width = currentFrame.size.width;

        if (std::isnan(desiredFrame.size.height))
            desiredFrame.size.height = currentFrame.size.height;

        if (std::isnan(desiredFrame.origin.x))
            desiredFrame.origin.x = currentFrame.origin.x;

#if PLATFORM(MAC)
        CGRect screenFrame = newWindow.screenFrame;
        CGFloat screenTop = screenFrame.size.height + screenFrame.origin.y;
        if (std::isnan(desiredFrame.origin.y)) {
            // Calculate the current top to keep the top-left corner of the window at the same position if the height changed.
            CGFloat currentTop = screenTop - currentFrame.size.height - currentFrame.origin.y;
            desiredFrame.origin.y = screenTop - desiredFrame.size.height - currentTop;
        }
#else
        if (std::isnan(desiredFrame.origin.y))
            desiredFrame.origin.y = currentFrame.origin.y;
#endif

        newWindow.frame = desiredFrame;

        completionHandler(newWindow, nil);
    };

    _internalDelegate.openNewTab = ^(_WKWebExtensionTabCreationOptions *options, _WKWebExtensionContext *context, void (^completionHandler)(id<_WKWebExtensionTab>, NSError *)) {
        auto *desiredWindow = dynamic_objc_cast<TestWebExtensionWindow>(options.desiredWindow) ?: window;
        auto *newTab = [desiredWindow openNewTabAtIndex:options.desiredIndex];

        if (options.desiredURL) {
            [newTab changeWebViewIfNeededForURL:options.desiredURL forExtensionContext:context];
            [newTab.mainWebView loadRequest:[NSURLRequest requestWithURL:options.desiredURL]];
        }

        newTab.parentTab = options.desiredParentTab;
        newTab.pinned = options.shouldPin;
        newTab.muted = options.shouldMute;
        newTab.showingReaderMode = options.shouldShowReaderMode;
        newTab.selected = options.shouldSelect;

        if (options.shouldActivate)
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

- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestAssertionResult:(BOOL)result withMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context
{
    if (result)
        return;

    if (!message.length)
        message = @"Assertion failed with no message.";

    ::testing::internal::AssertHelper(::testing::TestPartResult::kNonFatalFailure, sourceURL.UTF8String, lineNumber, message.UTF8String) = ::testing::Message();
}

- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestEqualityResult:(BOOL)result expectedValue:(NSString *)expectedValue actualValue:(NSString *)actualValue withMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context
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

- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context
{
    printf("\n%s:%u\n%s\n\n", sourceURL.UTF8String, lineNumber, message.UTF8String);
}

- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestYieldedWithMessage:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context
{
    _done = true;
    _yieldMessage = [message copy] ?: @"";
}

- (void)_webExtensionController:(_WKWebExtensionController *)controller recordTestFinishedWithResult:(BOOL)result message:(NSString *)message andSourceURL:(NSString *)sourceURL lineNumber:(unsigned)lineNumber forExtensionContext:(_WKWebExtensionContext *)context
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
    __weak _WKWebExtensionController *_extensionController;
}

- (instancetype)init
{
    return [self initWithWindow:nil extensionController:nil];
}

- (instancetype)initWithWindow:(TestWebExtensionWindow *)window extensionController:(_WKWebExtensionController *)extensionController
{
    if (!(self = [super init]))
        return nil;

    _window = window;

    if (extensionController) {
        BOOL usingPrivateBrowsing = _window.usingPrivateBrowsing;

        WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
        configuration._webExtensionController = extensionController;
        configuration.websiteDataStore = usingPrivateBrowsing ? WKWebsiteDataStore.nonPersistentDataStore : WKWebsiteDataStore.defaultDataStore;
        configuration.userContentController = userContentController(usingPrivateBrowsing);

        _mainWebView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
        _mainWebView.navigationDelegate = self;

        _extensionController = extensionController;
    }

    return self;
}

- (id<_WKWebExtensionWindow>)windowForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _window;
}

- (void)changeWebViewIfNeededForURL:(NSURL *)url forExtensionContext:(_WKWebExtensionContext *)context
{
    BOOL usingPrivateBrowsing = _window.usingPrivateBrowsing;

    if ([_mainWebView.URL.scheme isEqualToString:url.scheme])
        return;

    WKWebViewConfiguration *configuration = [url.scheme hasPrefix:@"http"] ? [[WKWebViewConfiguration alloc] init] : context.webViewConfiguration;
    configuration._webExtensionController = _extensionController;
    configuration.websiteDataStore = usingPrivateBrowsing ? WKWebsiteDataStore.nonPersistentDataStore : WKWebsiteDataStore.defaultDataStore;
    configuration.userContentController = userContentController(usingPrivateBrowsing);

    _mainWebView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
    _mainWebView.navigationDelegate = self;
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error
{
    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesTitle | _WKWebExtensionTabChangedPropertiesURL forTab:self];
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation
{
    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesLoading forTab:self];
}

- (WKWebView *)mainWebViewForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _mainWebView;
}

- (BOOL)isShowingReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _showingReaderMode;
}

- (void)toggleReaderModeForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_toggleReaderMode)
        _toggleReaderMode();

    _showingReaderMode = !_showingReaderMode;

    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesReaderMode forTab:self];

    completionHandler(nil);
}

- (void)detectWebpageLocaleForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSLocale *, NSError *))completionHandler
{
    if (_detectWebpageLocale)
        completionHandler(_detectWebpageLocale(), nil);
    else
        completionHandler(nil, nil);
}

- (void)reloadForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_reload)
        _reload();
    else
        [_mainWebView reload];

    completionHandler(nil);
}

- (void)reloadFromOriginForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_reloadFromOrigin)
        _reloadFromOrigin();
    else
        [_mainWebView reloadFromOrigin];

    completionHandler(nil);
}

- (void)goBackForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_goBack)
        _goBack();
    else
        [_mainWebView goBack];

    completionHandler(nil);
}

- (void)goForwardForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_goForward)
        _goForward();
    else
        [_mainWebView goForward];

    completionHandler(nil);
}

- (id<_WKWebExtensionTab>)parentTabForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _parentTab;
}

- (void)setParentTab:(id<_WKWebExtensionTab>)parentTab forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _parentTab = dynamic_objc_cast<TestWebExtensionTab>(parentTab);

    completionHandler(nil);
}

- (BOOL)isPinnedForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _pinned;
}

- (void)pinForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _pinned = YES;

    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesPinned forTab:self];

    completionHandler(nil);
}

- (void)unpinForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _pinned = NO;

    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesPinned forTab:self];

    completionHandler(nil);
}

- (BOOL)isMutedForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _muted;
}

- (void)muteForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _muted = YES;

    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesMuted forTab:self];

    completionHandler(nil);
}

- (void)unmuteForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _muted = NO;

    [_extensionController didChangeTabProperties:_WKWebExtensionTabChangedPropertiesMuted forTab:self];

    completionHandler(nil);
}

- (void)duplicateForWebExtensionContext:(_WKWebExtensionContext *)context withOptions:(_WKWebExtensionTabCreationOptions *)options completionHandler:(void (^)(id<_WKWebExtensionTab>, NSError *))completionHandler
{
    if (_duplicate)
        _duplicate(options, completionHandler);
    else
        completionHandler(nil, nil);
}

- (void)activateForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    auto *previousActiveTab = _window.activeTab;
    _window.activeTab = self;

    _selected = YES;

    [_extensionController didActivateTab:self previousActiveTab:previousActiveTab];

    completionHandler(nil);
}

- (BOOL)isSelectedForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _selected || _window.activeTab == self;
}

- (void)selectForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _selected = YES;

    [_extensionController didSelectTabs:[NSSet setWithObject:self]];

    completionHandler(nil);
}

- (void)deselectForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _selected = NO;

    [_extensionController didDeselectTabs:[NSSet setWithObject:self]];

    completionHandler(nil);
}

- (void)closeForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    [_window closeTab:self];

    completionHandler(nil);
}

@end

@implementation TestWebExtensionWindow {
    __weak _WKWebExtensionController *_extensionController;
    CGRect _previousFrame;
    NSMutableArray<TestWebExtensionTab *> *_tabs;
}

- (instancetype)init
{
    return [self initWithExtensionController:nil usesPrivateBrowsing:NO];
}

- (instancetype)initWithExtensionController:(_WKWebExtensionController *)extensionController usesPrivateBrowsing:(BOOL)usesPrivateBrowsing
{
    if (!(self = [super init]))
        return nil;

    _extensionController = extensionController;

    _windowState = _WKWebExtensionWindowStateNormal;
    _windowType = _WKWebExtensionWindowTypeNormal;
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
        [tab.mainWebView _close];
        tab.mainWebView = nil;

        [_extensionController didCloseTab:tab windowIsClosing:NO];
    }

    _tabs = [tabs mutableCopy];
    _activeTab = _tabs.firstObject;

    for (TestWebExtensionTab *tab in _tabs)
        [_extensionController didOpenTab:tab];

    if (_activeTab)
        [_extensionController didActivateTab:_activeTab previousActiveTab:previousActiveTab];
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

    newTab.duplicate = ^(_WKWebExtensionTabCreationOptions *options, void (^completionHandler)(TestWebExtensionTab *, NSError *)) {
        auto *desiredWindow = dynamic_objc_cast<TestWebExtensionWindow>(options.desiredWindow) ?: weakTab.window;
        auto *duplicatedTab = [desiredWindow openNewTabAtIndex:options.desiredIndex];

        [duplicatedTab.mainWebView loadRequest:[NSURLRequest requestWithURL:weakTab.mainWebView.URL]];

        duplicatedTab.selected = options.shouldSelect;

        if (options.shouldActivate)
            desiredWindow.activeTab = duplicatedTab;

        completionHandler(duplicatedTab, nil);
    };

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
    [tab.mainWebView _close];
    tab.mainWebView = nil;

    [_tabs removeObject:tab];

    if (tab == _activeTab) {
        _activeTab = _tabs.firstObject;

        if (_activeTab)
            [_extensionController didActivateTab:_activeTab previousActiveTab:tab];
    }

    [_extensionController didCloseTab:tab windowIsClosing:windowIsClosing];
}

- (void)replaceTab:(TestWebExtensionTab *)oldTab withTab:(TestWebExtensionTab *)newTab
{
    ASSERT([_tabs containsObject:oldTab]);
    ASSERT(![_tabs containsObject:newTab]);

    [oldTab.mainWebView _close];
    oldTab.mainWebView = nil;

    [_tabs replaceObjectAtIndex:[_tabs indexOfObject:oldTab] withObject:newTab];
    [_extensionController didReplaceTab:oldTab withTab:newTab];
}

- (void)moveTab:(TestWebExtensionTab *)tab toIndex:(NSUInteger)newIndex
{
    if (tab.window != self) {
        TestWebExtensionWindow *oldWindow = tab.window;

        auto oldIndex = [oldWindow->_tabs indexOfObject:tab];
        ASSERT(oldIndex != NSNotFound);

        [oldWindow->_tabs removeObjectAtIndex:oldIndex];
        [_tabs insertObject:tab atIndex:newIndex];

        tab.window = self;

        [_extensionController didMoveTab:tab fromIndex:oldIndex inWindow:oldWindow];

        return;
    }

    auto oldIndex = [_tabs indexOfObject:tab];
    ASSERT(oldIndex != NSNotFound);

    [_tabs removeObjectAtIndex:oldIndex];
    [_tabs insertObject:tab atIndex:newIndex];

    [_extensionController didMoveTab:tab fromIndex:oldIndex inWindow:nil];
}

- (NSArray<id<_WKWebExtensionTab>> *)tabsForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _tabs;
}

- (id<_WKWebExtensionTab>)activeTabForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _activeTab;
}

- (_WKWebExtensionWindowType)windowTypeForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _windowType;
}

- (_WKWebExtensionWindowState)windowStateForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _windowState;
}

- (void)setWindowState:(_WKWebExtensionWindowState)state forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    _windowState = state;

    if (state == _WKWebExtensionWindowStateFullscreen) {
        _previousFrame = _frame;
        _frame = _screenFrame;
    } else if (!CGRectIsEmpty(_previousFrame)) {
        _frame = _previousFrame;
        _previousFrame = CGRectNull;
    }

    completionHandler(nil);
}

- (BOOL)isUsingPrivateBrowsingForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _usingPrivateBrowsing;
}

- (CGRect)screenFrameForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _screenFrame;
}

- (CGRect)frameForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _frame;
}

- (void)setFrame:(CGRect)frame forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    _frame = frame;

    completionHandler(nil);
}

- (void)focusForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    if (_didFocus)
        _didFocus();

    completionHandler(nil);
}

- (void)closeForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *error))completionHandler
{
    if (_didClose)
        _didClose();

    completionHandler(nil);
}

@end

namespace TestWebKitAPI {
namespace Util {

RetainPtr<TestWebExtensionManager> loadAndRunExtension(_WKWebExtension *extension, _WKWebExtensionControllerConfiguration *configuration)
{
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension extensionControllerConfiguration:configuration]);
    [manager loadAndRun];
    return manager;
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *manifest, NSDictionary *resources, _WKWebExtensionControllerConfiguration *configuration)
{
    return loadAndRunExtension([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources], configuration);
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *resources, _WKWebExtensionControllerConfiguration *configuration)
{
    return loadAndRunExtension([[_WKWebExtension alloc] _initWithResources:resources], configuration);
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSURL *baseURL, _WKWebExtensionControllerConfiguration *configuration)
{
    return loadAndRunExtension([[_WKWebExtension alloc] initWithResourceBaseURL:baseURL error:nullptr], configuration);
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

} // namespace Util
} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
