/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#if ENABLE(APPLICATION_MANIFEST)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestCocoa.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebCore/ApplicationManifest.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKApplicationManifest.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace TestWebKitAPI {

TEST(ApplicationManifest, Coding)
{
    auto jsonString = @"{ \"name\": \"TestName\", \"short_name\": \"TestShortName\", \"description\": \"TestDescription\", \"scope\": \"https://test.com/app\", \"start_url\": \"https://test.com/app/index.html\", \"display\": \"minimal-ui\", \"theme_color\": \"red\" }";
    RetainPtr<_WKApplicationManifest> manifest { [_WKApplicationManifest applicationManifestFromJSON:jsonString manifestURL:[NSURL URLWithString:@"https://test.com/manifest.json"] documentURL:[NSURL URLWithString:@"https://test.com/"]] };

    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:manifest.get() requiringSecureCoding:YES error:nullptr];
    manifest = [NSKeyedUnarchiver unarchivedObjectOfClasses:[NSSet setWithObject:[_WKApplicationManifest class]] fromData:data error:nullptr];

    EXPECT_TRUE([manifest isKindOfClass:[_WKApplicationManifest class]]);
    EXPECT_STREQ("TestName", manifest.get().name.UTF8String);
    EXPECT_STREQ("TestShortName", manifest.get().shortName.UTF8String);
    EXPECT_STREQ("TestDescription", manifest.get().applicationDescription.UTF8String);
    EXPECT_STREQ("https://test.com/app", manifest.get().scope.absoluteString.UTF8String);
    EXPECT_STREQ("https://test.com/app/index.html", manifest.get().startURL.absoluteString.UTF8String);
    EXPECT_EQ(_WKApplicationManifestDisplayModeMinimalUI,  manifest.get().displayMode);

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
    EXPECT_TRUE(CGColorEqualToColor(manifest.get().themeColor.CGColor, redColor.get()));
}

TEST(ApplicationManifest, Basic)
{
    static bool done = false;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);
    [webView synchronouslyLoadHTMLString:@""];
    [webView.get() _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_NULL(manifest);
        done = true;
    }];
    Util::run(&done);

    done = false;
    [webView synchronouslyLoadHTMLString:@"<link rel=\"manifest\" href=\"invalidurl://path\">"];
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_NULL(manifest);
        done = true;
    }];
    Util::run(&done);

    done = false;
    NSDictionary *manifestObject = @{ @"name": @"Test" };
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\">", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]]];
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_TRUE([manifest.name isEqualToString:@"Test"]);
        done = true;
    }];
    Util::run(&done);

    done = false;
    manifestObject = @{
        @"name": @"A Web Application",
        @"short_name": @"WebApp",
        @"description": @"Hello.",
        @"start_url": @"http://example.com/app/start",
        @"scope": @"http://example.com/app",
        @"theme_color": @"red",
    };
    NSString *htmlString = [NSString stringWithFormat:@"<link rel=\"manifest\" href=\"data:text/plain;charset=utf-8;base64,%@\">", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]];
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://example.com/app/index"]];
    [webView _test_waitForDidFinishNavigation];
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_TRUE([manifest.name isEqualToString:@"A Web Application"]);
        EXPECT_TRUE([manifest.shortName isEqualToString:@"WebApp"]);
        EXPECT_TRUE([manifest.applicationDescription isEqualToString:@"Hello."]);
        EXPECT_TRUE([manifest.startURL isEqual:[NSURL URLWithString:@"http://example.com/app/start"]]);
        EXPECT_TRUE([manifest.scope isEqual:[NSURL URLWithString:@"http://example.com/app"]]);

        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        EXPECT_TRUE(CGColorEqualToColor(manifest.themeColor.CGColor, redColor.get()));

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, DisplayMode)
{
    static bool done;
    NSDictionary *displayModesAndExpectedContent = @{
        @"": @"(display-mode) (display-mode: browser)",
        @"browser": @"(display-mode) (display-mode: browser)",
        @"minimal-ui": @"(display-mode) (display-mode: minimal-ui)",
        @"standalone": @"(display-mode) (display-mode: standalone)",
        @"fullscreen": @"(display-mode) (display-mode: fullscreen)",
    };

    NSURL *baseURL = [[[NSBundle mainBundle] bundleURL] URLByAppendingPathComponent:@"TestWebKitAPI.resources"];
    [displayModesAndExpectedContent enumerateKeysAndObjectsUsingBlock:^(NSString *displayMode, NSString *expectedPageContent, BOOL* stop) {
        @autoreleasepool {
            NSString *m2 = displayMode.length ? [NSString stringWithFormat:@"{\"display\": \"%@\"}", displayMode] : @"{}";
            RetainPtr<_WKApplicationManifest> manifest = [_WKApplicationManifest applicationManifestFromJSON:m2 manifestURL:[baseURL URLByAppendingPathComponent:@"manifest.json"] documentURL:baseURL];
            auto config = adoptNS([[WKWebViewConfiguration alloc] init]);
            config.get()._applicationManifest = manifest.get();
            auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:config.get()]);
            [webView synchronouslyLoadTestPageNamed:@"display-mode"];
            
            done = false;
            [webView _getContentsAsStringWithCompletionHandler:^(NSString *contents, NSError *error) {
                done = true;
                EXPECT_STREQ(expectedPageContent.UTF8String, contents.UTF8String);
            }];
            Util::run(&done);
            [webView removeFromSuperview];
        }
    }];
}

