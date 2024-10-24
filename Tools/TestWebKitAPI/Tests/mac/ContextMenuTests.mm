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

#import "config.h"

#if PLATFORM(MAC)

#import "AppKitSPI.h"
#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKMenuItemIdentifiersPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKContextMenuElementInfo.h>
#import <WebKit/_WKHitTestResult.h>
#import <wtf/BlockPtr.h>

@interface PopoverNotificationListener : NSObject
- (instancetype)initWithCallback:(Function<void(NSNotification *)>&&)callback;
@end

@implementation PopoverNotificationListener {
    Function<void(NSNotification *)> _callback;
}

- (instancetype)initWithCallback:(Function<void(NSNotification *)>&&)callback
{
    if (!(self = [super init]))
        return nil;

    auto *notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self selector:@selector(handleNotification:) name:NSPopoverWillShowNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(handleNotification:) name:NSPopoverDidShowNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(handleNotification:) name:NSPopoverWillCloseNotification object:nil];
    [notificationCenter addObserver:self selector:@selector(handleNotification:) name:NSPopoverDidCloseNotification object:nil];
    _callback = WTFMove(callback);
    return self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];

    [super dealloc];
}

- (void)handleNotification:(NSNotification *)notification
{
    if (_callback)
        _callback(notification);
}

@end

@interface NSMenu (ContextMenuTests)
- (NSMenuItem *)itemWithIdentifier:(NSString *)identifier;
@end

@implementation NSMenu (ContextMenuTests)

- (NSMenuItem *)itemWithIdentifier:(NSString *)identifier
{
    for (NSInteger index = 0; index < self.numberOfItems; ++index) {
        NSMenuItem *item = [self itemAtIndex:index];
        if ([item.identifier isEqualToString:identifier])
            return item;
    }
    return nil;
}

@end

using MenuItemFilter = BOOL(^)(NSMenuItem *);

static NSMenuItem *itemMatchingFilter(NSMenu *menu, MenuItemFilter filter)
{
    for (NSInteger index = 0; index < menu.numberOfItems; ++index) {
        auto *item = [menu itemAtIndex:index];
        if (!item)
            continue;

        if (filter(item))
            return item;

        if (item.hasSubmenu) {
            if (auto *foundItem = itemMatchingFilter(item.submenu, filter))
                return foundItem;
        }
    }
    return nil;
}

@interface TestWKWebView (ContextMenuTests)

@end

@implementation TestWKWebView (ContextMenuTests)

- (void)rightClick:(NSPoint)clickLocation andSelectItemMatching:(MenuItemFilter)filter
{
    bool selectedItem = false;
    RetainPtr selectItemTimer = [NSTimer timerWithTimeInterval:0.25 repeats:YES block:[&selectedItem, strongSelf = RetainPtr { self }, filter = makeBlockPtr(filter)](NSTimer *timer) {
        NSMenu *activeMenu = [strongSelf _activeMenu];
        if (!activeMenu)
            return;

        auto *item = itemMatchingFilter(activeMenu, filter.get());
        if (!item)
            return;

        auto *itemMenu = item.menu;
        [itemMenu performActionForItemAtIndex:[itemMenu indexOfItem:item]];
        [activeMenu cancelTracking];
        [timer invalidate];
        selectedItem = true;
    }];

    [NSRunLoop.mainRunLoop addTimer:selectItemTimer.get() forMode:NSEventTrackingRunLoopMode];
    [self.window orderFrontRegardless];
    [self mouseDownAtPoint:NSMakePoint(50, 350) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [self mouseUpAtPoint:NSMakePoint(50, 350) withFlags:0 eventType:NSEventTypeRightMouseUp];
    TestWebKitAPI::Util::run(&selectedItem);
}

- (_WKContextMenuElementInfo *)rightClickAtPointAndWaitForContextMenu:(NSPoint)clickLocation
{
    auto uiDelegate = adoptNS([TestUIDelegate new]);

    __block RetainPtr<_WKContextMenuElementInfo> result;
    __block bool gotProposedMenu = false;
    [uiDelegate setGetContextMenuFromProposedMenu:^(NSMenu *, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        result = elementInfo;
        gotProposedMenu = true;
        completion(nil);
    }];

    EXPECT_NULL(self.UIDelegate);
    self.UIDelegate = uiDelegate.get();
    [self rightClickAtPoint:clickLocation];
    TestWebKitAPI::Util::run(&gotProposedMenu);
    [self waitForNextPresentationUpdate];

    self.UIDelegate = nil;
    return result.autorelease();
}

@end

namespace TestWebKitAPI {

TEST(ContextMenuTests, ProposedMenuContainsSpellingMenu)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block RetainPtr<NSMenu> proposedMenu;
    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        proposedMenu = menu;
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView mouseDownAtPoint:NSMakePoint(10, 10) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(10, 10) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);

    NSMenuItem *spellingMenuItem = [proposedMenu itemWithIdentifier:_WKMenuItemIdentifierSpellingMenu];
    NSMenu *spellingSubmenu = spellingMenuItem.submenu;
    EXPECT_NOT_NULL(spellingMenuItem);
    EXPECT_NOT_NULL([spellingSubmenu itemWithIdentifier:_WKMenuItemIdentifierShowSpellingPanel]);
    EXPECT_NOT_NULL([spellingSubmenu itemWithIdentifier:_WKMenuItemIdentifierCheckSpelling]);
    EXPECT_NOT_NULL([spellingSubmenu itemWithIdentifier:_WKMenuItemIdentifierCheckSpellingWhileTyping]);
    EXPECT_NOT_NULL([spellingSubmenu itemWithIdentifier:_WKMenuItemIdentifierCheckGrammarWithSpelling]);
}

