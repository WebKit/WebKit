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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Helpers/cocoa/TestCocoa.h"
#include "Helpers/cocoa/TestWebExtensionsDelegate.h"
#include "Helpers/Utilities.h"
#include "Helpers/WTFTestUtilities.h"

#ifdef __OBJC__

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

@class TestWebExtensionTab;
@class TestWebExtensionWindow;
@class TestWebExtensionsDelegate;

NS_SWIFT_UI_ACTOR
@interface TestWebExtensionManager : NSObject

- (instancetype)initForExtension:(WKWebExtension *)extension;
- (instancetype)initForExtension:(WKWebExtension *)extension extensionControllerConfiguration:(nullable WKWebExtensionControllerConfiguration *)configuration;
- (instancetype)initForExtension:(WKWebExtension *)extension extensionControllerConfiguration:(nullable WKWebExtensionControllerConfiguration *)configuration usesEnhancedSecurity:(BOOL)usesEnhancedSecurity;

// The equivalent of Util::parseExtension() for callers that cannot spell RetainPtr, such as Swift.
- (instancetype)initWithManifest:(NSDictionary<NSString *, id> *)manifest resources:(nullable NSDictionary<NSString *, id> *)resources;
- (instancetype)initWithManifest:(NSDictionary<NSString *, id> *)manifest resources:(nullable NSDictionary<NSString *, id> *)resources extensionControllerConfiguration:(nullable WKWebExtensionControllerConfiguration *)configuration;

@property (nonatomic, strong) WKWebExtension *extension;
// Cleared by tests that check what the controller does once the context is released.
@property (nonatomic, strong, nullable) WKWebExtensionContext *context;
@property (nonatomic, strong) WKWebExtensionController *controller;
@property (nonatomic, weak, nullable) id <WKWebExtensionControllerDelegate> controllerDelegate;

@property (nonatomic, readonly, strong) TestWebExtensionsDelegate *internalDelegate;

// Nil once the last window has been closed; every window and tab the manager creates itself outlives that.
@property (nonatomic, readonly, strong, nullable) TestWebExtensionWindow *defaultWindow;
@property (nonatomic, readonly, strong, nullable) TestWebExtensionTab *defaultTab;

@property (nonatomic, readonly, copy) NSArray<TestWebExtensionWindow *> *windows;
@property (nonatomic, readonly, copy) NSArray<NSString *> *testsAdded;
@property (nonatomic, readonly, copy) NSArray<NSString *> *testsStarted;
@property (nonatomic, readonly, copy) NSDictionary<NSString *, id> *testResults;

// Collects failures from the extension's `browser.test` harness and from -load / -unload rather
// than recording them with GoogleTest, so a Swift test can surface them from whichever call awaited.
@property (nonatomic) BOOL collectsFailures;

// Fails with anything the harness collected since the last check, and clears it.
- (BOOL)checkCollectedFailuresWithError:(NSError **)error;

- (TestWebExtensionWindow *)openNewWindow;
- (TestWebExtensionWindow *)openNewWindowUsingPrivateBrowsing:(BOOL)usesPrivateBrowsing;
- (void)focusWindow:(nullable TestWebExtensionWindow *)window;
- (void)closeWindow:(TestWebExtensionWindow *)window;

- (void)sendTestMessage:(NSString *)message;
- (void)sendTestMessage:(NSString *)message withArgument:(nullable id)argument;
- (void)sendTestStartedWithArgument:(nullable id)argument;
- (void)sendTestFinishedWithArgument:(nullable id)argument;

- (void)loadAndRun NS_SWIFT_UNAVAILABLE("Spins the run loop; use loadAndRun() async instead.");

- (void)load;
- (void)unload;

- (void)run NS_SWIFT_UNAVAILABLE("Spins the run loop; use run() async instead.");
- (void)runForTimeInterval:(NSTimeInterval)interval NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");
- (id)runUntilTestMessage:(NSString *)message NS_SWIFT_UNAVAILABLE("Spins the run loop; use waitForTestMessage(_:) async instead.");
- (void)runUntilContextError NS_SWIFT_UNAVAILABLE("Spins the run loop; add an async variant instead.");

- (void)runWithCompletionHandler:(void (^)(NSError * _Nullable error))completionHandler;
- (void)waitForTestMessage:(NSString *)message completionHandler:(void (^)(NSError * _Nullable error))completionHandler NS_SWIFT_NAME(waitForTestMessage(_:completionHandler:));
- (void)loadAndRunWithCompletionHandler:(void (^)(NSError * _Nullable error))completionHandler;

- (void)done;

@end

NS_SWIFT_UI_ACTOR
@interface TestWebExtensionTab : NSObject <WKWebExtensionTab>

- (instancetype)initWithWindow:(nullable TestWebExtensionWindow *)window extensionController:(nullable WKWebExtensionController *)extensionController NS_DESIGNATED_INITIALIZER;

- (void)assignWindow:(nullable TestWebExtensionWindow *)window;

@property (nonatomic, weak, nullable) TestWebExtensionWindow *window;

// Nil for a tab created without an extension controller, which has nothing to host a web view for.
@property (nonatomic, strong, nullable) WKWebView *webView;

@property (nonatomic, copy, nullable) NSURL *overrideURL;

