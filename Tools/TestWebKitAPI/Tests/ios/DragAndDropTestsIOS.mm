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

#if ENABLE(DRAG_SUPPORT) && PLATFORM(IOS_FAMILY) && WK_API_ENABLED

#import "ClassMethodSwizzler.h"
#import "DragAndDropSimulator.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "UIKitSPI.h"
#import "WKWebViewConfigurationExtras.h"
#import <Contacts/Contacts.h>
#import <MapKit/MapKit.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/NSItemProvider+UIKitAdditions.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WebItemProviderPasteboard.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/Seconds.h>
#import <wtf/SoftLinking.h>

SOFT_LINK_FRAMEWORK(Contacts)
SOFT_LINK_CLASS(Contacts, CNMutableContact)

SOFT_LINK_FRAMEWORK(MapKit)
SOFT_LINK_CLASS(MapKit, MKMapItem)
SOFT_LINK_CLASS(MapKit, MKPlacemark)

typedef void (^FileLoadCompletionBlock)(NSURL *, BOOL, NSError *);
typedef void (^DataLoadCompletionBlock)(NSData *, NSError *);
typedef void (^NSItemProviderDataLoadCompletionBlock)(NSData *, NSError *);

#if !USE(APPLE_INTERNAL_SDK)

@interface NSItemProviderRepresentationOptions : NSObject
@end

#endif

@interface NSItemProvider ()
+ (NSItemProvider *)itemProviderWithURL:(NSURL *)url title:(NSString *)title;
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

@implementation NSItemProvider (DragAndDropTests)

- (void)registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier withData:(NSData *)data
{
    RetainPtr<NSData> retainedData = data;
    [self registerDataRepresentationForTypeIdentifier:typeIdentifier visibility:NSItemProviderRepresentationVisibilityAll loadHandler: [retainedData] (DataLoadCompletionBlock block) -> NSProgress * {
        block(retainedData.get(), nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
}

@end

@implementation TestWKWebView (DragAndDropTests)

- (BOOL)editorContainsImageElement
{
    return [self stringByEvaluatingJavaScript:@"!!editor.querySelector('img')"].boolValue;
}

- (NSString *)editorValue
{
    return [self stringByEvaluatingJavaScript:@"editor.value"];
}

@end

static void loadTestPageAndEnsureInputSession(DragAndDropSimulator *simulator, NSString *testPageName)
{
    TestWKWebView *webView = [simulator webView];
    simulator.allowsFocusToStartInputSession = YES;
    [webView becomeFirstResponder];
    [webView synchronouslyLoadTestPageNamed:testPageName];
    [simulator ensureInputSession];
}

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

static void checkRichTextTypePrecedesPlainTextType(DragAndDropSimulator *simulator)
{
    // At least one of "com.apple.flat-rtfd" or "public.rtf" is expected to have higher precedence than "public.utf8-plain-text".
    NSArray *registeredTypes = [simulator.sourceItemProviders.firstObject registeredTypeIdentifiers];
    auto indexOfRTFType = [registeredTypes indexOfObject:(__bridge NSString *)kUTTypeRTF];
    auto indexOfFlatRTFDType = [registeredTypes indexOfObject:(__bridge NSString *)kUTTypeFlatRTFD];
    auto indexOfPlainTextType = [registeredTypes indexOfObject:(__bridge NSString *)kUTTypeUTF8PlainText];
    EXPECT_NE((NSInteger)indexOfPlainTextType, NSNotFound);
    EXPECT_TRUE((indexOfRTFType != NSNotFound && indexOfRTFType < indexOfPlainTextType) || (indexOfFlatRTFDType != NSNotFound && indexOfFlatRTFDType < indexOfPlainTextType));
}

static void checkFirstTypeIsPresentAndSecondTypeIsMissing(DragAndDropSimulator *simulator, CFStringRef firstType, CFStringRef secondType)
{
    NSArray *registeredTypes = [simulator.sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_TRUE([registeredTypes containsObject:(NSString *)firstType]);
    EXPECT_FALSE([registeredTypes containsObject:(NSString *)secondType]);
}

static void checkTypeIdentifierIsRegisteredAtIndex(DragAndDropSimulator *simulator, NSString *type, NSUInteger index)
{
    NSArray *registeredTypes = [simulator.sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_GT(registeredTypes.count, index);
    EXPECT_WK_STREQ(type.UTF8String, [registeredTypes[index] UTF8String]);
}

static void checkEstimatedSize(DragAndDropSimulator *simulator, CGSize estimatedSize)
{
    NSItemProvider *sourceItemProvider = [simulator sourceItemProviders].firstObject;
    EXPECT_EQ(estimatedSize.width, sourceItemProvider.preferredPresentationSize.width);
    EXPECT_EQ(estimatedSize.height, sourceItemProvider.preferredPresentationSize.height);
}

static void checkSuggestedNameAndEstimatedSize(DragAndDropSimulator *simulator, NSString *suggestedName, CGSize estimatedSize)
{
    NSItemProvider *sourceItemProvider = [simulator sourceItemProviders].firstObject;
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

namespace TestWebKitAPI {

TEST(DragAndDropTests, ImageToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"drop"]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 215, 174) ], [simulator finalSelectionRects]);
    checkFirstTypeIsPresentAndSecondTypeIsMissing(simulator.get(), kUTTypePNG, kUTTypeFileURL);
    checkEstimatedSize(simulator.get(), { 215, 174 });
    EXPECT_TRUE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, CanStartDragOnEnormousImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<img src='enormous.svg'></img>"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 100)];

    NSArray *registeredTypes = [[simulator sourceItemProviders].firstObject registeredTypeIdentifiers];
    EXPECT_WK_STREQ((__bridge NSString *)kUTTypeScalableVectorGraphics, [registeredTypes firstObject]);
}

TEST(DragAndDropTests, ImageToTextarea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-textarea"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView editorValue]);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"drop"]);
    checkFirstTypeIsPresentAndSecondTypeIsMissing(simulator.get(), kUTTypePNG, kUTTypeFileURL);
    checkEstimatedSize(simulator.get(), { 215, 174 });
}

TEST(DragAndDropTests, ImageInLinkToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-in-link-and-input"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 2057, 232) ], [simulator finalSelectionRects]);
    checkSuggestedNameAndEstimatedSize(simulator.get(), @"icon.png", { 215, 174 });
    checkTypeIdentifierIsRegisteredAtIndex(simulator.get(), (__bridge NSString *)kUTTypePNG, 0);
    EXPECT_TRUE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, AvoidPreciseDropNearTopOfTextArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><body style='margin: 0'><textarea style='height: 100px'></textarea></body>"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto firstItem = adoptNS([[NSItemProvider alloc] initWithObject:[NSURL URLWithString:@"https://webkit.org"]]);
    [simulator setExternalItemProviders:@[ firstItem.get() ]];
    [simulator runFrom:CGPointMake(320, 10) to:CGPointMake(20, 10)];

    EXPECT_WK_STREQ("https://webkit.org/", [webView stringByEvaluatingJavaScript:@"document.querySelector('textarea').value"]);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
    [webView evaluateJavaScript:@"document.querySelector('textarea').value = ''" completionHandler:nil];

    auto secondItem = adoptNS([[NSItemProvider alloc] initWithObject:[NSURL URLWithString:@"https://apple.com"]]);
    [simulator setExternalItemProviders:@[ secondItem.get() ]];
    [simulator runFrom:CGPointMake(320, 50) to:CGPointMake(20, 50)];
    EXPECT_WK_STREQ("https://apple.com/", [webView stringByEvaluatingJavaScript:@"document.querySelector('textarea').value"]);
    EXPECT_TRUE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ImageInLinkWithoutHREFToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"image-in-link-and-input"];
    [webView stringByEvaluatingJavaScript:@"link.href = ''"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView editorValue]);
    checkEstimatedSize(simulator.get(), { 215, 174 });
    checkTypeIdentifierIsRegisteredAtIndex(simulator.get(), (__bridge NSString *)kUTTypePNG, 0);
}

TEST(DragAndDropTests, ImageDoesNotUseElementSizeAsEstimatedSize)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"gif-and-file-input"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom: { 100, 100 } to: { 100, 300 }];

    checkTypeIdentifierIsRegisteredAtIndex(simulator.get(), (__bridge NSString *)kUTTypeGIF, 0);
    checkSuggestedNameAndEstimatedSize(simulator.get(), @"apple.gif", { 52, 64 });
    EXPECT_WK_STREQ("apple.gif (image/gif)", [webView stringByEvaluatingJavaScript:@"output.textContent"]);
}