TEST(ContextMenuTests, NavigationTypeWhenOpeningLink)
{
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadHTMLString:@"<a href='simple.html' style='font-size: 100px;'>Hello world</a>" baseURL:NSBundle.test_resourcesBundle.resourceURL];
    [navigationDelegate waitForDidFinishNavigation];

    __block bool didDecideNavigationPolicy = false;
    [navigationDelegate setDecidePolicyForNavigationAction:^(WKNavigationAction *action, void (^decisionHandler)(WKNavigationActionPolicy)) {
        EXPECT_EQ(action.navigationType, WKNavigationTypeLinkActivated);
        EXPECT_WK_STREQ("simple.html", action.request.URL.lastPathComponent);
        decisionHandler(WKNavigationActionPolicyAllow);
        didDecideNavigationPolicy = true;
    }];

    [webView rightClick:NSMakePoint(50, 350) andSelectItemMatching:^BOOL(NSMenuItem *item) {
        return [item.identifier isEqualToString:_WKMenuItemIdentifierOpenLink];
    }];
    Util::run(&didDecideNavigationPolicy);
}

static bool calledOrderFrontColorPanel = false;
static void swizzledOrderFrontColorPanel(id, SEL, id)
{
    calledOrderFrontColorPanel = true;
}

TEST(ContextMenuTests, ShowColorPanel)
{
    InstanceMethodSwizzler swizzler { NSApplication.class, @selector(orderFrontColorPanel:), reinterpret_cast<IMP>(swizzledOrderFrontColorPanel) };

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView rightClick:NSMakePoint(16, 16) andSelectItemMatching:^BOOL(NSMenuItem *item) {
        return [item.title isEqualToString:@"Show Colors"];
    }];
    Util::run(&calledOrderFrontColorPanel);
}

TEST(ContextMenuTests, SharePopoverDoesNotClearSelection)
{
    bool didShowPopover = false;
    auto listener = adoptNS([[PopoverNotificationListener alloc] initWithCallback:[&](NSNotification *notification) {
        if ([notification.name isEqualToString:NSPopoverDidShowNotification])
            didShowPopover = true;
    }]);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    auto presentingViewSwizzler = InstanceMethodSwizzler {
        NSMenu.class,
        @selector(_presentingView),
        imp_implementationWithBlock([&](NSMenu *) {
            return webView.get();
        })
    };

    [webView setForceWindowToBecomeKey:YES];
    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 100px;'>Hello world this is a test</body>"];
    [[webView window] makeFirstResponder:webView.get()];
    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];
    [webView rightClick:NSMakePoint(100, 100) andSelectItemMatching:^BOOL(NSMenuItem *item) {
        return [item.title containsString:@"Share"];
    }];

    TestWebKitAPI::Util::run(&didShowPopover);
    EXPECT_WK_STREQ("Hello world this is a test", [webView selectedText]);
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsHitTestResult)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView mouseDownAtPoint:NSMakePoint(10, 10) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(10, 10) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultDoesNotContainEmptyURLs)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        _WKHitTestResult *hitTestResult = elementInfo.hitTestResult;
        EXPECT_NOT_NULL(hitTestResult);

        // simple.html does not contain any links, so every URL in _WKHitTestResult should be nil.
        EXPECT_NULL(hitTestResult.absoluteImageURL);
        EXPECT_NULL(hitTestResult.absolutePDFURL);
        EXPECT_NULL(hitTestResult.absoluteLinkURL);
        EXPECT_NULL(hitTestResult.absoluteMediaURL);

        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView _setEditable:YES];
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView mouseDownAtPoint:NSMakePoint(10, 10) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(10, 10) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

