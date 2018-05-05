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
#import "UIKitSPI.h"
#import "WKWebViewConfigurationExtras.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebItemProviderPasteboard.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/Seconds.h>

typedef void (^FileLoadCompletionBlock)(NSURL *, BOOL, NSError *);
typedef void (^DataLoadCompletionBlock)(NSData *, NSError *);
typedef void (^UIItemProviderDataLoadCompletionBlock)(NSData *, NSError *);

#if !USE(APPLE_INTERNAL_SDK)

@interface UIItemProviderRepresentationOptions : NSObject
@end

#endif

@interface UIItemProvider()
+ (UIItemProvider *)itemProviderWithURL:(NSURL *)url title:(NSString *)title;
- (void) registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier options:(UIItemProviderRepresentationOptions*)options loadHandler:(NSProgress * (^)(void (^UIItemProviderDataLoadCompletionBlock)(NSData *item, NSError *error))) loadHandler;
@end

static NSString *InjectedBundlePasteboardDataType = @"org.webkit.data";

static UIImage *testIconImage()
{
    return [UIImage imageNamed:@"TestWebKitAPI.resources/icon.png"];
}

static NSData *testZIPArchive()
{
    NSURL *zipFileURL = [[NSBundle mainBundle] URLForResource:@"compressed-files" withExtension:@"zip" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:zipFileURL];
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

static void checkCGRectIsEqualToCGRectWithLogging(CGRect expected, CGRect observed)
{
    BOOL isEqual = CGRectEqualToRect(expected, observed);
    EXPECT_TRUE(isEqual);
    if (!isEqual)
        NSLog(@"Expected: %@ but observed: %@", NSStringFromCGRect(expected), NSStringFromCGRect(observed));
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

static void checkFirstTypeIsPresentAndSecondTypeIsMissing(DataInteractionSimulator *simulator, CFStringRef firstType, CFStringRef secondType)
{
    NSArray *registeredTypes = [simulator.sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_TRUE([registeredTypes containsObject:(NSString *)firstType]);
    EXPECT_FALSE([registeredTypes containsObject:(NSString *)secondType]);
}

static void checkTypeIdentifierIsRegisteredAtIndex(DataInteractionSimulator *simulator, NSString *type, NSUInteger index)
{
    NSArray *registeredTypes = [simulator.sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_GT(registeredTypes.count, index);
    EXPECT_WK_STREQ(type.UTF8String, [registeredTypes[index] UTF8String]);
}

static void checkEstimatedSize(DataInteractionSimulator *simulator, CGSize estimatedSize)
{
    UIItemProvider *sourceItemProvider = [simulator sourceItemProviders].firstObject;
    EXPECT_EQ(estimatedSize.width, sourceItemProvider.preferredPresentationSize.width);
    EXPECT_EQ(estimatedSize.height, sourceItemProvider.preferredPresentationSize.height);
}

static void checkSuggestedNameAndEstimatedSize(DataInteractionSimulator *simulator, NSString *suggestedName, CGSize estimatedSize)
{
    UIItemProvider *sourceItemProvider = [simulator sourceItemProviders].firstObject;
    EXPECT_WK_STREQ(suggestedName.UTF8String, sourceItemProvider.suggestedName.UTF8String);
    EXPECT_EQ(estimatedSize.width, sourceItemProvider.preferredPresentationSize.width);
    EXPECT_EQ(estimatedSize.height, sourceItemProvider.preferredPresentationSize.height);
}

static void checkStringArraysAreEqual(NSArray<NSString *> *expected, NSArray<NSString *> *observed)
{
    EXPECT_EQ(expected.count, observed.count);
    for (NSUInteger index = 0; index < expected.count; ++index) {
        NSString *expectedString = [expected objectAtIndex:index];
        NSString *observedString = [observed objectAtIndex:index];
        EXPECT_WK_STREQ(expectedString, observedString);
        if (![expectedString isEqualToString:observedString])
            NSLog(@"Expected observed string: %@ to match expected string: %@ at index: %tu", observedString, expectedString, index);
    }
}

static void checkDragCaretRectIsContainedInRect(CGRect caretRect, CGRect containerRect)
{
    BOOL contained = CGRectContainsRect(containerRect, caretRect);
    EXPECT_TRUE(contained);
    if (!contained)
        NSLog(@"Expected caret rect: %@ to fit within container rect: %@", NSStringFromCGRect(caretRect), NSStringFromCGRect(containerRect));
}

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110300

static void checkJSONWithLogging(NSString *jsonString, NSDictionary *expected)
{
    BOOL success = TestWebKitAPI::Util::jsonMatchesExpectedValues(jsonString, expected);
    EXPECT_TRUE(success);
    if (!success)
        NSLog(@"Expected JSON: %@ to match values: %@", jsonString, expected);
}

static NSData *testIconImageData()
{
    return [NSData dataWithContentsOfURL:[[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"]];
}

static void runTestWithTemporaryTextFile(void(^runTest)(NSURL *fileURL))
{
    NSString *fileName = [NSString stringWithFormat:@"drag-drop-text-file-%@.txt", [NSUUID UUID].UUIDString];
    RetainPtr<NSURL> temporaryFile = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:fileName] isDirectory:NO];
    [[NSFileManager defaultManager] removeItemAtURL:temporaryFile.get() error:nil];

    NSError *error = nil;
    [@"This is a tiny blob of text." writeToURL:temporaryFile.get() atomically:YES encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error writing temporary file: %@", error);

    @try {
        runTest(temporaryFile.get());
    } @finally {
        [[NSFileManager defaultManager] removeItemAtURL:temporaryFile.get() error:nil];
    }
}

static void runTestWithTemporaryFolder(void(^runTest)(NSURL *folderURL))
{
    NSString *folderName = [NSString stringWithFormat:@"some.directory-%@", [NSUUID UUID].UUIDString];
    RetainPtr<NSURL> temporaryFolder = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:folderName] isDirectory:YES];
    [[NSFileManager defaultManager] removeItemAtURL:temporaryFolder.get() error:nil];

    NSError *error = nil;
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    [defaultManager createDirectoryAtURL:temporaryFolder.get() withIntermediateDirectories:NO attributes:nil error:&error];
    [testIconImageData() writeToURL:[temporaryFolder.get() URLByAppendingPathComponent:@"icon.png" isDirectory:NO] atomically:YES];
    [testZIPArchive() writeToURL:[temporaryFolder.get() URLByAppendingPathComponent:@"archive.zip" isDirectory:NO] atomically:YES];

    NSURL *firstSubdirectory = [temporaryFolder.get() URLByAppendingPathComponent:@"subdirectory1" isDirectory:YES];
    [defaultManager createDirectoryAtURL:firstSubdirectory withIntermediateDirectories:NO attributes:nil error:&error];
    [@"I am a text file in the first subdirectory." writeToURL:[firstSubdirectory URLByAppendingPathComponent:@"text-file-1.txt" isDirectory:NO] atomically:YES encoding:NSUTF8StringEncoding error:&error];

    NSURL *secondSubdirectory = [temporaryFolder.get() URLByAppendingPathComponent:@"subdirectory2" isDirectory:YES];
    [defaultManager createDirectoryAtURL:secondSubdirectory withIntermediateDirectories:NO attributes:nil error:&error];
    [@"I am a text file in the second subdirectory." writeToURL:[secondSubdirectory URLByAppendingPathComponent:@"text-file-2.txt" isDirectory:NO] atomically:YES encoding:NSUTF8StringEncoding error:&error];

    if (error)
        NSLog(@"Error writing temporary file: %@", error);

    @try {
        runTest(temporaryFolder.get());
    } @finally {
        [[NSFileManager defaultManager] removeItemAtURL:temporaryFolder.get() error:nil];
    }
}

#endif // __IPHONE_OS_VERSION_MIN_REQUIRED >= 110300

namespace TestWebKitAPI {

TEST(DataInteractionTests, ImageToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 215, 174) ], [dataInteractionSimulator finalSelectionRects]);
    checkFirstTypeIsPresentAndSecondTypeIsMissing(dataInteractionSimulator.get(), kUTTypePNG, kUTTypeFileURL);
    checkEstimatedSize(dataInteractionSimulator.get(), { 215, 174 });
}

TEST(DataInteractionTests, CanStartDragOnEnormousImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<img src='enormous.svg'></img>"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 100)];

    NSArray *registeredTypes = [[dataInteractionSimulator sourceItemProviders].firstObject registeredTypeIdentifiers];
    EXPECT_WK_STREQ((NSString *)kUTTypeScalableVectorGraphics, [registeredTypes firstObject]);
}

TEST(DataInteractionTests, ImageToTextarea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-textarea"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView editorValue]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkFirstTypeIsPresentAndSecondTypeIsMissing(dataInteractionSimulator.get(), kUTTypePNG, kUTTypeFileURL);
    checkEstimatedSize(dataInteractionSimulator.get(), { 215, 174 });
}

TEST(DataInteractionTests, ImageInLinkToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-in-link-and-input"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 2057, 232) ], [dataInteractionSimulator finalSelectionRects]);
    checkSuggestedNameAndEstimatedSize(dataInteractionSimulator.get(), @"icon.png", { 215, 174 });
    checkTypeIdentifierIsRegisteredAtIndex(dataInteractionSimulator.get(), (NSString *)kUTTypePNG, 0);
}

TEST(DataInteractionTests, ImageInLinkWithoutHREFToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-in-link-and-input"];
    [webView stringByEvaluatingJavaScript:@"link.href = ''"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView editorValue]);
    checkEstimatedSize(dataInteractionSimulator.get(), { 215, 174 });
    checkTypeIdentifierIsRegisteredAtIndex(dataInteractionSimulator.get(), (NSString *)kUTTypePNG, 0);
}

TEST(DataInteractionTests, ImageDoesNotUseElementSizeAsEstimatedSize)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"gif-and-file-input"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom: { 100, 100 } to: { 100, 300 }];

    checkTypeIdentifierIsRegisteredAtIndex(dataInteractionSimulator.get(), (NSString *)kUTTypeGIF, 0);
    checkSuggestedNameAndEstimatedSize(dataInteractionSimulator.get(), @"apple.gif", { 52, 64 });
    EXPECT_WK_STREQ("apple.gif (image/gif)", [webView stringByEvaluatingJavaScript:@"output.textContent"]);
}