TEST(ApplicationManifest, AlwaysFetch)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    NSDictionary *manifestObject = @{ @"theme_color": @"red" };
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\">", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]]];

    {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        while (!CGColorEqualToColor([webView themeColor].CGColor, redColor.get()))
            Util::runFor(1_s);
    }

    __block bool done = false;
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        EXPECT_TRUE(CGColorEqualToColor(manifest.themeColor.CGColor, redColor.get()));

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, OnlyFirstManifest)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    NSDictionary *manifestObject1 = @{ @"theme_color": @"red" };
    NSDictionary *manifestObject2 = @{ @"theme_color": @"blue" };
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\"><link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\">", [[NSJSONSerialization dataWithJSONObject:manifestObject1 options:0 error:nil] base64EncodedStringWithOptions:0], [[NSJSONSerialization dataWithJSONObject:manifestObject2 options:0 error:nil] base64EncodedStringWithOptions:0]]];

    {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        while (!CGColorEqualToColor([webView themeColor].CGColor, redColor.get()))
            Util::runFor(1_s);
    }

    __block bool done = false;
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        EXPECT_TRUE(CGColorEqualToColor(manifest.themeColor.CGColor, redColor.get()));

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, NoManifest)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    [webView synchronouslyLoadHTMLString:@"Hello World"];

    EXPECT_NULL([webView themeColor]);

    __block bool done = false;
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_NULL(manifest);

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, MediaAttriute)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    NSDictionary *manifestObject1 = @{ @"theme_color": @"blue" };
    NSDictionary *manifestObject2 = @{ @"theme_color": @"red" };
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\" media=\"invalid\"><link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\" media=\"screen\">", [[NSJSONSerialization dataWithJSONObject:manifestObject1 options:0 error:nil] base64EncodedStringWithOptions:0], [[NSJSONSerialization dataWithJSONObject:manifestObject2 options:0 error:nil] base64EncodedStringWithOptions:0]]];

    {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        while (!CGColorEqualToColor([webView themeColor].CGColor, redColor.get()))
            Util::runFor(1_s);
    }

    __block bool done = false;
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        EXPECT_TRUE(CGColorEqualToColor(manifest.themeColor.CGColor, redColor.get()));

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, DoesNotExist)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    [webView synchronouslyLoadHTMLString:@"<link rel=\"manifest\" href=\"does not exist\">"];

    EXPECT_NULL([webView themeColor]);

    __block bool done = false;
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_NULL(manifest);

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, Blocked)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);

    NSDictionary *manifestObject = @{ @"theme_color": @"red" };
    [webView synchronouslyLoadHTMLString:[NSString stringWithFormat:@"<meta http-equiv=\"Content-Security-Policy\" content=\"manifest-src 'none'\"><link rel=\"manifest\" href=\"data:application/manifest+json;charset=utf-8;base64,%@\">", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]]];

    EXPECT_NULL([webView themeColor]);

    __block bool done = false;
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_NULL(manifest);

        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, Icons)
{
    static bool done = false;

    NSArray *expectedIcons = @[ @{
        @"src": @"https://example.com/images/touch/homescreen32.png",
        @"sizes": @"32x32",
        @"type": @"image/png"
    }, @{
        @"src": @"https://example.com/images/touch/homescreen48.png",
        @"sizes": @"48x48",
        @"type": @"image/png",
        @"purpose": @"monochrome maskable"
    }, @{
        @"src": @"https://example.com/images/touch/homescreen128.jpg",
        @"sizes": @"96x96 128x128",
        @"type": @"image/jpg",
        @"purpose": @"monochrome"
    }];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSZeroRect]);
    NSDictionary *manifestObject = @{
        @"name": @"A Web Application",
        @"short_name": @"WebApp",
        @"description": @"Hello.",
        @"start_url": @"http://example.com/app/start",
        @"scope": @"http://example.com/app",
        @"theme_color": @"red",
        @"icons": expectedIcons
    };
    NSString *htmlString = [NSString stringWithFormat:@"<link rel=\"manifest\" href=\"data:text/plain;charset=utf-8;base64,%@\">", [[NSJSONSerialization dataWithJSONObject:manifestObject options:0 error:nil] base64EncodedStringWithOptions:0]];
    [webView loadHTMLString:htmlString baseURL:[NSURL URLWithString:@"http://example.com/app/index"]];
    [webView _test_waitForDidFinishNavigation];
    [webView _getApplicationManifestWithCompletionHandler:^(_WKApplicationManifest *manifest) {
        EXPECT_TRUE([manifest.name isEqualToString:@"A Web Application"]);
        EXPECT_TRUE([manifest.shortName isEqualToString:@"WebApp"]);
        EXPECT_TRUE([manifest.applicationDescription isEqualToString:@"Hello."]);
        EXPECT_TRUE([manifest.startURL isEqual:[NSURL URLWithString:@"http://example.com/app/start"]]);
        EXPECT_TRUE([manifest.scope isEqual:[NSURL URLWithString:@"http://example.com/app"]]);

        auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
        auto redColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), redColorComponents));
        EXPECT_TRUE(CGColorEqualToColor(manifest.themeColor.CGColor, redColor.get()));

        size_t iconIndex = 0;
        for (_WKApplicationManifestIcon *icon in manifest.icons) {
            NSDictionary *expectedIcon = expectedIcons[iconIndex];
            NSString *expectedURLString = [expectedIcon objectForKey:@"src"];
            EXPECT_TRUE([icon.src isEqual:[NSURL URLWithString:expectedURLString]]);
            EXPECT_TRUE([icon.type isEqual:[expectedIcon objectForKey:@"type"]]);

            switch (iconIndex) {
            case 0:
                EXPECT_EQ(icon.sizes.count, 1ul);
                EXPECT_TRUE([icon.sizes[0] isEqual:[expectedIcon objectForKey:@"sizes"]]);
                EXPECT_EQ(icon.purposes.count, 1ul);
                EXPECT_EQ(icon.purposes[0].unsignedLongValue, 1ul);
                break;

            case 1:
                EXPECT_EQ(icon.sizes.count, 1ul);
                EXPECT_TRUE([icon.sizes[0] isEqual:[expectedIcon objectForKey:@"sizes"]]);
                EXPECT_EQ(icon.purposes.count, 2ul);
                EXPECT_EQ(icon.purposes[0].unsignedLongValue, 2ul);
                EXPECT_EQ(icon.purposes[1].unsignedLongValue, 4ul);
                break;

            case 2:
                EXPECT_EQ(icon.sizes.count, 2ul);
                EXPECT_TRUE([icon.sizes[0] isEqual:@"96x96"]);
                EXPECT_TRUE([icon.sizes[1] isEqual:@"128x128"]);
                EXPECT_EQ(icon.purposes.count, 1ul);
                EXPECT_EQ(icon.purposes[0].unsignedLongValue, 2ul);
                break;
            }

            ++iconIndex;
        }
        done = true;
    }];
    Util::run(&done);
}