TEST(DragAndDropTests, ContentEditableToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    loadTestPageAndEnsureInputSession(simulator.get(), @"autofocus-contenteditable");
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([simulator suppressedSelectionCommandsDuringDrop]);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"drop"]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 961, 227) ], [simulator finalSelectionRects]);
    checkRichTextTypePrecedesPlainTextType(simulator.get());
    EXPECT_TRUE([simulator lastKnownDropProposal].precise);

    // FIXME: Once <rdar://problem/46830277> is fixed, we should add "com.apple.webarchive" as a registered pasteboard type and rebaseline this expectation.
    EXPECT_FALSE([[[simulator sourceItemProviders].firstObject registeredTypeIdentifiers] containsObject:(__bridge NSString *)kUTTypeWebArchive]);
}

TEST(DragAndDropTests, ContentEditableToTextarea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    loadTestPageAndEnsureInputSession(simulator.get(), @"contenteditable-and-textarea");
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([simulator suppressedSelectionCommandsDuringDrop]);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.textContent"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"drop"]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(6, 203, 990, 232) ], [simulator finalSelectionRects]);
    checkRichTextTypePrecedesPlainTextType(simulator.get());
    EXPECT_TRUE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, NonEditableTextSelectionToTextarea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView synchronouslyLoadTestPageNamed:@"selected-text-and-textarea"];
    [simulator runFrom:CGPointMake(160, 100) to:CGPointMake(160, 300)];

    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"destination.value"]);
}

TEST(DragAndDropTests, ContentEditableMoveParagraphs)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    loadTestPageAndEnsureInputSession(simulator.get(), @"two-paragraph-contenteditable");
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(250, 450)];

    NSString *finalTextContent = [webView stringByEvaluatingJavaScript:@"editor.textContent"];
    NSUInteger firstParagraphOffset = [finalTextContent rangeOfString:@"This is the first paragraph"].location;
    NSUInteger secondParagraphOffset = [finalTextContent rangeOfString:@"This is the second paragraph"].location;

    EXPECT_TRUE([simulator suppressedSelectionCommandsDuringDrop]);
    EXPECT_FALSE(firstParagraphOffset == NSNotFound);
    EXPECT_FALSE(secondParagraphOffset == NSNotFound);
    EXPECT_GT(firstParagraphOffset, secondParagraphOffset);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(190, 100, 130, 20), makeCGRectValue(0, 120, 320, 100), makeCGRectValue(0, 220, 252, 20) ], [simulator finalSelectionRects]);
    EXPECT_TRUE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, DragImageFromContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView synchronouslyLoadTestPageNamed:@"contenteditable-and-target"];
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
}

TEST(DragAndDropTests, TextAreaToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    loadTestPageAndEnsureInputSession(simulator.get(), @"textarea-to-input");
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([simulator suppressedSelectionCommandsDuringDrop]);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_WK_STREQ("Hello world", [webView editorValue].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 990, 232) ], [simulator finalSelectionRects]);
}

TEST(DragAndDropTests, SinglePlainTextWordTypeIdentifiers)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    loadTestPageAndEnsureInputSession(simulator.get(), @"textarea-to-input");
    [webView stringByEvaluatingJavaScript:@"source.value = 'pneumonoultramicroscopicsilicovolcanoconiosis'"];
    [webView stringByEvaluatingJavaScript:@"source.selectionStart = 0"];
    [webView stringByEvaluatingJavaScript:@"source.selectionEnd = source.value.length"];
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([simulator suppressedSelectionCommandsDuringDrop]);

    NSItemProvider *itemProvider = [simulator sourceItemProviders].firstObject;
    NSArray *registeredTypes = [itemProvider registeredTypeIdentifiers];
    EXPECT_EQ(1UL, registeredTypes.count);
    EXPECT_WK_STREQ([(__bridge NSString *)kUTTypeUTF8PlainText UTF8String], [registeredTypes.firstObject UTF8String]);
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"source.value"].length, 0UL);
    EXPECT_EQ(UIPreferredPresentationStyleInline, itemProvider.preferredPresentationStyle);
    EXPECT_WK_STREQ("pneumonoultramicroscopicsilicovolcanoconiosis", [webView editorValue].UTF8String);
}

