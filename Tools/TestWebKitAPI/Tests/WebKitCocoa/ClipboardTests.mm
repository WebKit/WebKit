/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "TestCocoa.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import <CoreServices/CoreServices.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebsiteDataStoreRef.h>
#import <WebKit/_WKFeature.h>

#if PLATFORM(MAC)
#import <pal/spi/mac/NSPasteboardSPI.h>
#endif

@interface TestWKWebView (ClipboardTests)

- (void)readClipboard;

@end

@implementation TestWKWebView (ClipboardTests)

- (void)readClipboard
{
    __block bool done = false;
    [self performAfterReceivingMessage:@"readClipboard" action:^{
        done = true;
    }];
    [self evaluateJavaScript:@"readClipboard()" completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
}

- (void)writeString:(NSString *)string toClipboardWithType:(NSString *)type
{
    __block bool done = false;
    [self performAfterReceivingMessage:@"wroteStringToClipboard" action:^{
        done = true;
    }];
    [self evaluateJavaScript:[NSString stringWithFormat:@"writeStringToClipboard(`%@`, `%@`)", type, string] completionHandler:nil];
    TestWebKitAPI::Util::run(&done);
}

@end

static void setClipboardConfigurationPreferences(WKWebViewConfiguration *configuration)
{
    [[configuration preferences] _setDOMPasteAllowed:YES];
    [[configuration preferences] _setJavaScriptCanAccessClipboard:YES];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"AsyncClipboardAPIEnabled"]) {
            [[configuration preferences] _setEnabled:YES forFeature:feature];
            break;
        }
    }
}

static RetainPtr<TestWKWebView> createWebViewForClipboardTests()
{
#if PLATFORM(IOS_FAMILY)
    TestWebKitAPI::Util::instantiateUIApplicationIfNeeded();
#endif
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    setClipboardConfigurationPreferences(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"clipboard"];
    return webView;
}