TEST(ApplicationManifest, IconCoding)
{
    static constexpr auto testURL = "https://example.com/images/touch/homescreen128.jpg"_s;

    WebCore::ApplicationManifest::Icon icon = { URL { testURL }, makeVector<String>(@[@"96x96", @"128x128"]), "image/jpg"_s, { WebCore::ApplicationManifest::Icon::Purpose::Monochrome, WebCore::ApplicationManifest::Icon::Purpose::Maskable } };

    IGNORE_WARNINGS_BEGIN("objc-method-access")
    auto manifestIcon = adoptNS([[_WKApplicationManifestIcon alloc] initWithCoreIcon:&icon]);
    IGNORE_WARNINGS_END

    NSError *error = nil;
    NSData *archiveData = [NSKeyedArchiver archivedDataWithRootObject:manifestIcon.get() requiringSecureCoding:YES error:&error];
    EXPECT_NULL(error);

    _WKApplicationManifestIcon *decodedIcon = [NSKeyedUnarchiver unarchivedObjectOfClass:[_WKApplicationManifestIcon class] fromData:archiveData error:&error];
    EXPECT_NULL(error);

    EXPECT_TRUE([decodedIcon isKindOfClass:[_WKApplicationManifestIcon class]]);
    EXPECT_STREQ(testURL, decodedIcon.src.absoluteString.UTF8String);
    EXPECT_TRUE([decodedIcon.sizes[0] isEqual:@"96x96"]);
    EXPECT_TRUE([decodedIcon.sizes[1] isEqual:@"128x128"]);
    EXPECT_TRUE([decodedIcon.type isEqual:@"image/jpg"]);
    EXPECT_EQ(decodedIcon.purposes.count, 2ul);
    EXPECT_EQ(decodedIcon.purposes[0].unsignedLongValue, 2ul);
    EXPECT_EQ(decodedIcon.purposes[1].unsignedLongValue, 4ul);
}

} // namespace TestWebKitAPI

#endif // ENABLE(APPLICATION_MANIFEST)