TEST(DragAndDropTests, SinglePlainTextURLTypeIdentifiers)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    loadTestPageAndEnsureInputSession(simulator.get(), @"textarea-to-input");
    [webView stringByEvaluatingJavaScript:@"source.value = 'https://webkit.org/'"];
    [webView stringByEvaluatingJavaScript:@"source.selectionStart = 0"];
    [webView stringByEvaluatingJavaScript:@"source.selectionEnd = source.value.length"];
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_TRUE([simulator suppressedSelectionCommandsDuringDrop]);

    NSItemProvider *itemProvider = [simulator sourceItemProviders].firstObject;
    NSArray *registeredTypes = [itemProvider registeredTypeIdentifiers];
    EXPECT_EQ(2UL, registeredTypes.count);
    EXPECT_WK_STREQ([(__bridge NSString *)kUTTypeURL UTF8String], [registeredTypes.firstObject UTF8String]);
    EXPECT_WK_STREQ([(__bridge NSString *)kUTTypeUTF8PlainText UTF8String], [registeredTypes.lastObject UTF8String]);
    EXPECT_EQ(0UL, [webView stringByEvaluatingJavaScript:@"source.value"].length);
    EXPECT_EQ(UIPreferredPresentationStyleInline, itemProvider.preferredPresentationStyle);
    EXPECT_WK_STREQ("https://webkit.org/", [webView editorValue].UTF8String);
}

TEST(DragAndDropTests, LinkToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    __block bool doneLoadingURL = false;
    NSItemProvider *sourceItemProvider = [simulator sourceItemProviders].firstObject;
    [sourceItemProvider loadObjectOfClass:[NSURL class] completionHandler:^(id object, NSError *error) {
        NSURL *url = object;
        EXPECT_WK_STREQ("Hello world", url._title.UTF8String ?: "");
        doneLoadingURL = true;
    }];
    TestWebKitAPI::Util::run(&doneLoadingURL);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"drop"]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 273, 2057, 232) ], [simulator finalSelectionRects]);
    checkTypeIdentifierIsRegisteredAtIndex(simulator.get(), (__bridge NSString *)kUTTypeURL, 0);
}

TEST(DragAndDropTests, BackgroundImageLinkToInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"background-image-link-and-input"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://www.apple.com/", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"drop"]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(101, 241, 2057, 232) ], [simulator finalSelectionRects]);
    checkTypeIdentifierIsRegisteredAtIndex(simulator.get(), (__bridge NSString *)kUTTypeURL, 0);
}

TEST(DragAndDropTests, CanPreventStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-start"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ(DragAndDropPhaseCancelled, [simulator phase]);
    EXPECT_FALSE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_FALSE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_FALSE([observedEventNames containsObject:@"dragover"]);
    checkSelectionRectsWithLogging(@[ ], [simulator finalSelectionRects]);
}

TEST(DragAndDropTests, CanPreventOperation)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"prevent-operation"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_FALSE([webView editorContainsImageElement]);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    checkSelectionRectsWithLogging(@[ ], [simulator finalSelectionRects]);
}

TEST(DragAndDropTests, EnterAndLeaveEvents)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 450)];

    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);

    NSArray *observedEventNames = [simulator observedEventNames];
    EXPECT_TRUE([observedEventNames containsObject:@"dragenter"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragover"]);
    EXPECT_TRUE([observedEventNames containsObject:@"dragleave"]);
    EXPECT_FALSE([observedEventNames containsObject:@"drop"]);
    checkSelectionRectsWithLogging(@[ ], [simulator finalSelectionRects]);
}

TEST(DragAndDropTests, CanStartDragOnDivWithDraggableAttribute)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"custom-draggable-div"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 250)];

    EXPECT_GT([simulator sourceItemProviders].count, 0UL);
    NSItemProvider *itemProvider = [simulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleInline, itemProvider.preferredPresentationStyle);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!destination.querySelector('#item')"]);
    EXPECT_WK_STREQ(@"PASS", [webView stringByEvaluatingJavaScript:@"item.textContent"]);
}

TEST(DragAndDropTests, ExternalSourcePlainTextToIFrame)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-in-iframe"];

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(160, 250)];

    auto containerLeft = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().left"].floatValue;
    auto containerTop = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().top"].floatValue;
    auto containerWidth = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().width"].floatValue;
    auto containerHeight = [webView stringByEvaluatingJavaScript:@"container.getBoundingClientRect().height"].floatValue;
    checkDragCaretRectIsContainedInRect([simulator lastKnownDragCaretRect], CGRectMake(containerLeft, containerTop, containerWidth, containerHeight));
}