static NSString *qrCodeSVGString()
{
    return @"<svg xmlns='http://www.w3.org/2000/svg' width='400' height='400' viewBox='0 0 1160 1160'><path fill='#fff' d='M0 0h1160v1160H0z'/><path d='M400 80h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M600 80h40v40h-40zm-200 40h40v40h-40z'/><path d='M440 120h40v40h-40zm80 0h40v40h-40z'/><path d='M560 120h40v40h-40z'/><path d='M600 120h40v40h-40zm120 0h40v40h-40zm-320 40h40v40h-40z'/><path d='M440 160h40v40h-40zm120 0h40v40h-40z'/><path d='M600 160h40v40h-40zm80 0h40v40h-40z'/><path d='M720 160h40v40h-40zm-280 40h40v40h-40z'/><path d='M480 200h40v40h-40z'/><path d='M520 200h40v40h-40zm80 0h40v40h-40z'/><path d='M640 200h40v40h-40z'/><path d='M680 200h40v40h-40z'/><path d='M720 200h40v40h-40zm-320 40h40v40h-40z'/><path d='M440 240h40v40h-40z'/><path d='M480 240h40v40h-40zm120 0h40v40h-40z'/><path d='M640 240h40v40h-40zm80 0h40v40h-40zm-280 40h40v40h-40z'/><path d='M480 280h40v40h-40zm120 0h40v40h-40z'/><path d='M640 280h40v40h-40zm-240 40h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M640 320h40v40h-40zm80 0h40v40h-40zm-240 40h40v40h-40z'/><path d='M520 360h40v40h-40z'/><path d='M560 360h40v40h-40z'/><path d='M600 360h40v40h-40z'/><path d='M640 360h40v40h-40z'/><path d='M680 360h40v40h-40zM80 400h40v40H80zm120 0h40v40h-40z'/><path d='M240 400h40v40h-40z'/><path d='M280 400h40v40h-40z'/><path d='M320 400h40v40h-40z'/><path d='M360 400h40v40h-40z'/><path d='M400 400h40v40h-40zm80 0h40v40h-40z'/><path d='M520 400h40v40h-40z'/><path d='M560 400h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M760 400h40v40h-40zm120 0h40v40h-40zm80 0h40v40h-40z'/><path d='M1000 400h40v40h-40z'/><path d='M1040 400h40v40h-40zM80 440h40v40H80z'/><path d='M120 440h40v40h-40zm80 0h40v40h-40zm200 0h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M600 440h40v40h-40z'/><path d='M640 440h40v40h-40z'/><path d='M680 440h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M880 440h40v40h-40z'/><path d='M920 440h40v40h-40z'/><path d='M960 440h40v40h-40z'/><path d='M1000 440h40v40h-40zm-720 40h40v40h-40z'/><path d='M320 480h40v40h-40z'/><path d='M360 480h40v40h-40zm160 0h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40zm120 0h40v40h-40z'/><path d='M840 480h40v40h-40zm80 0h40v40h-40zm120 0h40v40h-40zm-840 40h40v40h-40zm200 0h40v40h-40zm240 0h40v40h-40z'/><path d='M680 520h40v40h-40z'/><path d='M720 520h40v40h-40zm120 0h40v40h-40zm80 0h40v40h-40z'/><path d='M960 520h40v40h-40z'/><path d='M1000 520h40v40h-40z'/><path d='M1040 520h40v40h-40zm-920 40h40v40h-40zm80 0h40v40h-40zm120 0h40v40h-40zm200 0h40v40h-40zm160 0h40v40h-40z'/><path d='M720 560h40v40h-40zm80 0h40v40h-40zm240 0h40v40h-40zM80 600h40v40H80z'/><path d='M120 600h40v40h-40zm280 0h40v40h-40z'/><path d='M440 600h40v40h-40zm120 0h40v40h-40z'/><path d='M600 600h40v40h-40zm80 0h40v40h-40z'/><path d='M720 600h40v40h-40z'/><path d='M760 600h40v40h-40zm120 0h40v40h-40zm120 0h40v40h-40zM80 640h40v40H80z'/><path d='M120 640h40v40h-40zm200 0h40v40h-40zm360 0h40v40h-40zm80 0h40v40h-40z'/><path d='M800 640h40v40h-40zm80 0h40v40h-40z'/><path d='M920 640h40v40h-40z'/><path d='M960 640h40v40h-40z'/><path d='M1000 640h40v40h-40z'/><path d='M1040 640h40v40h-40zM80 680h40v40H80zm80 0h40v40h-40z'/><path d='M200 680h40v40h-40zm160 0h40v40h-40zm80 0h40v40h-40zm120 0h40v40h-40zm80 0h40v40h-40zm120 0h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M960 680h40v40h-40zm80 0h40v40h-40zM80 720h40v40H80zm80 0h40v40h-40zm120 0h40v40h-40z'/><path d='M320 720h40v40h-40z'/><path d='M360 720h40v40h-40zm80 0h40v40h-40z'/><path d='M480 720h40v40h-40z'/><path d='M520 720h40v40h-40z'/><path d='M560 720h40v40h-40z'/><path d='M600 720h40v40h-40zm80 0h40v40h-40z'/><path d='M720 720h40v40h-40z'/><path d='M760 720h40v40h-40z'/><path d='M800 720h40v40h-40z'/><path d='M840 720h40v40h-40z'/><path d='M880 720h40v40h-40zm80 0h40v40h-40z'/><path d='M1000 720h40v40h-40zm-600 40h40v40h-40z'/><path d='M440 760h40v40h-40zm80 0h40v40h-40zm200 0h40v40h-40zm160 0h40v40h-40zm80 0h40v40h-40z'/><path d='M1000 760h40v40h-40zm-600 40h40v40h-40zm160 0h40v40h-40zm160 0h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M1040 800h40v40h-40zm-640 40h40v40h-40z'/><path d='M440 840h40v40h-40z'/><path d='M480 840h40v40h-40z'/><path d='M520 840h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M720 840h40v40h-40zm160 0h40v40h-40zm-480 40h40v40h-40z'/><path d='M440 880h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40zm80 0h40v40h-40z'/><path d='M720 880h40v40h-40z'/><path d='M760 880h40v40h-40z'/><path d='M800 880h40v40h-40z'/><path d='M840 880h40v40h-40z'/><path d='M880 880h40v40h-40zm120 0h40v40h-40z'/><path d='M1040 880h40v40h-40zm-640 40h40v40h-40zm160 0h40v40h-40z'/><path d='M600 920h40v40h-40z'/><path d='M640 920h40v40h-40z'/><path d='M680 920h40v40h-40zm120 0h40v40h-40zm200 0h40v40h-40z'/><path d='M1040 920h40v40h-40zm-600 40h40v40h-40z'/><path d='M480 960h40v40h-40z'/><path d='M520 960h40v40h-40zm80 0h40v40h-40zm160 0h40v40h-40zm120 0h40v40h-40z'/><path d='M920 960h40v40h-40z'/><path d='M960 960h40v40h-40z'/><path d='M1000 960h40v40h-40z'/><path d='M1040 960h40v40h-40zm-520 40h40v40h-40z'/><path d='M560 1000h40v40h-40z'/><path d='M600 1000h40v40h-40zm80 0h40v40h-40zm160 0h40v40h-40z'/><path d='M880 1000h40v40h-40zm80 0h40v40h-40z'/><path d='M1000 1000h40v40h-40z'/><path d='M1040 1000h40v40h-40zm-640 40h40v40h-40z'/><path d='M440 1040h40v40h-40zm80 0h40v40h-40z'/><path d='M560 1040h40v40h-40z'/><path d='M600 1040h40v40h-40zm120 0h40v40h-40z'/><path d='M760 1040h40v40h-40zm160 0h40v40h-40zm120 0h40v40h-40zM318 80H122 80v42 196 42h42 196 42v-42-196-42h-42zm0 238H122V122h196v196zm720-238H842h-42v42 196 42h42 196 42v-42-196-42h-42zm0 238H842V122h196v196zM318 800H122 80v42 196 42h42 196 42v-42-196-42h-42zm0 238H122V842h196v196zM160 160h120v120H160zm720 0h120v120H880zM160 880h120v120H160z'/></svg>";
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringDefaultConfiguration)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600)]);
    [webView synchronouslyLoadHTMLString:@"<img src='qr-code.png'></img>"];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadString)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<img src='qr-code.png'></img>"];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(150, 150)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringInsideLink)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<a href='https://www.webkit.org'><img src='qr-code.png'></img></a>"];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringCanvas)
{
    auto testMessageHandler = adoptNS([[TestMessageHandler alloc] init]);

    __block bool imageLoaded = false;
    [testMessageHandler addMessage:@"imageLoaded" withHandler:^{
        imageLoaded = true;
    }];

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration userContentController] addScriptMessageHandler:testMessageHandler.get() name:@"testHandler"];
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<canvas id='canvas' width='400' height='400'></canvas>"];

    NSString *drawImageToCanvasScript = @""
        "var canvas = document.getElementById('canvas');\n"
        "var ctx = canvas.getContext('2d');\n"
        "var img = new Image();\n"
        "img.onload = function() {\n"
        "    ctx.drawImage(img, 0, 0);\n"
        "    window.webkit.messageHandlers.testHandler.postMessage('imageLoaded');\n"
        "};\n"
        "img.src = 'qr-code.png';\n";

    [webView stringByEvaluatingJavaScript:drawImageToCanvasScript];
    Util::run(&imageLoaded);

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(150, 150)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringSVG)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:qrCodeSVGString()];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(150, 150)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringObscuredSVG)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"%@<div style='width: 400px; height: 100px; background: red; position: absolute; top: 0px; left: 0px;'></div>", qrCodeSVGString()]];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(150, 550)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringSVGInsideTransformedElement)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<div style='transform: translateX(100px);'>%@</div>", qrCodeSVGString()]];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(50, 550)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(150, 550)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

