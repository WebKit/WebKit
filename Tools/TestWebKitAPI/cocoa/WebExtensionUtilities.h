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

#pragma once

#if ENABLE(WK_WEB_EXTENSIONS)

#include "TestCocoa.h"
#include "TestWebExtensionsDelegate.h"
#include "Utilities.h"
#include "WTFTestUtilities.h"

#ifdef __OBJC__

@class TestWebExtensionTab;
@class TestWebExtensionWindow;
@class TestWebExtensionsDelegate;

@interface TestWebExtensionManager : NSObject

- (instancetype)initForExtension:(_WKWebExtension *)extension;
- (instancetype)initForExtension:(_WKWebExtension *)extension extensionControllerConfiguration:(_WKWebExtensionControllerConfiguration *)configuration;

@property (nonatomic, strong) _WKWebExtension *extension;
@property (nonatomic, strong) _WKWebExtensionContext *context;
@property (nonatomic, strong) _WKWebExtensionController *controller;
@property (nonatomic, weak) id <_WKWebExtensionControllerDelegate> controllerDelegate;

@property (nonatomic, readonly, strong) TestWebExtensionsDelegate *internalDelegate;
@property (nonatomic, readonly, strong) TestWebExtensionWindow *defaultWindow;
@property (nonatomic, readonly, strong) TestWebExtensionTab *defaultTab;
@property (nonatomic, readonly, copy) NSArray<TestWebExtensionWindow *> *windows;

- (TestWebExtensionWindow *)openNewWindow;
- (TestWebExtensionWindow *)openNewWindowUsingPrivateBrowsing:(BOOL)usesPrivateBrowsing;
- (void)focusWindow:(TestWebExtensionWindow *)window;
- (void)closeWindow:(TestWebExtensionWindow *)window;

@property (nonatomic, readonly, strong) NSString *yieldMessage;

- (void)load;
- (void)run;
- (void)runForTimeInterval:(NSTimeInterval)interval;
- (void)loadAndRun;
- (void)done;

@end

@interface TestWebExtensionTab : NSObject <_WKWebExtensionTab>

- (instancetype)initWithWindow:(TestWebExtensionWindow *)window extensionController:(_WKWebExtensionController *)extensionController NS_DESIGNATED_INITIALIZER;

@property (nonatomic, weak) TestWebExtensionWindow * window;
@property (nonatomic, strong) WKWebView *mainWebView;

- (void)changeWebViewIfNeededForURL:(NSURL *)url forExtensionContext:(_WKWebExtensionContext *)context;

@property (nonatomic, weak) TestWebExtensionTab *parentTab;

@property (nonatomic, getter=isPinned) bool pinned;
@property (nonatomic, getter=isMuted) bool muted;
@property (nonatomic, getter=isSelected) bool selected;
@property (nonatomic, getter=isShowingReaderMode) bool showingReaderMode;

@property (nonatomic, copy) void (^toggleReaderMode)(void);
@property (nonatomic, copy) NSLocale *(^detectWebpageLocale)(void);

@property (nonatomic, copy) void (^reload)(void);
@property (nonatomic, copy) void (^reloadFromOrigin)(void);
@property (nonatomic, copy) void (^goBack)(void);
@property (nonatomic, copy) void (^goForward)(void);
@property (nonatomic, copy) void (^duplicate)(_WKWebExtensionTabCreationOptions *, void (^completionHandler)(TestWebExtensionTab *, NSError *));

@end

@interface TestWebExtensionWindow : NSObject <_WKWebExtensionWindow>

- (instancetype)initWithExtensionController:(_WKWebExtensionController *)extensionController usesPrivateBrowsing:(BOOL)usesPrivateBrowsing NS_DESIGNATED_INITIALIZER;

@property (nonatomic, copy) NSArray<TestWebExtensionTab *> *tabs;
@property (nonatomic, strong) TestWebExtensionTab * activeTab;

- (TestWebExtensionTab *)openNewTab;
- (TestWebExtensionTab *)openNewTabAtIndex:(NSUInteger)index;

- (void)closeTab:(TestWebExtensionTab *)tab;
- (void)closeTab:(TestWebExtensionTab *)tab windowIsClosing:(BOOL)windowIsClosing;
- (void)replaceTab:(TestWebExtensionTab *)oldTab withTab:(TestWebExtensionTab *)newTab;
- (void)moveTab:(TestWebExtensionTab *)oldTab toIndex:(NSUInteger)newIndex;

@property (nonatomic) _WKWebExtensionWindowState windowState;
@property (nonatomic) _WKWebExtensionWindowType windowType;

@property (nonatomic) CGRect frame;
@property (nonatomic) CGRect screenFrame;

@property (nonatomic, readonly, getter=isUsingPrivateBrowsing) BOOL usingPrivateBrowsing;

@property (nonatomic, copy) void (^didFocus)(void);
@property (nonatomic, copy) void (^didClose)(void);

@end

#else // not __OBJC__

OBJC_CLASS TestWebExtensionManager;
OBJC_CLASS TestWebExtensionTab;
OBJC_CLASS TestWebExtensionWindow;
OBJC_CLASS TestWebExtensionsDelegate;

#endif // __OBJC__

namespace TestWebKitAPI::Util {

#ifdef __OBJC__

inline NSString *constructScript(NSArray *lines) { return [lines componentsJoinedByString:@"\n"]; }
inline NSString *constructJSArrayOfStrings(NSArray *elements) { return [NSString stringWithFormat:@"['%@']", [elements componentsJoinedByString:@"', '"]]; }

NSData *makePNGData(CGSize, SEL colorSelector);

#endif

RetainPtr<TestWebExtensionManager> loadAndRunExtension(_WKWebExtension *, _WKWebExtensionControllerConfiguration * = nil);
RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *manifest, NSDictionary *resources, _WKWebExtensionControllerConfiguration * = nil);
RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSDictionary *resources, _WKWebExtensionControllerConfiguration * = nil);
RetainPtr<TestWebExtensionManager> loadAndRunExtension(NSURL *baseURL, _WKWebExtensionControllerConfiguration * = nil);

} // namespace TestWebKitAPI::Util

#endif // ENABLE(WK_WEB_EXTENSIONS)
