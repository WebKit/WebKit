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
#import <UIKit/NSString+UIItemProvider.h>
#import <UIKit/NSURL+UIItemProvider.h>
#import <UIKit/UIImage+UIItemProvider.h>
#import <UIKit/UIItemProvider_Private.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/_WKProcessPoolConfiguration.h>

typedef void (^FileLoadCompletionBlock)(NSURL *, BOOL, NSError *);
typedef void (^DataLoadCompletionBlock)(NSData *, NSError *);

static UIImage *testIconImage()
{
    return [UIImage imageNamed:@"TestWebKitAPI.resources/icon.png"];
}

@implementation UIItemProvider (DataInteractionTests)

- (void)registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier withData:(NSData *)data
{
    RetainPtr<NSData> retainedData = data;
    [self registerDataRepresentationForTypeIdentifier:typeIdentifier visibility:UIItemProviderRepresentationOptionsVisibilityAll loadHandler: [retainedData] (DataLoadCompletionBlock block) -> NSProgress * {
        block(retainedData.get(), nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
}

@end

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
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"autofocus-contenteditable"];
    [dataInteractionSimulator waitForInputSession];
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
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"contenteditable-and-textarea"];
    [dataInteractionSimulator waitForInputSession];
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

TEST(DataInteractionTests, ContentEditableMoveParagraphs)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"two-paragraph-contenteditable"];
    [dataInteractionSimulator waitForInputSession];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(250, 450)];

    NSString *finalTextContent = [webView stringByEvaluatingJavaScript:@"editor.textContent"];
    NSUInteger firstParagraphOffset = [finalTextContent rangeOfString:@"This is the first paragraph"].location;
    NSUInteger secondParagraphOffset = [finalTextContent rangeOfString:@"This is the second paragraph"].location;

    EXPECT_FALSE(firstParagraphOffset == NSNotFound);
    EXPECT_FALSE(secondParagraphOffset == NSNotFound);
    EXPECT_GT(firstParagraphOffset, secondParagraphOffset);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(190, 100, 130, 20), makeCGRectValue(0, 120, 320, 100), makeCGRectValue(0, 220, 252, 20) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, TextAreaToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"textarea-to-input"];
    [dataInteractionSimulator waitForInputSession];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 990, 232) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, SinglePlainTextWordTypeIdentifiers)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"textarea-to-input"];
    [dataInteractionSimulator waitForInputSession];
    [webView stringByEvaluatingJavaScript:@"source.value = 'pneumonoultramicroscopicsilicovolcanoconiosis'"];
    [webView stringByEvaluatingJavaScript:@"source.selectionStart = 0"];
    [webView stringByEvaluatingJavaScript:@"source.selectionEnd = source.value.length"];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSArray *registeredTypes = [[dataInteractionSimulator sourceItemProviders].firstObject registeredTypeIdentifiers];
    EXPECT_EQ(1UL, registeredTypes.count);
    EXPECT_WK_STREQ([(NSString *)kUTTypeUTF8PlainText UTF8String], [registeredTypes.firstObject UTF8String]);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_WK_STREQ("pneumonoultramicroscopicsilicovolcanoconiosis", [webView editorValue].UTF8String);
}

TEST(DataInteractionTests, SinglePlainTextURLTypeIdentifiers)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"textarea-to-input"];
    [dataInteractionSimulator waitForInputSession];
    [webView stringByEvaluatingJavaScript:@"source.value = 'https://webkit.org/'"];
    [webView stringByEvaluatingJavaScript:@"source.selectionStart = 0"];
    [webView stringByEvaluatingJavaScript:@"source.selectionEnd = source.value.length"];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSArray *registeredTypes = [[dataInteractionSimulator sourceItemProviders].firstObject registeredTypeIdentifiers];
    EXPECT_EQ(2UL, registeredTypes.count);
    EXPECT_WK_STREQ([(NSString *)kUTTypeURL UTF8String], [registeredTypes.firstObject UTF8String]);
    EXPECT_WK_STREQ([(NSString *)kUTTypeUTF8PlainText UTF8String], [registeredTypes.lastObject UTF8String]);
    EXPECT_EQ(0UL, [webView stringByEvaluatingJavaScript:@"source.value"].length);
    EXPECT_WK_STREQ("https://webkit.org/", [webView editorValue].UTF8String);
}