TEST(ContextMenuTests, ContextMenuElementInfoContainsQRCodePayloadStringSVGPageZoom)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setContextMenuQRCodeDetectionEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
    [webView setPageZoom:1.3];
    [webView synchronouslyLoadHTMLString:qrCodeSVGString()];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(550, 50)];
    EXPECT_NULL(elementInfo.qrCodePayloadString);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(500, 100)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

// FIXME: Re-enable this test once webkit.org/b/266650 is resolved
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED > 140000 && !defined(NDEBUG)
TEST(ContextMenuElementInfoWithQRCodeDetectionCrash, DISABLED_ContextMenuTests)
#else
TEST(ContextMenuTests, ContextMenuElementInfoWithQRCodeDetectionCrash)
#endif
{
    auto createWebView = [] {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [configuration _setContextMenuQRCodeDetectionEnabled:YES];
        auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 600, 600) configuration:configuration.get()]);
        [webView synchronouslyLoadHTMLString:@"<img src='qr-code.png'></img>"];
        return webView;
    };

    @autoreleasepool {
        auto webView = createWebView();
        [webView rightClickAtPoint:CGPointMake(300, 300)];
        [webView waitForNextPresentationUpdate];
        [webView removeFromSuperview];
    }

    auto webView = createWebView();
    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(300, 300)];
    EXPECT_WK_STREQ(elementInfo.qrCodePayloadString, "https://www.webkit.org");
}