TEST(DataInteractionTests, ContentEditableToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

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
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000
    NSString *richTextTypeIdentifier = (NSString *)kUTTypeRTF;
#else
    NSString *richTextTypeIdentifier = (NSString *)kUTTypeRTFD;
#endif
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), richTextTypeIdentifier, (NSString *)kUTTypeUTF8PlainText);
}

TEST(DataInteractionTests, ContentEditableToTextarea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

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
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 120000
    NSString *richTextTypeIdentifier = (NSString *)kUTTypeRTF;
#else
    NSString *richTextTypeIdentifier = (NSString *)kUTTypeRTFD;
#endif
    checkTypeIdentifierPrecedesOtherTypeIdentifier(dataInteractionSimulator.get(), richTextTypeIdentifier, (NSString *)kUTTypeUTF8PlainText);
}

TEST(DataInteractionTests, ContentEditableMoveParagraphs)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

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

TEST(DataInteractionTests, DragImageFromContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView synchronouslyLoadTestPageNamed:@"contenteditable-and-target"];
    [dataInteractionSimulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
}

TEST(DataInteractionTests, TextAreaToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"textarea-to-input"];
    [dataInteractionSimulator waitForInputSession];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 990, 232) ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, SinglePlainTextWordTypeIdentifiers)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"textarea-to-input"];
    [dataInteractionSimulator waitForInputSession];
    [webView stringByEvaluatingJavaScript:@"source.value = 'pneumonoultramicroscopicsilicovolcanoconiosis'"];
    [webView stringByEvaluatingJavaScript:@"source.selectionStart = 0"];
    [webView stringByEvaluatingJavaScript:@"source.selectionEnd = source.value.length"];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSItemProvider *itemProvider = [dataInteractionSimulator sourceItemProviders].firstObject;
    NSArray *registeredTypes = [itemProvider registeredTypeIdentifiers];
    EXPECT_EQ(1UL, registeredTypes.count);
    EXPECT_WK_STREQ([(NSString *)kUTTypeUTF8PlainText UTF8String], [registeredTypes.firstObject UTF8String]);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_EQ(UIPreferredPresentationStyleInline, itemProvider.preferredPresentationStyle);
    EXPECT_WK_STREQ("pneumonoultramicroscopicsilicovolcanoconiosis", [webView editorValue].UTF8String);
}

