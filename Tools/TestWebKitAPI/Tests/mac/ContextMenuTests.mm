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
#import "Test.h"

#if PLATFORM(MAC)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import <WebKit/WKMenuItemIdentifiersPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <wtf/BlockPtr.h>

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
    [webView loadHTMLString:@"<a href='simple.html' style='font-size: 100px;'>Hello world</a>" baseURL:[NSBundle.mainBundle.bundleURL URLByAppendingPathComponent:@"TestWebKitAPI.resources"]];
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

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