#endif // ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

TEST(ContextMenuTests, ContextMenuElementInfoHasEntireImage)
{
    CGFloat iconWidth = 215;
    CGFloat iconHeight = 174;
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, iconWidth * 2, iconHeight * 2)]);
    [webView synchronouslyLoadHTMLString:@"<img src='icon.png'></img>"];

    _WKContextMenuElementInfo *elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(iconWidth, iconHeight)];
    EXPECT_TRUE(elementInfo.hasEntireImage);

    elementInfo = [webView rightClickAtPointAndWaitForContextMenu:NSMakePoint(10, 10)];
    EXPECT_FALSE(elementInfo.hasEntireImage);
}

TEST(ContextMenuTests, HitTestResultElementTypeNone)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_EQ(elementInfo.hitTestResult.elementType, _WKHitTestResultElementTypeNone);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultElementTypeVideo)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_EQ(elementInfo.hitTestResult.elementType, _WKHitTestResultElementTypeVideo);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<video src=\"video-without-audio.mp4\" controls></video>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultWithElementSelected)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE(elementInfo.hitTestResult.isSelected);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 100px;'>This text can be selected.</body>"];
    [[webView window] makeFirstResponder:webView.get()];
    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultWithNoElementSelected)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_FALSE(elementInfo.hitTestResult.isSelected);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 100px; -webkit-user-select: none;'>This text cannot be selected.</body>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultWhenClickingInSubframe)
{
    // This is the escaped version of "data:text/html,<body>Hello from a frame!</body>" since that's what -[NSURL absoluteString] returns.
    NSString *subframeContentsString = @"data:text/html,%3Cbody%3EHello%20from%20a%20frame!%3C/body%3E";

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        _WKHitTestResult *hitTestResult = elementInfo.hitTestResult;
        EXPECT_NOT_NULL(elementInfo.hitTestResult);

        WKFrameInfo *frameInfo = hitTestResult.frameInfo;
        EXPECT_NOT_NULL(frameInfo);
        EXPECT_FALSE(frameInfo.isMainFrame);
        EXPECT_TRUE([subframeContentsString isEqualToString:frameInfo.request.URL.absoluteString]);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<iframe width='400' height='400' src='%@'</iframe>", subframeContentsString]];
    [webView waitForNextPresentationUpdate];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultNonFullscreenMedia)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_FALSE(elementInfo.hitTestResult.isMediaFullscreen);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<video src=\"video-without-audio.mp4\" controls></video>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultMediaDownloadable)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE(elementInfo.hitTestResult.isMediaDownloadable);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<video src=\"video-without-audio.mp4\" controls></video>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultImageMIMEType)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE([elementInfo.hitTestResult.imageMIMEType isEqualToString:@"image/jpeg"]);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<img src='sunset-in-cupertino-600px.jpg'></img>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultLinkLocalDataMIMEType)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE([elementInfo.hitTestResult.linkLocalDataMIMEType isEqualToString:@"image/jpeg"]);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<a href='sunset-in-cupertino-600px.jpg'><img src='sunset-in-cupertino-600px.jpg'></img></a>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultLinkSuggestedFilename)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE([elementInfo.hitTestResult.linkSuggestedFilename isEqualToString:@"Sunset.jpg"]);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<a href='sunset-in-cupertino-600px.jpg' download='Sunset.jpg'><img src='sunset-in-cupertino-600px.jpg'></img></a>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultImageSuggestedFilename)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_TRUE(elementInfo.hasEntireImage);
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE([elementInfo.hitTestResult.imageSuggestedFilename isEqualToString:@"sunset-in-cupertino-600px.jpg"]);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<img src='sunset-in-cupertino-600px.jpg'></img>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

TEST(ContextMenuTests, HitTestResultHasLocalDataForLinkURL)
{
    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *elementInfo, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        EXPECT_NOT_NULL(elementInfo.hitTestResult);
        EXPECT_TRUE(elementInfo.hitTestResult.hasLocalDataForLinkURL);
        completion(nil);
        gotProposedMenu = true;
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 400, 400)]);
    [webView setUIDelegate:delegate.get()];
    [webView synchronouslyLoadHTMLString:@"<a href='sunset-in-cupertino-600px.jpg'><img src='sunset-in-cupertino-600px.jpg'></img></a>"];

    [webView mouseDownAtPoint:NSMakePoint(200, 200) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(200, 200) withFlags:0 eventType:NSEventTypeRightMouseUp];
    Util::run(&gotProposedMenu);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