TEST(DragAndDropTests, ExternalSourceInlineTextToFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider setPreferredPresentationStyle:UIPreferredPresentationStyleInline];
    [simulatedItemProvider registerObject:@"This item provider requested inline presentation style." visibility:NSItemProviderRepresentationVisibilityAll];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"output.value"]);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceJSONToFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedJSONItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *jsonData = [@"{ \"foo\": \"bar\",  \"bar\": \"baz\" }" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedJSONItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJSON withData:jsonData];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedJSONItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("application/json", [webView stringByEvaluatingJavaScript:@"output.value"]);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceImageToFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG withData:imageData];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedImageItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceHTMLToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:htmlData];
    [simulatedHTMLItemProvider setSuggestedName:@"index.html"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setShouldAllowMoveOperation:NO];
    [simulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("text/html", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceMoveOperationNotAllowed)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];
    [webView stringByEvaluatingJavaScript:@"upload.dropEffect = 'move'"];

    auto simulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:htmlData];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setShouldAllowMoveOperation:NO];
    [simulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DragAndDropTests, ExternalSourcePKCS12ToSingleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePKCS12 withData:[@"Not a real p12 file." dataUsingEncoding:NSUTF8StringEncoding]];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ item.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("application/x-pkcs12", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DragAndDropTests, ExternalSourceZIPArchiveAndURLToSingleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto archiveProvider = adoptNS([[NSItemProvider alloc] init]);
    [archiveProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeZipArchive withData:testZIPArchive()];

    auto urlProvider = adoptNS([[NSItemProvider alloc] init]);
    [urlProvider registerObject:[NSURL URLWithString:@"https://webkit.org"] visibility:NSItemProviderRepresentationVisibilityAll];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ archiveProvider.get(), urlProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("application/zip", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceZIPArchiveToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeZipArchive withData:testZIPArchive()];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("application/zip", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceImageAndHTMLToSingleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG withData:imageData];

    auto simulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:htmlData];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get(), simulatedImageItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceImageAndHTMLToMultipleFileInput)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];
    [webView stringByEvaluatingJavaScript:@"input.setAttribute('multiple', '')"];

    auto simulatedImageItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG withData:imageData];

    auto simulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *htmlData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:htmlData];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedHTMLItemProvider.get(), simulatedImageItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 100)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg, text/html", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceImageAndHTMLToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG withData:imageData];

    auto firstSimulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *firstHTMLData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [firstSimulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:firstHTMLData];

    auto secondSimulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *secondHTMLData = [@"<html><body>hello world</body></html>" dataUsingEncoding:NSUTF8StringEncoding];
    [secondSimulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:secondHTMLData];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setShouldAllowMoveOperation:NO];
    [simulator setExternalItemProviders:@[ simulatedImageItemProvider.get(), firstSimulatedHTMLItemProvider.get(), secondSimulatedHTMLItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("image/jpeg, text/html, text/html", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

TEST(DragAndDropTests, ExternalSourceHTMLToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *htmlData = [@"<h1>This is a test</h1>" dataUsingEncoding:NSUTF8StringEncoding];
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:htmlData];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    NSString *textContent = [webView stringByEvaluatingJavaScript:@"editor.textContent"];
    EXPECT_WK_STREQ("This is a test", textContent.UTF8String);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!editor.querySelector('h1')"].boolValue);
}

TEST(DragAndDropTests, ExternalSourceBoldSystemAttributedStringToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    NSDictionary *textAttributes = @{ NSFontAttributeName: [UIFont boldSystemFontOfSize:20] };
    NSAttributedString *richText = [[NSAttributedString alloc] initWithString:@"This is a test" attributes:textAttributes];
    auto itemProvider = adoptNS([[NSItemProvider alloc] initWithObject:richText]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("This is a test", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
}

TEST(DragAndDropTests, ExternalSourceColoredAttributedStringToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    NSDictionary *textAttributes = @{ NSForegroundColorAttributeName: [UIColor redColor] };
    NSAttributedString *richText = [[NSAttributedString alloc] initWithString:@"This is a test" attributes:textAttributes];
    auto itemProvider = adoptNS([[NSItemProvider alloc] initWithObject:richText]);
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("rgb(255, 0, 0)", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('p')).color"]);
    EXPECT_WK_STREQ("This is a test", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
}

TEST(DragAndDropTests, ExternalSourceMultipleURLsToContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto firstItem = adoptNS([[NSItemProvider alloc] init]);
    [firstItem registerObject:[NSURL URLWithString:@"https://www.apple.com/iphone/"] visibility:NSItemProviderRepresentationVisibilityAll];
    auto secondItem = adoptNS([[NSItemProvider alloc] init]);
    [secondItem registerObject:[NSURL URLWithString:@"https://www.apple.com/mac/"] visibility:NSItemProviderRepresentationVisibilityAll];
    auto thirdItem = adoptNS([[NSItemProvider alloc] init]);
    [thirdItem registerObject:[NSURL URLWithString:@"https://webkit.org/"] visibility:NSItemProviderRepresentationVisibilityAll];
    [simulator setExternalItemProviders:@[ firstItem.get(), secondItem.get(), thirdItem.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

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

TEST(DragAndDropTests, RespectsExternalSourceFidelityRankings)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    // Here, our source item provider vends two representations: plain text, and then an image. If we don't respect the
    // fidelity order requested by the source, we'll end up assuming that the image is a higher fidelity representation
    // than the plain text, and erroneously insert the image. If we respect source fidelities, we'll insert text rather
    // than an image.
    auto simulatedItemProviderWithTextFirst = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProviderWithTextFirst registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];
    [simulatedItemProviderWithTextFirst registerObject:testIconImage() visibility:NSItemProviderRepresentationVisibilityAll];
    [simulator setExternalItemProviders:@[ simulatedItemProviderWithTextFirst.get() ]];

    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    EXPECT_FALSE([webView editorContainsImageElement]);
    [webView stringByEvaluatingJavaScript:@"editor.innerHTML = ''"];

    // Now we register the item providers in reverse, and expect the image to be inserted instead of text.
    auto simulatedItemProviderWithImageFirst = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProviderWithImageFirst registerObject:testIconImage() visibility:NSItemProviderRepresentationVisibilityAll];
    [simulatedItemProviderWithImageFirst registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];
    [simulator setExternalItemProviders:@[ simulatedItemProviderWithImageFirst.get() ]];

    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    EXPECT_TRUE([webView editorContainsImageElement]);
}

static BOOL overrideIsInHardwareKeyboardMode()
{
    return NO;
}

TEST(DragAndDropTests, ExternalSourceUTF8PlainTextOnly)
{
    ClassMethodSwizzler swizzler([UIKeyboard class], @selector(isInHardwareKeyboardMode), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    NSString *textPayload = @"Ceci n'est pas une string";
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(NSItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([textPayload dataUsingEncoding:NSUTF8StringEncoding], nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ(textPayload.UTF8String, [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 1936, 227) ], [simulator finalSelectionRects]);
}

TEST(DragAndDropTests, ExternalSourceJPEGOnly)
{
    ClassMethodSwizzler swizzler([UIKeyboard class], @selector(isInHardwareKeyboardMode), reinterpret_cast<IMP>(overrideIsInHardwareKeyboardMode));

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(NSItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock(UIImageJPEGRepresentation(testIconImage(), 0.5), nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    EXPECT_TRUE([webView editorContainsImageElement]);
    checkSelectionRectsWithLogging(@[ makeCGRectValue(1, 201, 215, 174) ], [simulator finalSelectionRects]);
}

TEST(DragAndDropTests, ExternalSourceTitledNSURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    NSURL *titledURL = [NSURL URLWithString:@"https://www.apple.com"];
    titledURL._title = @"Apple";
    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider registerObject:titledURL visibility:NSItemProviderRepresentationVisibilityAll];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("Apple", [webView stringByEvaluatingJavaScript:@"editor.querySelector('a').textContent"]);
    EXPECT_WK_STREQ("https://www.apple.com/", [webView stringByEvaluatingJavaScript:@"editor.querySelector('a').href"]);
}

TEST(DragAndDropTests, ExternalSourceFileURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    NSURL *URL = [NSURL URLWithString:@"file:///some/file/that/is/not/real"];
    NSItemProvider *simulatedItemProvider = [NSItemProvider itemProviderWithURL:URL title:@""];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedItemProvider ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_FALSE([[webView stringByEvaluatingJavaScript:@"!!editor.querySelector('a')"] boolValue]);
    EXPECT_WK_STREQ("Hello world\nfile:///some/file/that/is/not/real", [webView stringByEvaluatingJavaScript:@"document.body.innerText"]);
}

TEST(DragAndDropTests, ExternalSourceOverrideDropFileUpload)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulatedImageItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *imageData = UIImageJPEGRepresentation(testIconImage(), 0.5);
    [simulatedImageItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG withData:imageData];

    auto simulatedHTMLItemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *firstHTMLData = [@"<body contenteditable></body>" dataUsingEncoding:NSUTF8StringEncoding];
    [simulatedHTMLItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:firstHTMLData];
    [simulatedHTMLItemProvider setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(2UL, session.items.count);
        UIDragItem *firstItem = session.items[0];
        UIDragItem *secondItem = session.items[1];
        EXPECT_TRUE([firstItem.itemProvider.registeredTypeIdentifiers isEqual:@[ (__bridge NSString *)kUTTypeJPEG ]]);
        EXPECT_TRUE([secondItem.itemProvider.registeredTypeIdentifiers isEqual:@[ (__bridge NSString *)kUTTypeHTML ]]);
        return @[ secondItem ];
    }];
    [simulator setExternalItemProviders:@[ simulatedImageItemProvider.get(), simulatedHTMLItemProvider.get() ]];
    [simulator runFrom:CGPointMake(200, 300) to:CGPointMake(100, 300)];

    NSString *outputValue = [webView stringByEvaluatingJavaScript:@"output.value"];
    EXPECT_WK_STREQ("text/html", outputValue.UTF8String);
    EXPECT_FALSE([simulator lastKnownDropProposal].precise);
}

static RetainPtr<NSItemProvider> createMapItemForTesting()
{
    auto placemark = adoptNS([allocMKPlacemarkInstance() initWithCoordinate:CLLocationCoordinate2DMake(37.3327, -122.0053)]);
    auto item = adoptNS([allocMKMapItemInstance() initWithPlacemark:placemark.get()]);
    [item setName:@"Apple Park"];

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerObject:item.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider setSuggestedName:[item name]];

    return itemProvider;
}

static RetainPtr<NSItemProvider> createContactItemForTesting()
{
    auto contact = adoptNS([allocCNMutableContactInstance() init]);
    [contact setGivenName:@"Foo"];
    [contact setFamilyName:@"Bar"];

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerObject:contact.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider setSuggestedName:@"Foo Bar"];

    return itemProvider;
}

TEST(DragAndDropTests, ExternalSourceMapItemIntoEditableAreas)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-and-textarea"];
    [webView _synchronouslyExecuteEditCommand:@"Delete" argument:nil];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ createMapItemForTesting().autorelease() ]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 100)];
    EXPECT_WK_STREQ("Apple Park", [webView stringByEvaluatingJavaScript:@"document.querySelector('div[contenteditable]').textContent"]);
    NSURL *firstURL = [NSURL URLWithString:[webView stringByEvaluatingJavaScript:@"document.querySelector('a').href"]];
    EXPECT_WK_STREQ("maps.apple.com", firstURL.host);

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 300)];
    NSURL *secondURL = [NSURL URLWithString:[webView stringByEvaluatingJavaScript:@"document.querySelector('textarea').value"]];
    EXPECT_WK_STREQ("maps.apple.com", secondURL.host);
}