TEST(DataInteractionTests, SinglePlainTextURLTypeIdentifiers)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView loadTestPageNamed:@"textarea-to-input"];
    [dataInteractionSimulator waitForInputSession];
    [webView stringByEvaluatingJavaScript:@"source.value = 'https://webkit.org/'"];
    [webView stringByEvaluatingJavaScript:@"source.selectionStart = 0"];
    [webView stringByEvaluatingJavaScript:@"source.selectionEnd = source.value.length"];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    NSItemProvider *itemProvider = [dataInteractionSimulator sourceItemProviders].firstObject;
    NSArray *registeredTypes = [itemProvider registeredTypeIdentifiers];
    EXPECT_EQ(2UL, registeredTypes.count);
    EXPECT_WK_STREQ([(NSString *)kUTTypeURL UTF8String], [registeredTypes.firstObject UTF8String]);
    EXPECT_WK_STREQ([(NSString *)kUTTypeUTF8PlainText UTF8String], [registeredTypes.lastObject UTF8String]);
    EXPECT_EQ(0UL, [webView stringByEvaluatingJavaScript:@"source.value"].length);
    EXPECT_EQ(UIPreferredPresentationStyleInline, itemProvider.preferredPresentationStyle);
    EXPECT_WK_STREQ("https://webkit.org/", [webView editorValue].UTF8String);
}

TEST(DataInteractionTests, LinkToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    __block bool doneLoadingURL = false;
    UIItemProvider *sourceItemProvider = [dataInteractionSimulator sourceItemProviders].firstObject;
    [sourceItemProvider loadObjectOfClass:[NSURL class] completionHandler:^(id object, NSError *error) {
        NSURL *url = object;
        EXPECT_WK_STREQ("Hello world", url._title.UTF8String ?: "");
        doneLoadingURL = true;
    }];
    TestWebKitAPI::Util::run(&doneLoadingURL);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 273, 2057, 232) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierIsRegisteredAtIndex(dataInteractionSimulator.get(), (NSString *)kUTTypeURL, 0);
}

TEST(DataInteractionTests, BackgroundImageLinkToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"background-image-link-and-input"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 2057, 232) ], [dataInteractionSimulator finalSelectionRects]);
    checkTypeIdentifierIsRegisteredAtIndex(dataInteractionSimulator.get(), (NSString *)kUTTypeURL, 0);
}

TEST(DataInteractionTests, CanPreventStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-start"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-operation"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_FALSE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    checkSelectionRectsWithLogging(@[ ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, EnterAndLeaveEvents)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 450)];

    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [dataInteractionSimulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionEnterEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionOverEventName]);
    EXPECT_TRUE([observedEventNames containsObject:DataInteractionLeaveEventName]);
    EXPECT_FALSE([observedEventNames containsObject:DataInteractionPerformOperationEventName]);
    checkSelectionRectsWithLogging(@[ ], [dataInteractionSimulator finalSelectionRects]);
}

TEST(DataInteractionTests, CanStartDragOnDivWithDraggableAttribute)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"custom-draggable-div"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 250)];

    EXPECT_GT([dataInteractionSimulator sourceItemProviders].count, 0UL);
    NSItemProvider *itemProvider = [dataInteractionSimulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleInline, itemProvider.preferredPresentationStyle);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!destination.querySelector('#item')"]);
    EXPECT_WK_STREQ(@"PASS", [webView stringByEvaluatingJavaScript:@"item.textContent"]);
}

TEST(DataInteractionTests, ExternalSourcePlainTextToIFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-in-iframe"];

    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    [itemProvider registerObject:@"Hello world" visibility:UIItemProviderRepresentationOptionsVisibilityAll];

    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(160, 250)];

    auto containerLeft = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().left"].floatValue;
    auto containerTop = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().top"].floatValue;
    auto containerWidth = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().width"].floatValue;
    auto containerHeight = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().height"].floatValue;
    checkDragCaretRectIsContainedInRect([simulator lastKnownDragCaretRect], CGRectMake(containerLeft, containerTop, containerWidth, containerHeight));
}

TEST(DataInteractionTests, ExternalSourceInlineTextToFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider setPreferredPresentationStyle:UIPreferredPresentationStyleInline];
    [simulatedItemProvider registerObject:@"This item provider requested inline presentation style." visibility:NSItemProviderRepresentationVisibilityAll];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DataInteractionTests, ExternalSourceJSONToFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedJSONItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *jsonData = [@"{ \"foo\": \"bar\",  \"bar\": \"baz\" }" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedJSONItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJSON withData:jsonData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedJSONItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("application/json", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DataInteractionTests, ExternalSourceImageToFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedImageItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceHTMLToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setShouldAllowMoveOperation:NO];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("text/html", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceMoveOperationNotAllowed)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];
    [webView stringByEvaluatingJavaScript:@"upload.dropEffect = 'move'"];

    auto simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setShouldAllowMoveOperation:NO];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DataInteractionTests, ExternalSourceZIPArchiveAndURLToSingleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto archiveProvider = adoptNS([[UIItemProvider alloc] init]);
    [archiveProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeZipArchive withData:testZIPArchive()];

    auto urlProvider = adoptNS([[UIItemProvider alloc] init]);
    [urlProvider registerObject:[NSURL URLWithString:@"https://webkit.org"] visibility:UIItemProviderRepresentationOptionsVisibilityAll];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ archiveProvider.get(), urlProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("application/zip", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceZIPArchiveToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeZipArchive withData:testZIPArchive()];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ itemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("application/zip", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceImageAndHTMLToSingleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    auto simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get(), simulatedImageItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceImageAndHTMLToMultipleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];
    [webView stringByEvaluatingJavaScript:@"input.setAttribute('multiple', '')"];

    auto simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    auto simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get(), simulatedImageItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg, text/html", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceImageAndHTMLToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    auto firstSimulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *firstHTMLData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [firstSimulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:firstHTMLData];

    auto secondSimulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *secondHTMLData = [@"<html><body>hello world</body></html>" dataUsingEncoding:NSUTF8StringEncoding];
    [secondSimulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:secondHTMLData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setShouldAllowMoveOperation:NO];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedImageItemProvider.get(), firstSimulatedHTMLItemProvider.get(), secondSimulatedHTMLItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg, text/html, text/html", outputValue.UTF8String);
}

