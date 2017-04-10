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

#include "config.h"

#if ENABLE(DATA_INTERACTION)

#import "DataInteractionSimulator.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIItemProvider_Private.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

@implementation TestWKWebView (DataInteractionTests)

- (BOOL)editorContainsImageElement
{
    return [self stringByEvaluatingJavaScript:@"!!editor.querySelector('img')"].boolValue;
}

- (NSString *)editorValue
{
    return [self stringByEvaluatingJavaScript:@"editor.value"];
}

@end

static NSValue *makeCGRectValue(CGFloat x, CGFloat y, CGFloat width, CGFloat height)
{
    return [NSValue valueWithCGRect:CGRectMake(x, y, width, height)];
}

static void checkSelectionRectsWithLogging(NSArray *expected, NSArray *observed)
{
    if (![expected isEqualToArray:observed])
        NSLog(@"Expected selection rects: %@ but observed: %@", expected, observed);
    EXPECT_TRUE([expected isEqualToArray:observed]);
}

static void checkTypeIdentifierPrecedesOtherTypeIdentifier(DataInteractionSimulator *simulator, NSString *firstType, NSString *secondType)
{
    NSArray *registeredTypes = [simulator.sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_TRUE([registeredTypes containsObject:firstType]);
    EXPECT_TRUE([registeredTypes containsObject:secondType]);
    EXPECT_TRUE([registeredTypes indexOfObject:firstType] < [registeredTypes indexOfObject:secondType]);
}

namespace TestWebKitAPI {

TEST(DataInteractionTests, ImageToContentEditable)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 215, 174) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypePNG, (NSString *)kUTTypeFileURL);
}

TEST(DataInteractionTests, ImageToTextarea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-textarea"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSURL *imageURL = [NSURL fileURLWithPath:[webView editorValue]];
    EXPECT_WK_STREQ("icon.png", imageURL.lastPathComponent);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);

    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypePNG, (NSString *)kUTTypeFileURL);
}

TEST(DataInteractionTests, ImageInLinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-in-link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 2057, 232) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, ImageInLinkWithoutHREFToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-in-link-and-input"];
    [webView stringByEvaluatingJavaScript:@"link.href = ''"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSURL *imageURL = [NSURL fileURLWithPath:[webView editorValue]];
    EXPECT_WK_STREQ("icon.png", imageURL.lastPathComponent);
}

TEST(DataInteractionTests, ContentEditableToContentEditable)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 961, 227) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypeRTFD, (NSString *)kUTTypeUTF8PlainText);
}

TEST(DataInteractionTests, ContentEditableToTextarea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-and-textarea"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(6, 203, 990, 232) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypeRTFD, (NSString *)kUTTypeUTF8PlainText);
}

TEST(DataInteractionTests, TextAreaToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"textarea-to-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 990, 232) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, LinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 273, 2057, 232) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypeURL, (NSString *)kUTTypeUTF8PlainText);
}

TEST(DataInteractionTests, BackgroundImageLinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"background-image-link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 2057, 232) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypeURL, (NSString *)kUTTypeUTF8PlainText);
}

TEST(DataInteractionTests, CanPreventStart)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-start"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ(DataInteractionCancelled, [dataInteractionSimulator phase]);
    EXPECT_FALSE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionOverEventName]);
    checkSelectionRectsWithLogging(@[ ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, CanPreventOperation)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-operation"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_FALSE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    checkSelectionRectsWithLogging(@[ ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, EnterAndLeaveEvents)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 450)];

    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionLeaveEventName]);
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, ExternalSourceUTF8PlainTextOnly)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    NSString *textPayload = @"Ceci n'est pas une string";
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    RetainPtr<UIItemProvider> simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([textPayload dataUsingEncoding:NSUTF8StringEncoding], nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
    [dataInteractionSimulator setExternalItemProvider:simulatedItemProvider.get()];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ(textPayload.UTF8String, [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 1936, 227) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, ExternalSourceJPEGOnly)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    UIImage *testImage = [UIImage imageNamed:@"TestWebKitAPI.resources/icon.png"];
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    RetainPtr<UIItemProvider> simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock(UIImageJPEGRepresentation(testImage, 0.5), nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
    [dataInteractionSimulator setExternalItemProvider:simulatedItemProvider.get()];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_TRUE([webView editorContainsImageElement]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 215, 174) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, AttachmentElementItemProviders)
{
    RetainPtr<WKWebViewConfiguration> configuration = [WKWebViewConfiguration testwebkitapi_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration _setAttachmentElementEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"attachment-element"];

    NSString *injectedTypeIdentifier = @"org.webkit.data";
    __block RetainPtr<NSString> injectedString;
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setConvertItemProvidersBlock:^NSArray *(NSArray *originalItemProviders)
    {
        for (UIItemProvider *provider in originalItemProviders) {
            NSData *injectedData = [provider copyDataRepresentationForTypeIdentifier:injectedTypeIdentifier error:nil];
            if (!injectedData.length)
                continue;
            injectedString = adoptNS([[NSString alloc] initWithData:injectedData encoding:NSUTF8StringEncoding]);
            break;
        }
        return originalItemProviders;
    }];

    [dataInteractionSimulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400)];

    EXPECT_WK_STREQ("hello", [injectedString UTF8String]);
}

TEST(DataInteractionTests, LargeImageToTargetDiv)
{
    auto testWebViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[testWebViewConfiguration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:testWebViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"div-and-large-image"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(200, 400) to:CGPointMake(200, 150)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"].UTF8String);
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), (NSString *)kUTTypePNG, (NSString *)kUTTypeFileURL);
}

TEST(DataInteractionTests, LinkWithEmptyHREF)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('a').href = ''"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ(DataInteractionCancelled, [dataInteractionSimulator phase]);
    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);
}

TEST(DataInteractionTests, CustomActionSheetPopover)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-target-div"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setShouldEnsureUIApplication:YES];

    __block BOOL didInvokeCustomActionSheet = NO;
    [dataInteractionSimulator setShowCustomActionSheetBlock:^BOOL(_WKActivatedElementInfo *element)
    {
        EXPECT_EQ(_WKActivatedElementTypeLink, element.type);
        EXPECT_WK_STREQ("Hello world", element.title.UTF8String);
        didInvokeCustomActionSheet = YES;
        return YES;
    }];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_TRUE(didInvokeCustomActionSheet);
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"].UTF8String);
}

TEST(DataInteractionTests, UnresponsivePageDoesNotHangUI)
{
    _WKProcessPoolConfiguration *processPoolConfiguration = [[[_WKProcessPoolConfiguration alloc] init] autorelease];
    processPoolConfiguration.ignoreSynchronousMessagingTimeoutsForTesting = YES;

    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:[[[WKWebViewConfiguration alloc] init] autorelease] processPoolConfiguration:processPoolConfiguration]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView evaluateJavaScript:@"while(1);" completionHandler:nil];

    // The test passes if we can prepare for data interaction without timing out.
    [webView _simulatePrepareForDataInteractionSession:nil completion:^() { }];
}

} // namespace TestWebKitAPI

#endif // ENABLE(DATA_INTERACTION)