TEST(DragAndDropTests, ExternalSourceContactIntoEditableAreas)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"contenteditable-and-textarea"];
    [webView _synchronouslyExecuteEditCommand:@"Delete" argument:nil];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ createContactItemForTesting().autorelease() ]];
    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 100)];
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"document.querySelector('div[contenteditable]').textContent"]);

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"document.querySelector('textarea').textContent"]);
}

TEST(DragAndDropTests, ExternalSourceMapItemAndContactToUploadArea)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"file-uploading"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ createMapItemForTesting().autorelease(), createContactItemForTesting().autorelease() ]];
    [simulator runFrom:CGPointMake(200, 100) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("text/vcard, text/vcard", [webView stringByEvaluatingJavaScript:@"output.value"]);
}

static RetainPtr<TestWKWebView> setUpTestWebViewForDataTransferItems()
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransferItem-getAsEntry"];

    auto preferences = (__bridge WKPreferencesRef)[[webView configuration] preferences];
    WKPreferencesSetDataTransferItemsEnabled(preferences, true);
    WKPreferencesSetDirectoryUploadEnabled(preferences, true);

    return webView;
}

TEST(DragAndDropTests, ExternalSourceDataTransferItemGetFolderAsEntry)
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
        [itemProvider registerFileRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeFolder fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[capturedFolderURL = retainPtr(folderURL)] (FileLoadCompletionBlock completionHandler) -> NSProgress * {
            completionHandler(capturedFolderURL.get(), NO, nil);
            return nil;
        }];

        auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
        [simulator setExternalItemProviders:@[ itemProvider.get() ]];
        [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    });

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ([expectedOutput componentsJoinedByString:@"\n"], [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DragAndDropTests, ExternalSourceDataTransferItemGetPlainTextFileAsEntry)
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
        [itemProvider registerFileRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[capturedFileURL = retainPtr(fileURL)](FileLoadCompletionBlock completionHandler) -> NSProgress * {
            completionHandler(capturedFileURL.get(), NO, nil);
            return nil;
        }];

        auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
        [simulator setExternalItemProviders:@[ itemProvider.get() ]];
        [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(150, 50)];
    });

    TestWebKitAPI::Util::run(&done);
    EXPECT_WK_STREQ([expectedOutput componentsJoinedByString:@"\n"], [webView stringByEvaluatingJavaScript:@"output.value"]);
}