- (void)changeWebViewIfNeededForURL:(NSURL *)url forExtensionContext:(WKWebExtensionContext *)context;

@property (nonatomic, weak, nullable) TestWebExtensionTab *parentTab;

@property (nonatomic) bool shouldBypassPermissions;
@property (nonatomic, getter=isPinned) bool pinned;
@property (nonatomic, getter=isMuted) bool muted;
@property (nonatomic, getter=isSelected) bool selected;
@property (nonatomic, getter=isShowingReaderMode) bool showingReaderMode;

@property (nonatomic, copy, nullable) void (^setReaderModeShowing)(BOOL);
@property (nonatomic, copy, nullable) NSLocale * _Nullable (^webpageLocale)(void);

@property (nonatomic, copy, nullable) void (^reload)(BOOL);
@property (nonatomic, copy, nullable) void (^goBack)(void);
@property (nonatomic, copy, nullable) void (^goForward)(void);
@property (nonatomic, copy, nullable) void (^duplicate)(WKWebExtensionTabConfiguration *, void (^completionHandler)(TestWebExtensionTab * _Nullable, NSError * _Nullable));

@end

NS_SWIFT_UI_ACTOR
@interface TestWebExtensionWindow : NSObject <WKWebExtensionWindow>

- (instancetype)initWithExtensionController:(nullable WKWebExtensionController *)extensionController usesPrivateBrowsing:(BOOL)usesPrivateBrowsing;
- (instancetype)initWithExtensionController:(nullable WKWebExtensionController *)extensionController usesPrivateBrowsing:(BOOL)usesPrivateBrowsing usesEnhancedSecurity:(BOOL)usesEnhancedSecurity NS_DESIGNATED_INITIALIZER;

@property (nonatomic, copy) NSArray<TestWebExtensionTab *> *tabs;

// Nil while the window has no tabs, which happens between closing the last one and opening another.
@property (nonatomic, strong, nullable) TestWebExtensionTab *activeTab;

- (TestWebExtensionTab *)openNewTab;
- (TestWebExtensionTab *)openNewTabAtIndex:(NSUInteger)index;

- (NSUInteger)removeTab:(TestWebExtensionTab *)tab;
- (void)insertTab:(TestWebExtensionTab *)tab atIndex:(NSUInteger)index;

- (void)closeTab:(TestWebExtensionTab *)tab;
- (void)closeTab:(TestWebExtensionTab *)tab windowIsClosing:(BOOL)windowIsClosing;
- (void)replaceTab:(TestWebExtensionTab *)oldTab withTab:(TestWebExtensionTab *)newTab;
- (void)moveTab:(TestWebExtensionTab *)oldTab toIndex:(NSUInteger)newIndex;

@property (nonatomic) WKWebExtensionWindowState windowState;
@property (nonatomic) WKWebExtensionWindowType windowType;

@property (nonatomic) CGRect frame;
@property (nonatomic) CGRect screenFrame;

@property (nonatomic, readonly, getter=isUsingPrivateBrowsing) BOOL usingPrivateBrowsing;
@property (nonatomic, readonly, getter=isUsingEnhancedSecurity) BOOL usingEnhancedSecurity;

@property (nonatomic, copy, nullable) void (^didFocus)(void);
@property (nonatomic, copy, nullable) void (^didClose)(void);

@end

NS_HEADER_AUDIT_END(nullability, sendability)

#else // not __OBJC__

OBJC_CLASS TestWebExtensionManager;
OBJC_CLASS TestWebExtensionTab;
OBJC_CLASS TestWebExtensionWindow;
OBJC_CLASS TestWebExtensionsDelegate;

#endif // __OBJC__

#ifdef __cplusplus

namespace TestWebKitAPI::Util {

#ifdef __OBJC__

inline NSString * _Nonnull constructScript(NSArray * _Nonnull lines) { return [lines componentsJoinedByString:@"\n"]; }
inline NSString * _Nonnull constructJSArrayOfStrings(NSArray * _Nonnull elements) { return [NSString stringWithFormat:@"['%@']", [elements componentsJoinedByString:@"', '"]]; }

NSData * _Nonnull makePNGData(CGSize, SEL _Nonnull colorSelector);

enum class Appearance { Light, Dark };

void performWithAppearance(Appearance, void (^ _Nonnull block)(void));

#endif

extern bool shouldEnableSiteIsolationForWebExtensionsTest;

RetainPtr<TestWebExtensionManager> parseExtension(NSDictionary * _Nonnull manifest, NSDictionary * _Nonnull resources, WKWebExtensionControllerConfiguration * _Nullable = nil, BOOL usesEnhancedSecurity = NO);
RetainPtr<TestWebExtensionManager> loadExtension(NSDictionary * _Nonnull manifest, NSDictionary * _Nonnull resources, WKWebExtensionControllerConfiguration * _Nullable = nil, BOOL usesEnhancedSecurity = NO);
void loadAndRunExtension(NSDictionary * _Nonnull manifest, NSDictionary * _Nonnull resources, WKWebExtensionControllerConfiguration * _Nullable = nil);

} // namespace TestWebKitAPI::Util

#endif // __cplusplus

#endif // ENABLE(WK_WEB_EXTENSIONS)