TEST(DataInteractionTests, ExternalSourceHTMLToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *htmlData = [@"<h1>This is a test</h1>" dataUsingEncoding:NSUTF8StringEncoding];
    [itemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:htmlData];
    [dataInteractionSimulator setExternalItemProviders:@[ itemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    NSString *textContent = [webView stringByEvaluatingJavaScript:@"editor.textContent"];
    EXPECT_WK_STREQ("This is a test", textContent.UTF8String);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!editor.querySelector('h1')"].boolValue);
}

TEST(DataInteractionTests, ExternalSourceBoldSystemAttributedStringToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    NSDictionary *textAttributes = @{ NSFontAttributeName: [UIFont boldSystemFontOfSize:20] };
    NSAttributedString *richText = [[NSAttributedString alloc] initWithString:@"This is a test" attributes:textAttributes];
    auto itemProvider = adoptNS([[UIItemProvider alloc] initWithObject:richText]);
    [dataInteractionSimulator setExternalItemProviders:@[ itemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("This is a test", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
}

TEST(DataInteractionTests, ExternalSourceColoredAttributedStringToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    NSDictionary *textAttributes = @{ NSForegroundColorAttributeName: [UIColor redColor] };
    NSAttributedString *richText = [[NSAttributedString alloc] initWithString:@"This is a test" attributes:textAttributes];
    auto itemProvider = adoptNS([[UIItemProvider alloc] initWithObject:richText]);
    [dataInteractionSimulator setExternalItemProviders:@[ itemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('p')).color"]);
    EXPECT_WK_STREQ("This is a test", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
}

TEST(DataInteractionTests, ExternalSourceMultipleURLsToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto firstItem = adoptNS([[UIItemProvider alloc] init]);
    [firstItem registerObject:[NSURL URLWithString:@"https://www.apple.com/iphone/"] visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    auto secondItem = adoptNS([[UIItemProvider alloc] init]);
    [secondItem registerObject:[NSURL URLWithString:@"https://www.apple.com/mac/"] visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    auto thirdItem = adoptNS([[UIItemProvider alloc] init]);
    [thirdItem registerObject:[NSURL URLWithString:@"https://webkit.org/"] visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [dataInteractionSimulator setExternalItemProviders:@[ firstItem.get(), secondItem.get(), thirdItem.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    NSArray *droppedURLs = [webView objectByEvaluatingJavaScript:@"Array.from(editor.querySelectorAll('a')).map(a => a.href)"];
    EXPECT_EQ(3UL, droppedURLs.count);
    EXPECT_WK_STREQ("https://www.apple.com/iphone/", droppedURLs[0]);
    EXPECT_WK_STREQ("https://www.apple.com/mac/", droppedURLs[1]);
    EXPECT_WK_STREQ("https://webkit.org/", droppedURLs[2]);

    NSArray *linksSeparatedByLine = [[webView objectByEvaluatingJavaScript:@"editor.innerText"] componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]];
    EXPECT_EQ(3UL, linksSeparatedByLine.count);
    EXPECT_WK_STREQ("https://www.apple.com/iphone/", linksSeparatedByLine[0]);
    EXPECT_WK_STREQ("https://www.apple.com/mac/", linksSeparatedByLine[1]);
    EXPECT_WK_STREQ("https://webkit.org/", linksSeparatedByLine[2]);
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    NSString *textPayload = @"Ceci n'est pas une string";
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
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
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
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

TEST(DataInteractionTests, ExternalSourceOverrideDropFileUpload)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG withData:imageData];

    auto simulatedHTMLItemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSData *firstHTMLData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:firstHTMLData];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(2UL, session.items.count);
        UIDragItem *firstItem = session.items[0];
        UIDragItem *secondItem = session.items[1];
        EXPECT_TRUE([firstItem.itemProvider.registeredTypeIdentifiers isEqual:@[ (NSString *)kUTTypeJPEG ]]);
        EXPECT_TRUE([secondItem.itemProvider.registeredTypeIdentifiers isEqual:@[ (NSString *)kUTTypeHTML ]]);
        return @[ secondItem ];
    }];
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedImageItemProvider.get(), simulatedHTMLItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("text/html", outputValue.UTF8String);
}

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110300

static RetainPtr<TestWKWebView> setUpTestWebViewForDataTransferItems()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransferItem-getAsEntry"];

    auto preferences = (WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetDirectoryUploadEnabled(preferences, true);

    return webView;
}

TEST(DataInteractionTests, ExternalSourceDataTransferItemGetFolderAsEntry)
{
    // The expected output is sorted by alphabetical order here for consistent behavior across different test environments.
    // See DataTransferItem-getAsEntry.html for more details.
    NSArray<NSString *> *expectedOutput = @[
        @"Found data transfer item (kind: 'file', type: '')",
        @"DIR: /somedirectory",
        @"DIR: /somedirectory/subdirectory1",
        @"DIR: /somedirectory/subdirectory2",
        [NSString stringWithFormat:@"FILE: /somedirectory/archive.zip ('application/zip', %tu bytes)", testZIPArchive().length],
        [NSString stringWithFormat:@"FILE: /somedirectory/icon.png ('image/png', %tu bytes)", testIconImageData().length],
        @"FILE: /somedirectory/subdirectory1/text-file-1.txt ('text/plain', 43 bytes)",
        @"FILE: /somedirectory/subdirectory2/text-file-2.txt ('text/plain', 44 bytes)"
    ];

    auto webView = setUpTestWebViewForDataTransferItems();
    __block bool done = false;
    [webView performAfterReceivingMessage:@"dropped" action:^() {
        done = true;
    }];

    runTestWithTemporaryFolder(^(NSURL *folderURL) {
        auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
        [itemProvider setSuggestedName:@"somedirectory"];
        [itemProvider registerFileRepresentationForTypeIdentifier:(NSString *)kUTTypeFolder fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[capturedFolderURL = retainPtr(folderURL)] (FileLoadCompletionBlock completionHandler) -> NSProgress * {
            completionHandler(capturedFolderURL.get(), NO, nil);
            return nil;
        }];

        auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
        [dataInteractionSimulator setExternalItemProviders:@[ itemProvider.get() ]];
        [dataInteractionSimulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    });

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ([expectedOutput componentsJoinedByString:@"\n"], [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DataInteractionTests, ExternalSourceDataTransferItemGetPlainTextFileAsEntry)
{
    NSArray<NSString *> *expectedOutput = @[
        @"Found data transfer item (kind: 'file', type: 'text/plain')",
        @"FILE: /foo.txt ('text/plain', 28 bytes)"
    ];

    auto webView = setUpTestWebViewForDataTransferItems();
    __block bool done = false;
    [webView performAfterReceivingMessage:@"dropped" action:^() {
        done = true;
    }];

    runTestWithTemporaryTextFile(^(NSURL *fileURL) {
        auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
        [itemProvider setSuggestedName:@"foo"];
        [itemProvider registerFileRepresentationForTypeIdentifier:(NSString *)kUTTypeUTF8PlainText fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[capturedFileURL = retainPtr(fileURL)](FileLoadCompletionBlock completionHandler) -> NSProgress * {
            completionHandler(capturedFileURL.get(), NO, nil);
            return nil;
        }];

        auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
        [dataInteractionSimulator setExternalItemProviders:@[ itemProvider.get() ]];
        [dataInteractionSimulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    });

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ([expectedOutput componentsJoinedByString:@"\n"], [webView stringByEvaluatingJavaScript:@"output.value"]);
}

#endif // __IPHONE_OS_VERSION_MIN_REQUIRED >= 110300

TEST(DataInteractionTests, ExternalSourceOverrideDropInsertURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        NSMutableArray<UIDragItem *> *allowedItems = [NSMutableArray array];
        for (UIDragItem *item in session.items) {
            if ([item.itemProvider.registeredTypeIdentifiers containsObject:(NSString *)kUTTypeURL])
                [allowedItems addObject:item];
        }
        EXPECT_EQ(1UL, allowedItems.count);
        return allowedItems;
    }];

    auto firstItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [firstItemProvider registerObject:@"This is a string." visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    auto secondItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [secondItemProvider registerObject:[NSURL URLWithString:@"https://webkit.org/"] visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [dataInteractionSimulator setExternalItemProviders:@[ firstItemProvider.get(), secondItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://webkit.org/", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(DataInteractionTests, OverrideDataInteractionOperation)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:[@"<body></body>" dataUsingEncoding:NSUTF8StringEncoding]];

    __block bool finishedLoadingData = false;
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator setOverrideDataInteractionOperationBlock:^NSUInteger(NSUInteger operation, id session)
    {
        EXPECT_EQ(0U, operation);
        return UIDropOperationCopy;
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

TEST(DataInteractionTests, InjectedBundleOverridePerformTwoStepDrop)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration.processPool _setObject:@YES forBundleParameter:@"BundleOverridePerformTwoStepDrop"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView loadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeUTF8PlainText options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([@"Hello world" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_EQ(0UL, [webView stringByEvaluatingJavaScript:@"editor.textContent"].length);
}

TEST(DataInteractionTests, InjectedBundleAllowPerformTwoStepDrop)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration.processPool _setObject:@NO forBundleParameter:@"BundleOverridePerformTwoStepDrop"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulatedItemProvider = adoptNS([[UIItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeUTF8PlainText options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([@"Hello world" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [dataInteractionSimulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
}

TEST(DataInteractionTests, InjectedBundleImageElementData)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration _setAttachmentElementEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    __block RetainPtr<NSString> injectedString;
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setConvertItemProvidersBlock:^NSArray *(UIItemProvider *itemProvider, NSArray *, NSDictionary *data)
    {
        injectedString = adoptNS([[NSString alloc] initWithData:data[InjectedBundlePasteboardDataType] encoding:NSUTF8StringEncoding]);
        return @[ itemProvider ];
    }];

    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 250)];

    EXPECT_WK_STREQ("hello", [injectedString UTF8String]);
}

TEST(DataInteractionTests, InjectedBundleAttachmentElementData)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration _setAttachmentElementEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"attachment-element"];

    __block RetainPtr<NSString> injectedString;
    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setConvertItemProvidersBlock:^NSArray *(UIItemProvider *itemProvider, NSArray *, NSDictionary *data)
    {
        injectedString = adoptNS([[NSString alloc] initWithData:data[InjectedBundlePasteboardDataType] encoding:NSUTF8StringEncoding]);
        return @[ itemProvider ];
    }];

    [dataInteractionSimulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400)];

    EXPECT_WK_STREQ("hello", [injectedString UTF8String]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
}

TEST(DataInteractionTests, LargeImageToTargetDiv)
{
    auto testWebViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[testWebViewConfiguration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:testWebViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"div-and-large-image"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(200, 400) to:CGPointMake(200, 150)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"].UTF8String);
    checkFirstTypeIsPresentAndSecondTypeIsMissing(dataInteractionSimulator.get(), kUTTypePNG, kUTTypeFileURL);
    checkEstimatedSize(dataInteractionSimulator.get(), { 2000, 2000 });
}

TEST(DataInteractionTests, LinkWithEmptyHREF)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('a').href = ''"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ(DataInteractionCancelled, [dataInteractionSimulator phase]);
    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);
}

TEST(DataInteractionTests, CancelledLiftDoesNotCauseSubsequentDragsToFail)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-target-div"];

    auto dataInteractionSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [dataInteractionSimulator setConvertItemProvidersBlock:^NSArray *(UIItemProvider *, NSArray *, NSDictionary *)
    {
        return @[ ];
    }];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_EQ(DataInteractionCancelled, [dataInteractionSimulator phase]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    NSString *outputText = [webView stringByEvaluatingJavaScript:@"output.textContent"];
    checkStringArraysAreEqual(@[@"dragstart", @"dragend"], [outputText componentsSeparatedByString:@" "]);

    [webView stringByEvaluatingJavaScript:@"output.innerHTML = ''"];
    [dataInteractionSimulator setConvertItemProvidersBlock:^NSArray *(UIItemProvider *itemProvider, NSArray *, NSDictionary *)
    {
        return @[ itemProvider ];
    }];
    [dataInteractionSimulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    [webView stringByEvaluatingJavaScript:@"output.textContent"];
    checkStringArraysAreEqual(@[@"dragstart", @"dragend"], [outputText componentsSeparatedByString:@" "]);
}

static void testDragAndDropOntoTargetElements(TestWKWebView *webView)
{
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView]);
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 250)];
    EXPECT_WK_STREQ("rgb(0, 128, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(target1).backgroundColor"]);
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target1.textContent"]);

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(250, 50)];
    EXPECT_WK_STREQ("rgb(0, 128, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(target2).backgroundColor"]);
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target2.textContent"]);

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(250, 250)];
    EXPECT_WK_STREQ("rgb(0, 128, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(target3).backgroundColor"]);
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target3.textContent"]);
}

TEST(DataInteractionTests, DragEventClientCoordinatesBasic)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DataInteractionTests, DragEventClientCoordinatesWithScrollOffset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];
    [webView stringByEvaluatingJavaScript:@"document.body.style.margin = '1000px 0'"];
    [webView stringByEvaluatingJavaScript:@"document.scrollingElement.scrollTop = 1000"];
    [webView waitForNextPresentationUpdate];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DataInteractionTests, DragEventPageCoordinatesBasic)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];
    [webView stringByEvaluatingJavaScript:@"window.usePageCoordinates = true"];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DataInteractionTests, DragEventPageCoordinatesWithScrollOffset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];
    [webView stringByEvaluatingJavaScript:@"document.body.style.margin = '1000px 0'"];
    [webView stringByEvaluatingJavaScript:@"document.scrollingElement.scrollTop = 1000"];
    [webView stringByEvaluatingJavaScript:@"window.usePageCoordinates = true"];
    [webView waitForNextPresentationUpdate];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DataInteractionTests, DoNotCrashWhenSelectionIsClearedInDragStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dragstart-clear-selection"];

    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"paragraph.textContent"]);
}