TEST(DragAndDropTests, ExternalSourceOverrideDropInsertURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        NSMutableArray<UIDragItem *> *allowedItems = [NSMutableArray array];
        for (UIDragItem *item in session.items) {
            if ([item.itemProvider.registeredTypeIdentifiers containsObject:(__bridge NSString *)kUTTypeURL])
                [allowedItems addObject:item];
        }
        EXPECT_EQ(1UL, allowedItems.count);
        return allowedItems;
    }];

    auto firstItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [firstItemProvider registerObject:@"This is a string." visibility:NSItemProviderRepresentationVisibilityAll];
    auto secondItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [secondItemProvider registerObject:[NSURL URLWithString:@"https://webkit.org/"] visibility:NSItemProviderRepresentationVisibilityAll];
    [simulator setExternalItemProviders:@[ firstItemProvider.get(), secondItemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("https://webkit.org/", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(DragAndDropTests, OverrideDrop)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];

    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:[@"<body></body>" dataUsingEncoding:NSUTF8StringEncoding]];

    __block bool finishedLoadingData = false;
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator setOverrideDragUpdateBlock:[] (UIDropOperation operation, id <UIDropSession> session) {
        EXPECT_EQ(UIDropOperationCancel, operation);
        return UIDropOperationCopy;
    }];
    [simulator setDropCompletionBlock:^(BOOL handled, NSArray *itemProviders) {
        EXPECT_FALSE(handled);
        [itemProviders.firstObject loadDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML completionHandler:^(NSData *data, NSError *error) {
            NSString *text = [[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] autorelease];
            EXPECT_WK_STREQ("<body></body>", text.UTF8String);
            EXPECT_FALSE(!!error);
            finishedLoadingData = true;
        }];
    }];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];
    TestWebKitAPI::Util::run(&finishedLoadingData);
}

TEST(DragAndDropTests, InjectedBundleOverridePerformTwoStepDrop)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration.processPool _setObject:@YES forBundleParameter:@"BundleOverridePerformTwoStepDrop"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView loadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(NSItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([@"Hello world" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_EQ(0UL, [webView stringByEvaluatingJavaScript:@"editor.textContent"].length);
}

TEST(DragAndDropTests, InjectedBundleAllowPerformTwoStepDrop)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration.processPool _setObject:@NO forBundleParameter:@"BundleOverridePerformTwoStepDrop"];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"autofocus-contenteditable"];
    [webView stringByEvaluatingJavaScript:@"getSelection().removeAllRanges()"];

    auto simulatedItemProvider = adoptNS([[NSItemProvider alloc] init]);
    [simulatedItemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(NSItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([@"Hello world" dataUsingEncoding:NSUTF8StringEncoding], nil);
        return [NSProgress discreteProgressWithTotalUnitCount:100];
    }];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setExternalItemProviders:@[ simulatedItemProvider.get() ]];
    [simulator runFrom:CGPointMake(300, 400) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("Hello world", [webView stringByEvaluatingJavaScript:@"editor.textContent"].UTF8String);
}

TEST(DragAndDropTests, InjectedBundleImageElementData)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration _setAttachmentElementEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"image-and-contenteditable"];

    __block RetainPtr<NSString> injectedString;
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setConvertItemProvidersBlock:^NSArray *(NSItemProvider *itemProvider, NSArray *, NSDictionary *data)
    {
        injectedString = adoptNS([[NSString alloc] initWithData:data[InjectedBundlePasteboardDataType] encoding:NSUTF8StringEncoding]);
        return @[ itemProvider ];
    }];

    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 250)];

    EXPECT_WK_STREQ("hello", [injectedString UTF8String]);
}

TEST(DragAndDropTests, InjectedBundleAttachmentElementData)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"];
    [configuration _setAttachmentElementEnabled:YES];
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration]);
    [webView synchronouslyLoadTestPageNamed:@"attachment-element"];

    __block RetainPtr<NSString> injectedString;
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setConvertItemProvidersBlock:^NSArray *(NSItemProvider *itemProvider, NSArray *, NSDictionary *data)
    {
        injectedString = adoptNS([[NSString alloc] initWithData:data[InjectedBundlePasteboardDataType] encoding:NSUTF8StringEncoding]);
        return @[ itemProvider ];
    }];

    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400)];

    EXPECT_WK_STREQ("hello", [injectedString UTF8String]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"getSelection().isCollapsed"].boolValue);
}

TEST(DragAndDropTests, LargeImageToTargetDiv)
{
    auto testWebViewConfiguration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[testWebViewConfiguration preferences] _setLargeImageAsyncDecodingEnabled:NO];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:testWebViewConfiguration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"div-and-large-image"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(200, 400) to:CGPointMake(200, 150)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"].UTF8String);
    checkFirstTypeIsPresentAndSecondTypeIsMissing(simulator.get(), kUTTypePNG, kUTTypeFileURL);
    checkEstimatedSize(simulator.get(), { 2000, 2000 });
}

TEST(DragAndDropTests, LinkWithEmptyHREF)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-input"];
    [webView stringByEvaluatingJavaScript:@"document.querySelector('a').href = ''"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];

    EXPECT_EQ(DragAndDropPhaseCancelled, [simulator phase]);
    EXPECT_WK_STREQ("", [webView editorValue].UTF8String);
}

TEST(DragAndDropTests, CancelledLiftDoesNotCauseSubsequentDragsToFail)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-target-div"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setConvertItemProvidersBlock:^NSArray *(NSItemProvider *, NSArray *, NSDictionary *)
    {
        return @[ ];
    }];
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_EQ(DragAndDropPhaseCancelled, [simulator phase]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    NSString *outputText = [webView stringByEvaluatingJavaScript:@"output.textContent"];
    checkStringArraysAreEqual(@[@"dragstart", @"dragend"], [outputText componentsSeparatedByString:@" "]);

    [webView stringByEvaluatingJavaScript:@"output.innerHTML = ''"];
    [simulator setConvertItemProvidersBlock:^NSArray *(NSItemProvider *itemProvider, NSArray *, NSDictionary *)
    {
        return @[ itemProvider ];
    }];
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
    [webView stringByEvaluatingJavaScript:@"output.textContent"];
    checkStringArraysAreEqual(@[@"dragstart", @"dragend"], [outputText componentsSeparatedByString:@" "]);
}

static void testDragAndDropOntoTargetElements(TestWKWebView *webView)
{
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView]);
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

TEST(DragAndDropTests, DragEventClientCoordinatesBasic)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DragAndDropTests, DragEventClientCoordinatesWithScrollOffset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];
    [webView stringByEvaluatingJavaScript:@"document.body.style.margin = '1000px 0'"];
    [webView stringByEvaluatingJavaScript:@"document.scrollingElement.scrollTop = 1000"];
    [webView waitForNextPresentationUpdate];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DragAndDropTests, DragEventPageCoordinatesBasic)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];
    [webView stringByEvaluatingJavaScript:@"window.usePageCoordinates = true"];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DragAndDropTests, DragEventPageCoordinatesWithScrollOffset)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"drop-targets"];
    [webView stringByEvaluatingJavaScript:@"document.body.style.margin = '1000px 0'"];
    [webView stringByEvaluatingJavaScript:@"document.scrollingElement.scrollTop = 1000"];
    [webView stringByEvaluatingJavaScript:@"window.usePageCoordinates = true"];
    [webView waitForNextPresentationUpdate];

    testDragAndDropOntoTargetElements(webView.get());
}