static void writeMultipleObjectsToPlatformPasteboard()
{
#if PLATFORM(MAC)
    RetainPtr firstItem = adoptNS([[NSPasteboardItem alloc] init]);
    [firstItem setString:@"Hello" forType:NSPasteboardTypeString];

    RetainPtr secondItem = adoptNS([[NSPasteboardItem alloc] init]);
    [secondItem setString:@"https://apple.com/" forType:NSPasteboardTypeURL];

    RetainPtr thirdItem = adoptNS([[NSPasteboardItem alloc] init]);
    [thirdItem setString:@"<strong style='color: rgb(0, 255, 0);'>Hello world</strong>" forType:NSPasteboardTypeHTML];

    RetainPtr fourthItem = adoptNS([[NSPasteboardItem alloc] init]);
    [fourthItem setString:@"WebKit" forType:NSPasteboardTypeString];
    [fourthItem setString:@"https://webkit.org/" forType:NSPasteboardTypeURL];
    [fourthItem setString:@"<a href='https://webkit.org/'>Hello world</a>" forType:NSPasteboardTypeHTML];

    NSPasteboard *pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
    [pasteboard writeObjects:@[firstItem.get(), secondItem.get(), thirdItem.get(), fourthItem.get()]];
#elif PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS) && !PLATFORM(APPLETV)
    RetainPtr firstItem = adoptNS([[NSItemProvider alloc] initWithObject:@"Hello"]);
    RetainPtr secondItem = adoptNS([[NSItemProvider alloc] initWithObject:[NSURL URLWithString:@"https://apple.com/"]]);
    RetainPtr thirdItem = adoptNS([[NSItemProvider alloc] init]);
    [thirdItem registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler([@"<strong style='color: rgb(0, 255, 0);'>Hello world</strong>" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];

    RetainPtr fourthItem = adoptNS([[NSItemProvider alloc] init]);
    [fourthItem registerObject:@"WebKit" visibility:NSItemProviderRepresentationVisibilityAll];
    [fourthItem registerObject:[NSURL URLWithString:@"https://webkit.org/"] visibility:NSItemProviderRepresentationVisibilityAll];
    [fourthItem registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler([@"<a href='https://webkit.org/'>Hello world</a>" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];

    UIPasteboard.generalPasteboard.itemProviders = @[ firstItem.get(), secondItem.get(), thirdItem.get(), fourthItem.get() ];
#endif
}

static RetainPtr<NSString> readMarkupFromPasteboard()
{
#if PLATFORM(MAC)
    NSData *rawData = [NSPasteboard.generalPasteboard dataForType:WebCore::legacyHTMLPasteboardType()];
#else
    NSData *rawData = [UIPasteboard.generalPasteboard dataForPasteboardType:(__bridge NSString *)kUTTypeHTML];
#endif
    return adoptNS([[NSString alloc] initWithData:rawData encoding:NSUTF8StringEncoding]);
}

// rdar://138144869
#if PLATFORM(IOS) && !defined(NDEBUG)
TEST(ClipboardTests, DISABLED_ReadMultipleItems)
#else
TEST(ClipboardTests, ReadMultipleItems)
#endif
{
    RetainPtr webView = createWebViewForClipboardTests();
    writeMultipleObjectsToPlatformPasteboard();
    [webView readClipboard];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"exception ? exception.message : ''"]);
    EXPECT_EQ(4U, [[webView objectByEvaluatingJavaScript:@"clipboardData.length"] unsignedIntValue]);
    EXPECT_WK_STREQ("Hello", [webView stringByEvaluatingJavaScript:@"clipboardData[0]['text/plain']"]);
    EXPECT_WK_STREQ("https://apple.com/", [webView stringByEvaluatingJavaScript:@"clipboardData[1]['text/uri-list']"]);
    EXPECT_WK_STREQ("rgb(0, 255, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(clipboardData[2]['text/html'].querySelector('strong')).color"]);
    EXPECT_WK_STREQ("WebKit", [webView stringByEvaluatingJavaScript:@"clipboardData[3]['text/plain']"]);
    EXPECT_WK_STREQ("https://webkit.org/", [webView objectByEvaluatingJavaScript:@"clipboardData[3]['text/uri-list']"]);
    EXPECT_WK_STREQ("https://webkit.org/", [webView stringByEvaluatingJavaScript:@"clipboardData[3]['text/html'].querySelector('a').href"]);
}

TEST(ClipboardTests, WriteSanitizedMarkup)
{
    RetainPtr webView = createWebViewForClipboardTests();
    [webView writeString:@"<script>/* super secret */</script>This is a test." toClipboardWithType:@"text/html"];

    RetainPtr writtenMarkup = readMarkupFromPasteboard();
    EXPECT_TRUE([writtenMarkup containsString:@"This is a test."]);
    EXPECT_FALSE([writtenMarkup containsString:@"super secret"]);
    EXPECT_FALSE([writtenMarkup containsString:@"<script>"]);
}

#if PLATFORM(MAC)

static RetainPtr<TestWKWebView> createEphemeralWebViewForClipboardTests()
{
    RetainPtr ephemeralStore = [WKWebsiteDataStore nonPersistentDataStore];
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    configuration.get().websiteDataStore = ephemeralStore.get();

    setClipboardConfigurationPreferences(configuration.get());

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body>Hello world</body>"];
    return webView;
}

TEST(ClipboardTests, ConvertTIFFToPNGWhenPasting)
{
    RetainPtr webView = createWebViewForClipboardTests();
    RetainPtr url = [NSBundle.test_resourcesBundle URLForResource:@"sunset-in-cupertino-100px" withExtension:@"tiff"];
    auto pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
    [pasteboard setData:[NSData dataWithContentsOfURL:url.get()] forType:NSPasteboardTypeTIFF];
    [webView readClipboard];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"exception ? exception.message : ''"]);
    EXPECT_EQ(1U, [[webView objectByEvaluatingJavaScript:@"clipboardData.length"] unsignedIntValue]);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"clipboardData[0]['image/png'].src"] containsString:@"blob:"]);
}

TEST(ClipboardTests, EphemeralSessionClipboardHasExpirationJavaScript)
{
    RetainPtr webView = createEphemeralWebViewForClipboardTests();

    __block bool pasteboardHaveExpirationDate = false;
    auto setExpirationDateSwizzler = InstanceMethodSwizzler {
        NSPasteboard.class,
        @selector(_setExpirationDate:),
        imp_implementationWithBlock(^{
            pasteboardHaveExpirationDate = true;
            return YES;
        })
    };

    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.execCommand(\"selectAll\", true); document.execCommand(\"copy\");"]);

    EXPECT_TRUE(pasteboardHaveExpirationDate);
}

TEST(ClipboardTests, EphemeralSessionClipboardHasExpirationWebView)
{
    RetainPtr webView = createEphemeralWebViewForClipboardTests();

    __block bool pasteboardHaveExpirationDate = false;
    auto setExpirationDateSwizzler = InstanceMethodSwizzler {
        NSPasteboard.class,
        @selector(_setExpirationDate:),
        imp_implementationWithBlock(^{
            pasteboardHaveExpirationDate = true;
            return YES;
        })
    };

    [webView selectAll:nil];
    [webView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    EXPECT_TRUE(pasteboardHaveExpirationDate);
}

#endif // PLATFORM(MAC)
