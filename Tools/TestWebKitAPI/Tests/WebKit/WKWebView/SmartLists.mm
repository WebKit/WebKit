/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>

#if PLATFORM(MAC)

// MARK: Utilities

static NSString* const WebSmartListsEnabled = @"WebSmartListsEnabled";

static void setSmartListsPreference(WKWebViewConfiguration *configuration, BOOL value)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"SmartListsAvailable"]) {
            [preferences _setEnabled:value forFeature:feature];
            break;
        }
    }
}

static NSNumber *userDefaultsValue()
{
    return [[NSUserDefaults standardUserDefaults] objectForKey:WebSmartListsEnabled];
}

static void resetUserDefaults()
{
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebSmartListsEnabled];
}

static void setUserDefaultsValue(BOOL value)
{
    [[NSUserDefaults standardUserDefaults] setBool:value forKey:WebSmartListsEnabled];
}

static RetainPtr<NSMenu> invokeContextMenu(TestWKWebView *webView)
{
    RetainPtr delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block RetainPtr<NSMenu> proposedMenu;
    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        proposedMenu = menu;
        completion(nil);
        gotProposedMenu = true;
    }];

    [webView setUIDelegate:delegate.get()];

    [webView waitForNextPresentationUpdate];
    [webView rightClickAtPoint:NSMakePoint(10, [webView frame].size.height - 10)];
    TestWebKitAPI::Util::run(&gotProposedMenu);

    return proposedMenu;
}

// MARK: Tests

TEST(SmartLists, EnablementIsLogicallyConsistentWhenInterfacedThroughResponder)
{
    resetUserDefaults();

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<div>hi</div>"];
    [webView waitForNextPresentationUpdate];

    // Case 1: user default => nil, preference => false

    setSmartListsPreference(configuration.get(), NO);

    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_NULL(userDefaultsValue());

    [webView _setSmartListsEnabled:YES];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_NULL(userDefaultsValue());

    // Case 2: user default => nil, preference => true

    setSmartListsPreference(configuration.get(), YES);

    EXPECT_TRUE([webView _isSmartListsEnabled]);
    EXPECT_NULL(userDefaultsValue());

    [webView _setSmartListsEnabled:NO];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_FALSE([userDefaultsValue() boolValue]);

    [webView _toggleSmartLists:nil];
    EXPECT_TRUE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    // Case 3: user default => true, preference => false

    setSmartListsPreference(configuration.get(), NO);
    setUserDefaultsValue(YES);

    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    [webView _setSmartListsEnabled:YES];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    // Case 4: user default => true, preference => true

    setSmartListsPreference(configuration.get(), YES);
    setUserDefaultsValue(YES);

    EXPECT_TRUE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    [webView _setSmartListsEnabled:NO];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_FALSE([userDefaultsValue() boolValue]);
}

TEST(SmartLists, ContextMenuItemStateIsConsistentWithAvailability)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:@"<body contenteditable>hi</body>"];
    [webView waitForNextPresentationUpdate];

    // Case 1: Available
    {
        setSmartListsPreference(configuration.get(), YES);

        NSString *script = @"document.body.focus()";
        [webView stringByEvaluatingJavaScript:script];

        RetainPtr menu = invokeContextMenu(webView.get());
        RetainPtr substitutionMenu = [menu itemWithTitle:@"Substitutions"];
        EXPECT_NOT_NULL(substitutionMenu.get());

        RetainPtr smartListsItem = [[substitutionMenu submenu] itemWithTitle:@"Smart Lists"];
        EXPECT_TRUE([smartListsItem isEnabled]);
    }

    // Case 2: Unavailable
    {
        setSmartListsPreference(configuration.get(), NO);

        NSString *script = @"document.body.focus()";
        [webView stringByEvaluatingJavaScript:script];

        RetainPtr menu = invokeContextMenu(webView.get());
        RetainPtr substitutionMenu = [menu itemWithTitle:@"Substitutions"];
        EXPECT_NOT_NULL(substitutionMenu.get());

        RetainPtr smartListsItem = [[substitutionMenu submenu] itemWithTitle:@"Smart Lists"];
        EXPECT_FALSE([smartListsItem isEnabled]);
    }
}

#endif // PLATFORM(MAC)

#endif // ENABLE_SWIFTUI