TEST(DragAndDropTests, DoNotCrashWhenSelectionIsClearedInDragStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dragstart-clear-selection"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"paragraph.textContent"]);
}

// FIXME: Re-enable this test once we resolve <https://bugs.webkit.org/show_bug.cgi?id=175204>
#if 0
TEST(DragAndDropTests, CustomActionSheetPopover)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"link-and-target-div"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator setShouldEnsureUIApplication:YES];

    __block BOOL didInvokeCustomActionSheet = NO;
    [simulator setShowCustomActionSheetBlock:^BOOL(_WKActivatedElementInfo *element)
    {
        EXPECT_EQ(_WKActivatedElementTypeLink, element.type);
        EXPECT_WK_STREQ("Hello world", element.title.UTF8String);
        didInvokeCustomActionSheet = YES;
        return YES;
    }];
    [simulator runFrom:CGPointMake(100, 50) to:CGPointMake(100, 300)];
    EXPECT_TRUE(didInvokeCustomActionSheet);
    EXPECT_WK_STREQ("PASS", [webView stringByEvaluatingJavaScript:@"target.textContent"].UTF8String);
}
#endif

TEST(DragAndDropTests, UnresponsivePageDoesNotHangUI)
{
    _WKProcessPoolConfiguration *processPoolConfiguration = [[[_WKProcessPoolConfiguration alloc] init] autorelease];
    processPoolConfiguration.ignoreSynchronousMessagingTimeoutsForTesting = YES;

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:[[[WKWebViewConfiguration alloc] init] autorelease] processPoolConfiguration:processPoolConfiguration]);
    [webView synchronouslyLoadTestPageNamed:@"simple"];
    [webView evaluateJavaScript:@"while(1);" completionHandler:nil];

    // The test passes if we can prepare for a drag session without timing out.
    auto dragSession = adoptNS([[MockDragSession alloc] init]);
    [(id <UIDragInteractionDelegate_ForWebKitOnly>)[webView dragInteractionDelegate] _dragInteraction:[webView dragInteraction] prepareForSession:dragSession.get() completion:^() { }];
}

TEST(DragAndDropTests, WebItemProviderPasteboardLoading)
{
    static NSString *fastString = @"This data loads quickly";
    static NSString *slowString = @"This data loads slowly";

    WebItemProviderPasteboard *pasteboard = [WebItemProviderPasteboard sharedInstance];
    auto fastItem = adoptNS([[NSItemProvider alloc] init]);
    [fastItem registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(NSItemProviderDataLoadCompletionBlock completionBlock)
    {
        completionBlock([fastString dataUsingEncoding:NSUTF8StringEncoding], nil);
        return nil;
    }];

    auto slowItem = adoptNS([[NSItemProvider alloc] init]);
    [slowItem registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(NSItemProviderDataLoadCompletionBlock completionBlock)
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

TEST(DragAndDropTests, DoNotCrashWhenSelectionMovesOffscreenAfterDragStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dragstart-change-selection-offscreen"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 100)];

    EXPECT_WK_STREQ("FAR OFFSCREEN", [webView stringByEvaluatingJavaScript:@"getSelection().getRangeAt(0).toString()"]);
}

TEST(DragAndDropTests, AdditionalItemsCanBePreventedOnDragStart)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"selected-text-image-link-and-editable"];
    [webView stringByEvaluatingJavaScript:@"link.addEventListener('dragstart', e => e.preventDefault())"];
    [webView stringByEvaluatingJavaScript:@"image.addEventListener('dragstart', e => e.preventDefault())"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400) additionalItemRequestLocations:@{
        @0.33: [NSValue valueWithCGPoint:CGPointMake(50, 150)],
        @0.66: [NSValue valueWithCGPoint:CGPointMake(50, 250)]
    }];
    EXPECT_WK_STREQ("ABCD", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
}

TEST(DragAndDropTests, AdditionalLinkAndImageIntoContentEditable)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"selected-text-image-link-and-editable"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(50, 50) to:CGPointMake(50, 400) additionalItemRequestLocations:@{
        @0.33: [NSValue valueWithCGPoint:CGPointMake(50, 150)],
        @0.66: [NSValue valueWithCGPoint:CGPointMake(50, 250)]
    }];
    EXPECT_WK_STREQ("ABCDA link", [webView stringByEvaluatingJavaScript:@"editor.textContent"]);
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!editor.querySelector('img')"]);
    EXPECT_WK_STREQ("https://www.apple.com/", [webView stringByEvaluatingJavaScript:@"editor.querySelector('a').href"]);
}

TEST(DragAndDropTests, DragLiftPreviewDataTransferSetDragImage)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer-setDragImage"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

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

static NSData *testIconImageData()
{
    return [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"icon" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
}

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingImageAndMarkup)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    WKPreferencesSetCustomPasteboardDataEnabled((__bridge WKPreferencesRef)[webView configuration].preferences, true);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePNG withData:testIconImageData()];
    NSString *markupString = @"<script>bar()</script><strong onmousedown=javascript:void(0)>HELLO WORLD</strong>";
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML withData:[markupString dataUsingEncoding:NSUTF8StringEncoding]];
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

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingPlainText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(plain)"];
    [simulator runFrom:CGPointMake(50, 75) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ @"text/plain" : @"" },
        @"drop": @{ @"text/plain" : @"Plain text" }
    });
}

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingCustomData)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

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

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

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

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingImageWithFileURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSURL *iconURL = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    [itemProvider registerFileRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypePNG fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedIconURL = retainPtr(iconURL)] (FileLoadCompletionBlock completionHandler) -> NSProgress * {
        completionHandler(protectedIconURL.get(), NO, nil);
        return nil;
    }];
    [itemProvider registerObject:iconURL visibility:NSItemProviderRepresentationVisibilityAll];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];

    [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];

    // File URLs should never be exposed directly to web content, so DataTransfer.getData should return an empty string here.
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ @"Files": @"" },
        @"drop": @{ @"Files": @"" }
    });
}

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingImageWithHTTPURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeJPEG visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(DataLoadCompletionBlock completionHandler)
    {
        completionHandler(UIImageJPEGRepresentation(testIconImage(), 0.5), nil);
        return nil;
    }];
    [itemProvider registerObject:[NSURL URLWithString:@"http://webkit.org"] visibility:NSItemProviderRepresentationVisibilityAll];
    [simulator setExternalItemProviders:@[ itemProvider.get() ]];

    [simulator runFrom:CGPointMake(300, 375) to:CGPointMake(50, 375)];
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{ @"Files": @"", @"text/uri-list": @"" },
        @"drop": @{ @"Files": @"", @"text/uri-list": @"http://webkit.org/" }
    });
}

