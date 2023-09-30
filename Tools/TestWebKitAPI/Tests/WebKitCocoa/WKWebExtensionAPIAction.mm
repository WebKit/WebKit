/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"

namespace TestWebKitAPI {

static auto *actionPopupManifest = @{
    @"manifest_version": @3,

    @"name": @"Tabs Test",
    @"description": @"Tabs Test",
    @"version": @"1",

    @"background": @{
        @"scripts": @[ @"background.js" ],
        @"type": @"module",
        @"persistent": @NO,
    },

    @"action": @{
        @"default_title": @"Test Action",
        @"default_popup": @"popup.html",
        @"default_icon": @{
            @"16": @"toolbar-16.png",
            @"32": @"toolbar-32.png",
        },
    },
};

NSData *makePNGData(CGSize size, SEL colorSelector) {
#if USE(APPKIT)
    auto image = adoptNS([[NSImage alloc] initWithSize:size]);

    [image lockFocus];

    [[NSColor performSelector:colorSelector] setFill];
    NSRectFill(NSMakeRect(0, 0, size.width, size.height));

    [image unlockFocus];

    auto cgImageRef = [image CGImageForProposedRect:NULL context:nil hints:nil];
    auto newRep = adoptNS([[NSBitmapImageRep alloc] initWithCGImage:cgImageRef]);
    [newRep setSize:size];

    return [newRep representationUsingType:NSBitmapImageFileTypePNG properties:@{ }];
#else
    UIGraphicsBeginImageContextWithOptions(size, NO, 0.0);

    [[UIColor performSelector:colorSelector] setFill];
    UIRectFill(CGRectMake(0, 0, size.width, size.height));

    auto *image = UIGraphicsGetImageFromCurrentImageContext();

    UIGraphicsEndImageContext();

    return UIImagePNGRepresentation(image);
#endif
}

TEST(WKWebExtensionAPIAction, PresentActionPopup)
{
    auto *popupPage = @"<b>Hello World!</b>";

    auto *backgroundScript = Util::constructScript(@[
        @"browser.test.yield('Test Popup Action')"
    ]);

    auto *smallToolbarIcon = makePNGData(CGSizeMake(16, 16), @selector(redColor));
    auto *largeToolbarIcon = makePNGData(CGSizeMake(32, 32), @selector(blueColor));

    auto *resources = @{
        @"background.js": backgroundScript,
        @"popup.html": popupPage,
        @"toolbar-16.png": smallToolbarIcon,
        @"toolbar-32.png": largeToolbarIcon,
    };

    auto extension = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:actionPopupManifest resources:resources]);
    auto manager = adoptNS([[TestWebExtensionManager alloc] initForExtension:extension.get()]);

    manager.get().internalDelegate.presentActionPopup = ^(_WKWebExtensionAction *action) {
        EXPECT_TRUE(action.hasPopup);
        EXPECT_TRUE(action.isEnabled);
        EXPECT_NULL(action.badgeText);

        EXPECT_NS_EQUAL(action.displayLabel, @"Test Action");

        auto *smallIcon = [action iconForSize:CGSizeMake(16, 16)];
        EXPECT_NOT_NULL(smallIcon);
        EXPECT_TRUE(CGSizeEqualToSize(smallIcon.size, CGSizeMake(16, 16)));

        auto *largeIcon = [action iconForSize:CGSizeMake(32, 32)];
        EXPECT_NOT_NULL(largeIcon);
        EXPECT_TRUE(CGSizeEqualToSize(largeIcon.size, CGSizeMake(32, 32)));

        EXPECT_NOT_NULL(action.popupWebView);
        EXPECT_FALSE(action.popupWebView.loading);

        NSURL *webViewURL = action.popupWebView.URL;
        EXPECT_NS_EQUAL(webViewURL.scheme, @"webkit-extension");
        EXPECT_NS_EQUAL(webViewURL.path, @"/popup.html");

        [action closePopupWebView];
    };

    [manager loadAndRun];

    EXPECT_NS_EQUAL(manager.get().yieldMessage, @"Test Popup Action");

    [manager.get().context performActionForTab:manager.get().defaultTab];
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
