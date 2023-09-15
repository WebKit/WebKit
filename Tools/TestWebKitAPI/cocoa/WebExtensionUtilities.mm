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
#import "WebExtensionUtilities.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>

@interface TestWebExtensionManager () <_WKWebExtensionControllerDelegatePrivate>
@end

@implementation TestWebExtensionManager {
    bool _done;
}

- (instancetype)initForExtension:(_WKWebExtension *)extension
{
    if (!(self = [super init]))
        return nil;

    _extension = extension;
    _context = [[_WKWebExtensionContext alloc] initForExtension:extension];
    _controller = [[_WKWebExtensionController alloc] initWithConfiguration:_WKWebExtensionControllerConfiguration.nonPersistentConfiguration];

    _context._testingMode = YES;

    // This should always be self. If you need the delegate, use the controllerDelegate property.
    // Delegate method calls will be forwarded to the controllerDelegate.
    _controller.delegate = self;

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

- (void)load
{
    NSError *error;
    EXPECT_TRUE([_controller loadExtensionContext:_context error:&error]);
    EXPECT_NULL(error);
}

- (void)run
{
    _done = false;
    TestWebKitAPI::Util::run(&_done);
}

- (void)loadAndRun
{
    [self load];
    [self run];
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
    _yieldMessage = [message copy];
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

@implementation TestWebExtensionTab {
    __weak _WKWebExtensionController *_extensionController;
}

- (instancetype)initWithWindow:(id<_WKWebExtensionWindow>)window extensionController:(_WKWebExtensionController *)extensionController
{
    if (!(self = [super init]))
        return nil;

    _window = window;

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    configuration._webExtensionController = extensionController;

    _mainWebView = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration];
    _extensionController = extensionController;

    return self;
}

- (id<_WKWebExtensionWindow>)windowForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _window;
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

    completionHandler(nil);
}

- (void)reloadFromOriginForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_reloadFromOrigin)
        _reloadFromOrigin();

    completionHandler(nil);
}

- (void)goBackForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_goBack)
        _goBack();

    completionHandler(nil);
}

- (void)goForwardForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (_goForward)
        _goForward();

    completionHandler(nil);
}

- (id<_WKWebExtensionTab>)parentTabForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _parentTab;
}

- (void)setParentTab:(id<_WKWebExtensionTab>)parentTab forWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _parentTab = parentTab;

    completionHandler(nil);
}

- (BOOL)isPinnedForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _pinned;
}

- (void)pinForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _pinned = YES;

    completionHandler(nil);
}

- (void)unpinForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _pinned = NO;

    completionHandler(nil);
}

- (BOOL)isMutedForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _muted;
}

- (void)muteForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _muted = YES;

    completionHandler(nil);
}

- (void)unmuteForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _muted = NO;

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
    if (auto *window = dynamic_objc_cast<TestWebExtensionWindow>(_window))
        window.activeTab = self;

    _selected = YES;

    completionHandler(nil);
}

- (BOOL)isSelectedForWebExtensionContext:(_WKWebExtensionContext *)context
{
    return _selected || dynamic_objc_cast<TestWebExtensionWindow>(_window).activeTab == self;
}

- (void)selectForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _selected = YES;

    completionHandler(nil);
}

- (void)deselectForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    _selected = NO;

    completionHandler(nil);
}

- (void)closeForWebExtensionContext:(_WKWebExtensionContext *)context completionHandler:(void (^)(NSError *))completionHandler
{
    if (auto *window = dynamic_objc_cast<TestWebExtensionWindow>(_window))
        [window closeTab:self];

    [_extensionController didCloseTab:self windowIsClosing:NO];

    completionHandler(nil);
}

@end

@implementation TestWebExtensionWindow {
    CGRect _previousFrame;
    NSMutableArray *_tabs;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    _tabs = [NSMutableArray array];
    _windowState = _WKWebExtensionWindowStateNormal;
    _windowType = _WKWebExtensionWindowTypeNormal;

    _screenFrame = CGRectMake(0, 0, 1920, 1080);

#if PLATFORM(MAC)
    // This is 50pt from top on the screen in Mac screen coordinates.
    _frame = CGRectMake(100, _screenFrame.size.height - 600 - 50, 800, 600);
#else
    _frame = CGRectMake(100, 50, 800, 600);
#endif

    _previousFrame = CGRectNull;

    return self;
}

- (NSArray<id<_WKWebExtensionTab>> *)tabs
{
    return [_tabs copy];
}

- (void)setTabs:(NSArray<id<_WKWebExtensionTab>> *)tabs
{
    _tabs = [tabs mutableCopy];
    _activeTab = _tabs.firstObject;
}

- (void)closeTab:(id<_WKWebExtensionTab>)tab
{
    [_tabs removeObject:tab];
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

RetainPtr<TestWebExtensionManager> loadAndRunExtension(_WKWebExtension *extension)
{
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension]);
    [manager loadAndRun];
    return manager;
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *manifest, NSDictionary *resources)
{
    return loadAndRunExtension([[_WKWebExtension alloc] _initWithManifestDictionary:manifest resources:resources]);
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *resources)
{
    return loadAndRunExtension([[_WKWebExtension alloc] _initWithResources:resources]);
}

RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSURL *baseURL)
{
    return loadAndRunExtension([[_WKWebExtension alloc] initWithResourceBaseURL:baseURL error:nullptr]);
}

} // namespace Util
} // namespace TestWebKitAPI