TEST(DataInteractionTests, CustomActionSheetPopover)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
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

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:[[[WKWebViewConfiguration alloc] init] autorelease] processPoolConfiguration:processPoolConfiguration]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView evaluateJavaScript:@"while(1);" completionHandler:nil];

    // The test passes if we can prepare for data interaction without timing out.
    auto dragSession = adoptNS([[MockDragSession alloc] init]);
    [(id <UIDragInteractionDelegate_ForWebKitOnly>)[webView dragInteractionDelegate] _dragInteraction:[webView dragInteraction] prepareForSession:dragSession.get() completion:^() { }];
}

TEST(DataInteractionTests, WebItemProviderPasteboardLoading)
{
    static NSString *fastString = @"This data loads quickly";
    static NSString *slowString = @"This data loads slowly";

    WebItemProviderPasteboard *pasteboard = [WebItemProviderPasteboard sharedInstance];
    auto fastItem = adoptNS([[UIItemProvider alloc] init]);
    [fastItem registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeUTF8PlainText options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([fastString dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];

    auto slowItem = adoptNS([[UIItemProvider alloc] init]);
    [slowItem registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeUTF8PlainText options:nil loadHandler:^NSProgress *(UIItemProviderDataLoadCompletionBlock completionBlock)
    {
        sleep(2_s);
        completionBlock([slowString dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];

    __block bool hasRunFirstCompletionBlock = false;
    pasteboard.itemProviders = @[ fastItem.get(), slowItem.get() ];
    [pasteboard doAfterLoadingProvidedContentIntoFileURLs:^(NSArray<NSURL *> *urls) {
        EXPECT_EQ(2UL, urls.count);
        auto firstString = adoptNS([[NSString alloc] initWithContentsOfURL:urls[0] encoding:NSUTF8StringEncoding error:nil]);
        auto secondString = adoptNS([[NSString alloc] initWithContentsOfURL:urls[1] encoding:NSUTF8StringEncoding error:nil]);
        EXPECT_WK_STREQ(fastString, [firstString UTF8String]);
        EXPECT_WK_STREQ(slowString, [secondString UTF8String]);
        hasRunFirstCompletionBlock = true;
    } synchronousTimeout:600];
    EXPECT_TRUE(hasRunFirstCompletionBlock);

    __block bool hasRunSecondCompletionBlock = false;
    [pasteboard doAfterLoadingProvidedContentIntoFileURLs:^(NSArray<NSURL *> *urls) {
        EXPECT_EQ(2UL, urls.count);
        auto firstString = adoptNS([[NSString alloc] initWithContentsOfURL:urls[0] encoding:NSUTF8StringEncoding error:nil]);
        auto secondString = adoptNS([[NSString alloc] initWithContentsOfURL:urls[1] encoding:NSUTF8StringEncoding error:nil]);
        EXPECT_WK_STREQ(fastString, [firstString UTF8String]);
        EXPECT_WK_STREQ(slowString, [secondString UTF8String]);
        hasRunSecondCompletionBlock = true;
    } synchronousTimeout:0];
    EXPECT_FALSE(hasRunSecondCompletionBlock);
    TestWebKitAPI::Util::run(&hasRunSecondCompletionBlock);
}

TEST(DataInteractionTests, DoNotCrashWhenSelectionMovesOffscreenAfterDragStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dragstart-change-selection-offscreen"];

    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("FAR OFFSCREEN", [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"]);
}

TEST(DataInteractionTests, AdditionalItemsCanBePreventedOnDragStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"selected-text-image-link-and-editable"];
    [webView stringByEvaluatingJavaScript:@"link.addEventListener('dragstart', e => e.preventDefault())"];
    [webView stringByEvaluatingJavaScript:@"image.addEventListener('dragstart', e => e.preventDefault())"];

    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400) additionalItemRequestLocations:@{
        @0.33: [NSValue valueWithCGPoint:CGPointMake(50, 150)],
        @0.66: [NSValue valueWithCGPoint:CGPointMake(50, 250)]
    }];
    EXPECT_WK_STREQ("ABCD", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(DataInteractionTests, AdditionalLinkAndImageIntoContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"selected-text-image-link-and-editable"];

    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400) additionalItemRequestLocations:@{
        @0.33: [NSValue valueWithCGPoint:CGPointMake(50, 150)],
        @0.66: [NSValue valueWithCGPoint:CGPointMake(50, 250)]
    }];
    EXPECT_WK_STREQ("ABCDA link", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!editor.querySelector('img')"]);
    EXPECT_WK_STREQ("https://www.apple.com/", [webView stringByEvaluatingJavaScript:@"editor.querySelector('a').href"]);
}

TEST(DataInteractionTests, DragLiftPreviewDataTransferSetDragImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer-setDragImage"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    // Use DataTransfer.setDragImage to specify an existing image element in the DOM.
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(250, 50)];
    checkCGRectIsEqualToCGRectWithLogging({{100, 50}, {215, 174}}, [simulator liftPreviews][0].view.frame);

    // Use DataTransfer.setDragImage to specify an existing image element in the DOM, with x and y offsets.
    [simulator runFrom:CGPointMake(10, 150) to:CGPointMake(250, 150)];
    checkCGRectIsEqualToCGRectWithLogging({{-90, 50}, {215, 174}}, [simulator liftPreviews][0].view.frame);

    // Use DataTransfer.setDragImage to specify a newly created Image, disconnected from the DOM.
    [simulator runFrom:CGPointMake(100, 250) to:CGPointMake(250, 250)];
    checkCGRectIsEqualToCGRectWithLogging({{100, 250}, {215, 174}}, [simulator liftPreviews][0].view.frame);

    // Don't use DataTransfer.setDragImage and fall back to snapshotting the dragged element.
    [simulator runFrom:CGPointMake(50, 350) to:CGPointMake(250, 350)];
    checkCGRectIsEqualToCGRectWithLogging({{0, 300}, {300, 100}}, [simulator liftPreviews][0].view.frame);

    // Perform a normal drag on an image.
    [simulator runFrom:CGPointMake(50, 450) to:CGPointMake(250, 450)];
    checkCGRectIsEqualToCGRectWithLogging({{0, 400}, {215, 174}}, [simulator liftPreviews][0].view.frame);
}

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 110300

static NSData *testIconImageData()
{
    return [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"icon" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingImageAndMarkup)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    WKPreferencesSetCustomPasteboardDataEnabled((WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];

    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypePNG withData:testIconImageData()];
    NSString *markupString = @"<script>bar()</script><strong onmousedown=javascript:void(0)>HELLO WORLD</strong>";
    [itemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML withData:[markupString dataUsingEncoding:NSUTF8StringEncoding]];
    [itemProvider setSuggestedName:@"icon"];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointZero to:CGPointMake(50, 100)];

    EXPECT_WK_STREQ("Files, text/html", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/html), (FILE, image/png)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('icon.png', image/png)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("HELLO WORLD", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"rawHTMLData.textContent"] containsString:@"script"]);
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingPlainText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(plain)"];
    [simulator runFrom:CGPointMake(50, 75) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ @"text/plain" : @"" },
        @"drop": @{ @"text/plain" : @"Plain text" }
    });
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingCustomData)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];
    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover" : @{
            @"text/plain" : @"",
            @"foo/plain" : @"",
            @"text/html" : @"",
            @"bar/html" : @"",
            @"text/uri-list" : @"",
            @"baz/uri-list" : @""
        },
        @"drop" : @{
            @"text/plain" : @"ben bitdiddle",
            @"foo/plain" : @"eva lu ator",
            @"text/html" : @"<b>bold text</b>",
            @"bar/html" : @"<i>italic text</i>",
            @"text/uri-list" : @"https://www.apple.com",
            @"baz/uri-list" : @"https://www.webkit.org"
        }
    });
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"rich.innerHTML = '<a href=\"https://www.apple.com/\">This is a link.</a>'"];
    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/uri-list" : @"",
            @"text/plain" : @""
        },
        @"drop": @{
            @"text/uri-list" : @"https://www.apple.com/",
            @"text/plain" : @"https://www.apple.com/"
        }
    });
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingImageWithFileURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    NSURL *iconURL = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    [itemProvider registerFileRepresentationForTypeIdentifier:(NSString *)kUTTypePNG fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedIconURL = retainPtr(iconURL)] (FileLoadCompletionBlock completionHandler) -> NSProgress * {
        completionHandler(protectedIconURL.get(), NO, nil);
        return nil;
    }];
    [itemProvider registerObject:iconURL visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];

    [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];

    // File URLs should never be exposed directly to web content, so DataTransfer.getData should return an empty string here.
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ @"Files": @"", @"text/uri-list": @"" },
        @"drop": @{ @"Files": @"", @"text/uri-list": @"" }
    });
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingImageWithHTTPURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeJPEG visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(DataLoadCompletionBlock completionHandler)
    {
        completionHandler(UIImageJPEGRepresentation(testIconImage(), 0.5), nil);
        return nil;
    }];
    [itemProvider registerObject:[NSURL URLWithString:@"http://webkit.org"] visibility:UIItemProviderRepresentationOptionsVisibilityAll];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];

    [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ @"Files": @"", @"text/uri-list": @"" },
        @"drop": @{ @"Files": @"", @"text/uri-list": @"http://webkit.org/" }
    });
}