TEST(DragAndDropTests, DataTransferGetDataWhenDroppingRespectsPresentationStyle)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    runTestWithTemporaryTextFile(^(NSURL *fileURL) {
        auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
        [itemProvider registerFileRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeUTF8PlainText fileOptions:0 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedFileURL = retainPtr(fileURL)] (FileLoadCompletionBlock completionHandler) -> NSProgress * {
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

TEST(DragAndDropTests, DataTransferSetDataCannotWritePlatformTypes)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

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

TEST(DragAndDropTests, DataTransferGetDataReadPlainAndRichText)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSDictionary *textAttributes = @{ NSFontAttributeName: [UIFont boldSystemFontOfSize:20] };
    NSAttributedString *richText = [[NSAttributedString alloc] initWithString:@"WebKit" attributes:textAttributes];
    [itemProvider registerObject:richText visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider registerObject:[NSURL URLWithString:@"https://www.webkit.org/"] visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider registerObject:@"WebKit" visibility:NSItemProviderRepresentationVisibilityAll];

    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointZero to:CGPointMake(50, 100)];

    EXPECT_WK_STREQ("text/html, text/plain, text/uri-list", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("WebKit", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("https://www.webkit.org/", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("WebKit", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_WK_STREQ("(STRING, text/html), (STRING, text/plain), (STRING, text/uri-list)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
}

TEST(DragAndDropTests, DataTransferSuppressGetDataDueToPresenceOfTextFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];
    [itemProvider setSuggestedName:@"hello.txt"];

    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointZero to:CGPointMake(50, 100)];

    EXPECT_WK_STREQ("Files", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_WK_STREQ("(FILE, text/plain)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('hello.txt', text/plain)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
}

TEST(DragAndDropTests, DataTransferExposePlainTextWithFileURLAsFile)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"DataTransfer"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    NSData *urlAsData = [@"file:///some/file/path.txt" dataUsingEncoding:NSUTF8StringEncoding];
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeFileURL withData:urlAsData];
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeURL withData:urlAsData];
    [itemProvider registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];

    [simulator setExternalItemProviders:@[ itemProvider.get() ]];
    [simulator runFrom:CGPointZero to:CGPointMake(50, 100)];

    EXPECT_WK_STREQ("Files", [webView stringByEvaluatingJavaScript:@"types.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"textData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"urlData.textContent"]);
    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"htmlData.textContent"]);
    EXPECT_WK_STREQ("(FILE, text/plain)", [webView stringByEvaluatingJavaScript:@"items.textContent"]);
    EXPECT_WK_STREQ("('text.txt', text/plain)", [webView stringByEvaluatingJavaScript:@"files.textContent"]);
}

TEST(DragAndDropTests, DataTransferGetDataCannotReadPrivateArbitraryTypes)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    auto itemProvider = adoptNS([[NSItemProvider alloc] init]);
    [itemProvider registerDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeMP3 visibility:NSItemProviderRepresentationVisibilityAll loadHandler:^NSProgress *(DataLoadCompletionBlock completionHandler)
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

TEST(DragAndDropTests, DataTransferSetDataValidURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'url' : 'https://webkit.org/b/123' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    __block bool done = false;
    [simulator.get() setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(1UL, session.items.count);
        auto *item = session.items[0].itemProvider;
        EXPECT_TRUE([item.registeredTypeIdentifiers containsObject:(__bridge NSString *)kUTTypeURL]);
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

TEST(DragAndDropTests, DataTransferSetDataUnescapedURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'url' : 'http://webkit.org/b/\u4F60\u597D;?x=8 + 6' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    __block bool done = false;
    [simulator.get() setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(1UL, session.items.count);
        auto *item = session.items[0].itemProvider;
        EXPECT_TRUE([item.registeredTypeIdentifiers containsObject:(__bridge NSString *)kUTTypeURL]);
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

TEST(DragAndDropTests, DataTransferSetDataInvalidURL)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'url' : 'some random string' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    [simulator runFrom:CGPointMake(50, 225) to:CGPointMake(50, 375)];
    NSArray *registeredTypes = [simulator.get().sourceItemProviders.firstObject registeredTypeIdentifiers];
    EXPECT_FALSE([registeredTypes containsObject:(__bridge NSString *)kUTTypeURL]);
    checkJSONWithLogging([webView stringByEvaluatingJavaScript:@"output.value"], @{
        @"dragover": @{
            @"text/uri-list": @"",
        },
        @"drop": @{
            @"text/uri-list": @"some random string",
        }
    });
}

TEST(DragAndDropTests, DataTransferSanitizeHTML)
{
    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView synchronouslyLoadTestPageNamed:@"dump-datatransfer-types"];
    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);

    [webView stringByEvaluatingJavaScript:@"select(rich)"];
    [webView stringByEvaluatingJavaScript:@"customData = { 'text/html' : '<meta content=\"secret\">"
        "<b onmouseover=\"dangerousCode()\">hello</b><!-- secret-->, world<script>dangerousCode()</script>' }"];
    [webView stringByEvaluatingJavaScript:@"writeCustomData = true"];

    __block bool done = false;
    [simulator.get() setOverridePerformDropBlock:^NSArray<UIDragItem *> *(id <UIDropSession> session)
    {
        EXPECT_EQ(1UL, session.items.count);
        auto *item = session.items[0].itemProvider;
        EXPECT_TRUE([item.registeredTypeIdentifiers containsObject:(__bridge NSString *)kUTTypeHTML]);
        [item loadDataRepresentationForTypeIdentifier:(__bridge NSString *)kUTTypeHTML completionHandler:^(NSData *data, NSError *error) {
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

} // namespace TestWebKitAPI

#endif // ENABLE(DRAG_SUPPORT) && PLATFORM(IOS_FAMILY) && WK_API_ENABLED