TEST(DataInteractionTests, LinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    __block bool doneLoadingURL = false;
    UIItemProvider *sourceItemProvider = [dataInteractionSimulator sourceItemProviders].firstObject;
    [sourceItemProvider loadObjectOfClass:[NSURL class] completionHandler:^(NSURL *url, NSError *error) {
        EXPECT_WK_STREQ("Hello world", url._title.UTF8String ?: "");
        doneLoadingURL = true;
    }];
    TestWebKitAPI::Util::run(&doneLoadingURL);

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

TEST(DataInteractionTests, ExternalSourceJSONToFileInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    RetainPtr<UIItemProvider> simulatedJSONItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *jsonData = [@"{ \"foo\": \"bar\",  \"bar\": \"baz\" }" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedJSONItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJSON withData:jsonData];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedJSONItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("application/json", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DataInteractionTests, ExternalSourceImageToFileInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    RetainPtr<UIItemProvider> simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedImageItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceHTMLToUploadArea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    RetainPtr<UIItemProvider> simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("text/html", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceImageAndHTMLToSingleFileInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    RetainPtr<UIItemProvider> simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    RetainPtr<UIItemProvider> simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get(), simulatedImageItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceImageAndHTMLToMultipleFileInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];
    [webView stringByEvaluatingJavaScript:@"input.setAttribute('multiple', '')"];

    RetainPtr<UIItemProvider> simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    RetainPtr<UIItemProvider> simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get(), simulatedImageItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg, text/html", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceImageAndHTMLToUploadArea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    RetainPtr<UIItemProvider> simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    RetainPtr<UIItemProvider> firstSimulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *firstHTMLData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [firstSimulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:firstHTMLData];

    RetainPtr<UIItemProvider> secondSimulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *secondHTMLData = [@"<html><body>hello world</body></html>" dataUsingEncoding:NSUTF8StringEncoding];
    [secondSimulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:secondHTMLData];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedImageItemProvider.get(), firstSimulatedHTMLItemProvider.get(), secondSimulatedHTMLItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg, text/html, text/html", outputValue.UTF8String);
}

TEST(DataInteractionTests, RespectsExternalSourceFidelityRankings)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    // Here, our source item provider vends two representations: plain text, and then an image. If we don't respect the
    // fidelity order requested by the source, we'll end up assuming that the image is a higher fidelity representation
    // than the plain text, and erroneously insert the image. If we respect source fidelities, we'll insert text rather
    // than an image.
    auto simulatedItemProviderWithTextFirst = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProviderWithTextFirst registerObject:@"Hello world" visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [simulatedItemProviderWithTextFirst registerObject:testIconImage() visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProviderWithTextFirst.get() ]];

    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    EXPECT_FALSE([webView editorContainsImageElement]);
    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = ''"];

    // Now we register the item providers in reverse, and expect the image to be inserted instead of text.
    auto simulatedItemProviderWithImageFirst = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProviderWithImageFirst registerObject:testIconImage() visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [simulatedItemProviderWithImageFirst registerObject:@"Hello world" visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProviderWithImageFirst.get() ]];

    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    EXPECT_TRUE([webView editorContainsImageElement]);
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
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ(textPayload.UTF8String, [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 1936, 227) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, ExternalSourceJPEGOnly)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    RetainPtr<UIItemProvider> simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock(UIImageJPEGRepresentation(testIconImage(), 0.5), nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_TRUE([webView editorContainsImageElement]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 215, 174) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, ExternalSourceTitledNSURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    NSURL *titledURL = [NSURL URLWithString:@"https://www.apple.com"];
    titledURL._title = @"Apple";
    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerObject:titledURL visibility:UIItemProviderRepresentationOptionsVisibilityAll];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("Apple", [webView stringByEvaluatingJavaScript:@"editor.querySelector('a').textContent"]);
    EXPECT_WK_STREQ("https://www.apple.com/", [webView stringByEvaluatingJavaScript:@"editor.querySelector('a').href"]);
}

TEST(DataInteractionTests, ExternalSourceFileURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    NSURL *URL = [NSURL URLWithString:@"file:///some/file/that/is/not/real"];
    UIItemProvider *simulatedItemProvider = [UIItemProvider itemProviderWithURL:URL title:@""];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"!!editor.querySelector('a')"] boolValue]);
    EXPECT_WK_STREQ("Hello world\nfile:///some/file/that/is/not/real", [webView stringByEvaluatingJavaScript:@"document.body.innerText"]);
}

TEST(DataInteractionTests, OverrideDataInteractionOperation)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    RetainPtr<UIItemProvider> simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:[@"<body></body>" dataUsingEncoding:NSUTF8StringEncoding]];

    __block bool finishedLoadingData = false;
    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator setOverrideDataInteractionOperationBlock:^NSUInteger(NSUInteger operation, id session)
    {
        EXPECT_EQ(0U, operation);
        return 1;
    }];
    [dataInteractionSimulator setDataInteractionOperationCompletionBlock:^(BOOL handled, NSArray *itemProviders) {
        EXPECT_FALSE(handled);
        [itemProviders.firstObject loadDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML completionHandler:^(NSData *data, NSError *error) {
            NSString *text = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
            EXPECT_WK_STREQ("<body></body>", text.UTF8String);
            EXPECT_FALSE(!!error);
            finishedLoadingData = true;
        }];
    }];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    TestWebKitAPI::Util::run(&finishedLoadingData);
}

TEST(DataInteractionTests, AttachmentElementItemProviders)
{
    RetainPtr<WKWebViewConfiguration> configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
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