TEST(DataInteractionTests, DataTransferGetDataWhenDroppingRespectsPresentationStyle)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    runTestWithTemporaryTextFile(^(NSURL *fileURL) {
        auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
        [itemProvider registerFileRepresentationForTypeIdentifier:(NSString *)kUTTypeUTF8PlainText fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedFileURL = retainPtr(fileURL)] (FileLoadCompletionBlock completionHandler) -> NSProgress * {
            completionHandler(protectedFileURL.get(), NO, nil);
            return nil;
        }];
        [simulator setExternalItemProviders:@[ itemProvider.get() ]];

        [itemProvider setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
        [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];
        checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
            @"dragover": @{ @"Files" : @"" },
            @"drop": @{ @"Files" : @"" }
        });

        [itemProvider setPreferredPresentationStyle:UIPreferredPresentationStyleInline];
        [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];
        checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
            @"dragover": @{ @"text/plain" : @"" },
            @"drop": @{ @"text/plain" : @"This is a tiny blob of text." }
        });
    });
}

TEST(DataInteractionTests, DataTransferSetDataCannotWritePlatformTypes)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'text/plain' : 'foo', 'com.adobe.pdf' : 'try and decode me!' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];
    checkFirstTypeIsPresentAndSecondTypeIsMissing(simulator.get(), kUTTypeUTF8PlainText, kUTTypePDF);
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/plain": @"",
            @"com.adobe.pdf": @""
        },
        @"drop": @{
            @"text/plain": @"foo",
            @"com.adobe.pdf": @"try and decode me!"
        }
    });
}

