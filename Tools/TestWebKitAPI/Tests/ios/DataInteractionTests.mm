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
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIItemProvider_Private.h>

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
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(1, 201, 215, 174) ]]);
}

TEST(DataInteractionTests, ImageToTextarea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-textarea"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSURL *imageURL = [NSURL fileURLWithPath:[webView editorValue]];
    EXPECT_WK_STREQ("icon.png", imageURL.lastPathComponent);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);

    NSArray *expectedSelectionRects = [NSArray arrayWithObjects:makeCGRectValue(6, 203, 188, 14), makeCGRectValue(6, 217, 188, 14), makeCGRectValue(6, 231, 66, 14), nil];
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:expectedSelectionRects]);
}

TEST(DataInteractionTests, ContentEditableToContentEditable)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(1, 201, 961, 227) ]]);
}

TEST(DataInteractionTests, ContentEditableToTextarea)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-and-textarea"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(6, 203, 990, 232) ]]);
}

TEST(DataInteractionTests, LinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.daringfireball.net/", [webView editorValue].UTF8String);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(101, 273, 2613, 232) ]]);
}

TEST(DataInteractionTests, BackgroundImageLinkToInput)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"background-image-link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.daringfireball.net/", [webView editorValue].UTF8String);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(101, 241, 2613, 232) ]]);
}

TEST(DataInteractionTests, CanPreventStart)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-start"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_FALSE([webView editorContainsImageElement]);
    EXPECT_FALSE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ ]]);
}

TEST(DataInteractionTests, CanPreventOperation)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-operation"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_FALSE([webView editorContainsImageElement]);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ ]]);
}

TEST(DataInteractionTests, EnterAndLeaveEvents)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 450)];

    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);
    EXPECT_TRUE([dataInteractionSimulator didTryToBeginDataInteraction]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionLeaveEventName]);
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ ]]);
}

TEST(DataInteractionTests, HandlesDataInteractionFailureGracefully)
{
    RetainPtr<TestWKWebView> webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    RetainPtr<DataInteractionSimulator> dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setForceRequestToFail:YES];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);

    // Before r212266, starting a subsequent gesture would have caused a debug assertion in the web process.
    [dataInteractionSimulator setForceRequestToFail:NO];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("https://www.daringfireball.net/", [webView editorValue].UTF8String);
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
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(1, 201, 1936, 227) ]]);
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
    EXPECT_TRUE([[dataInteractionSimulator finalSelectionRects] isEqualToArray:@[ makeCGRectValue(1, 201, 215, 174) ]]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(DATA_INTERACTION)
