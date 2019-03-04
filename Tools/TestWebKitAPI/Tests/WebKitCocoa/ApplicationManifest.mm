/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKApplicationManifest.h>
#import <pal/spi/cocoa/NSKeyedArchiverSPI.h>

namespace TestWebKitAPI {

TEST(WebKit, ApplicationManifestCoding)
{
    auto jsonString = @"{ \"name\": \"TestName\", \"short_name\": \"TestShortName\", \"description\": \"TestDescription\", \"scope\": \"https://test.com/app\", \"start_url\": \"https://test.com/app/index.html\", \"display\": \"minimal-ui\" }";
    RetainPtr<_WKApplicationManifest> manifest { [_WKApplicationManifest applicationManifestFromJSON:jsonString manifestURL:[NSURL URLWithString:@"https://test.com/manifest.json"] documentURL:[NSURL URLWithString:@"https://test.com/"]] };
    
    manifest = unarchivedObjectOfClassesFromData([NSSet setWithObject:[_WKApplicationManifest class]], securelyArchivedDataWithRootObject(manifest.get()));
    
    EXPECT_TRUE([manifest isKindOfClass:[_WKApplicationManifest class]]);
    EXPECT_STREQ("TestName", manifest.get().name.UTF8String);
    EXPECT_STREQ("TestShortName", manifest.get().shortName.UTF8String);
    EXPECT_STREQ("TestDescription", manifest.get().applicationDescription.UTF8String);
    EXPECT_STREQ("https://test.com/app", manifest.get().scope.absoluteString.UTF8String);
    EXPECT_STREQ("https://test.com/app/index.html", manifest.get().startURL.absoluteString.UTF8String);
    EXPECT_EQ(_WKApplicationManifestDisplayModeMinimalUI,  manifest.get().displayMode);
}

TEST(WebKit, ApplicationManifestBasic)
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
        done = true;
    }];
    Util::run(&done);
}

TEST(WebKit, ApplicationManifestDisplayMode)
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

} // namespace TestWebKitAPI

#endif // ENABLE(APPLICATION_MANIFEST)