TEST(DataInteractionTests, DataTransferGetDataCannotReadPrivateArbitraryTypes)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[UIItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(NSString *)kUTTypeMP3 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(DataLoadCompletionBlock completionHandler)
    {
        completionHandler([@"this is a test" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];
    [itemProvider registerDataRepresentationForTypeIdentifier:@"org.WebKit.TestWebKitAPI.custom-pasteboard-type" visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(DataLoadCompletionBlock completionHandler)
    {
        completionHandler([@"this is another test" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [itemProvider setPreferredPresentationStyle:UIPreferredPresentationStyleInline];
    [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ },
        @"drop": @{ }
    });
}

TEST(DataInteractionTests, DataTransferSetDataValidURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'url' : 'https://webkit.org/b/123' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    __block bool done = false;
    [simulator.get() setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(1UL, session.items.count);
        auto *item = session.items[0].itemProvider;
        EXPECT_TRUE([item.registeredTypeIdentifiers containsObject:(NSString *)kUTTypeURL]);
        EXPECT_TRUE([item canLoadObjectOfClass: [NSURL class]]);
        [item loadObjectOfClass:[NSURL class] completionHandler:^(id<NSItemProviderReading> url, NSError *error) {
            EXPECT_TRUE([url isKindOfClass: [NSURL class]]);
            EXPECT_WK_STREQ([(NSURL *)url absoluteString], @"https://webkit.org/b/123");
            done = true;
        }];
        return session.items;
    }];
    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];

    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/uri-list": @"",
        },
        @"drop": @{
            @"text/uri-list": @"https://webkit.org/b/123",
        }
    });
    TestWebKitAPI::Util::run(&done);
}

