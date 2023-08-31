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

#import "TestCocoa.h"
#import "TestWebExtensionsDelegate.h"
#import "WebExtensionUtilities.h"
#import <WebKit/WKFoundation.h>
#import <WebKit/_WKWebExtensionContextPrivate.h>
#import <WebKit/_WKWebExtensionControllerPrivate.h>
#import <WebKit/_WKWebExtensionMatchPatternPrivate.h>
#import <WebKit/_WKWebExtensionPermission.h>
#import <WebKit/_WKWebExtensionPrivate.h>
#import <WebKit/_WKWebExtensionWindow.h>

namespace TestWebKitAPI {

TEST(WKWebExtensionTab, OpenTabs)
{
    auto testExtensionOne = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @3 }]);
    auto testContextOne = adoptNS([[_WKWebExtensionContext alloc] initForExtension:testExtensionOne.get()]);

    auto testExtensionTwo = adoptNS([[_WKWebExtension alloc] _initWithManifestDictionary:@{ @"manifest_version": @3 }]);
    auto testContextTwo = adoptNS([[_WKWebExtensionContext alloc] initForExtension:testExtensionTwo.get()]);

    auto testWindowOne = adoptNS([[TestWebExtensionWindow alloc] init]);
    auto testWindowTwo = adoptNS([[TestWebExtensionWindow alloc] init]);

    auto testTabOne = adoptNS([[TestWebExtensionTab alloc] init]);
    auto testTabTwo = adoptNS([[TestWebExtensionTab alloc] init]);
    auto testTabThree = adoptNS([[TestWebExtensionTab alloc] init]);
    auto testTabFour = adoptNS([[TestWebExtensionTab alloc] init]);

    testWindowOne.get().tabs = @[ testTabOne.get() ];
    testWindowTwo.get().tabs = @[ testTabTwo.get(), testTabThree.get() ];

    auto controllerDelegate = adoptNS([[TestWebExtensionsDelegate alloc] init]);

    controllerDelegate.get().openWindows = ^NSArray<id<_WKWebExtensionWindow>> *(_WKWebExtensionContext *context) {
        return @[ testWindowOne.get(), testWindowTwo.get() ];
    };

    auto testController = adoptNS([[_WKWebExtensionController alloc] init]);
    testController.get().delegate = controllerDelegate.get();

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, [NSSet set]);
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, [NSSet set]);

    [testController loadExtensionContext:testContextOne.get() error:nullptr];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, [NSSet set]);

    [testController loadExtensionContext:testContextTwo.get() error:nullptr];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), nil]));

    [testContextOne didOpenTab:testTabFour.get()];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), nil]));

    testWindowTwo.get().tabs = @[ testTabTwo.get(), testTabThree.get(), testTabFour.get() ];
    [testContextTwo didOpenTab:testTabFour.get()];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));

    testWindowOne.get().tabs = @[ ];
    [testController didCloseTab:testTabOne.get() windowIsClosing:NO];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));

    [testController unloadExtensionContext:testContextOne.get() error:nullptr];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));

    testWindowOne.get().tabs = @[ testTabOne.get() ];
    [testController didOpenTab:testTabOne.get()];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabOne.get(), testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));

    [testController didCloseWindow:testWindowOne.get()];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));

    [testController didCloseWindow:testWindowTwo.get()];

    EXPECT_NS_EQUAL(testContextOne.get().openTabs, ([NSSet setWithObjects:testTabTwo.get(), testTabThree.get(), testTabFour.get(), nil]));
    EXPECT_NS_EQUAL(testContextTwo.get().openTabs, [NSSet set]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(WK_WEB_EXTENSIONS)