TEST(DataInteractionTests, DataTransferSetDataUnescapedURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'url' : 'http://webkit.org/b/\u4F60\u597D;?x=8 + 6' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    __block bool done = false;
    [simulator.get() setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(1UL, session.items.count);
        auto *item = session.items[0].itemProvider;
        EXPECT_TRUE([item.registeredTypeIdentifiers containsObject:(NSString *)kUTTypeURL]);
        EXPECT_TRUE([item canLoadObjectOfClass: [NSURL class]]);
        [item loadObjectOfClass:[NSURL class] completionHandler:^(id<NSItemProviderReading> url, NSError *error) {
            EXPECT_TRUE([url isKindOfClass: [NSURL class]]);
            EXPECT_WK_STREQ([(NSURL *)url absoluteString], @"http://webkit.org/b/%E4%BD%A0%E5%A5%BD;?x=8%20+%206");
            done = true;
        }];
        return session.items;
    }];
    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];

    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/uri-list": @"",
        },
        @"drop": @{
            @"text/uri-list": @"http://webkit.org/b/\u4F60\u597D;?x=8 + 6",
        }
    });
    TestWebKitAPI::Util::run(&done);
}

TEST(DataInteractionTests, DataTransferSetDataInvalidURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'url' : 'some random string' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];
    NSArray *registeredTypes = [simulator.get().sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_FALSE([registeredTypes containsObject:(NSString *)kUTTypeURL]);
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/uri-list": @"",
        },
        @"drop": @{
            @"text/uri-list": @"some random string",
        }
    });
}

TEST(DataInteractionTests, DataTransferSanitizeHTML)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'text/html' : '<meta content=\"secret\">"
        "<b onmouseover=\"dangerousCode()\">hello</b><!-- secret-->, world<script>dangerousCode()</script>' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    __block bool done = false;
    [simulator.get() setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(1UL, session.items.count);
        auto *item = session.items[0].itemProvider;
        EXPECT_TRUE([item.registeredTypeIdentifiers containsObject:(NSString *)kUTTypeHTML]);
        [item loadDataRepresentationForTypeIdentifier:(NSString *)kUTTypeHTML completionHandler:^(NSData *data, NSError *error) {
            NSString *markup = [[[NSString alloc] initWithData:(NSData *)data encoding:NSUTF8StringEncoding] autorelease];
            EXPECT_TRUE([markup containsString:@"hello"]);
            EXPECT_TRUE([markup containsString:@", world"]);
            EXPECT_FALSE([markup containsString:@"secret"]);
            EXPECT_FALSE([markup containsString:@"dangerousCode"]);
            done = true;
        }];
        return session.items;
    }];
    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];

    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/html": @"",
        },
        @"drop": @{
            @"text/html": @"<meta content=\"secret\"><b onmouseover=\"dangerousCode()\">hello</b><!-- secret-->, world<script>dangerousCode()</script>",
        }
    });
    TestWebKitAPI::Util::run(&done);
}

#endif // __IPHONE_OS_VERSION_MIN_REQUIRED >= 110300

} // namespace TestWebKitAPI

#endif // ENABLE(DATA_INTERACTION)
