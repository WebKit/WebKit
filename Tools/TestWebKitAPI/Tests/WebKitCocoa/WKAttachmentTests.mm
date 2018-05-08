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

#import "DataInteractionSimulator.h"
#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKPreferencesRefPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebKitPrivate.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

#define USES_MODERN_ATTRIBUTED_STRING_CONVERSION ((PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 110000) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101300))

#if WK_API_ENABLED && !PLATFORM(WATCHOS) && !PLATFORM(TVOS)

@interface AttachmentUpdateObserver : NSObject <WKUIDelegatePrivate>
@property (nonatomic, readonly) NSArray *inserted;
@property (nonatomic, readonly) NSArray *removed;
@end

@implementation AttachmentUpdateObserver {
    RetainPtr<NSMutableArray<_WKAttachment *>> _inserted;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removed;
    RetainPtr<NSMutableDictionary<NSString *, NSString *>> _identifierToSourceMap;
}

- (instancetype)init
{
    if (self = [super init]) {
        _inserted = adoptNS([[NSMutableArray alloc] init]);
        _removed = adoptNS([[NSMutableArray alloc] init]);
        _identifierToSourceMap = adoptNS([[NSMutableDictionary alloc] init]);
    }
    return self;
}

- (NSArray<_WKAttachment *> *)inserted
{
    return _inserted.get();
}

- (NSArray<_WKAttachment *> *)removed
{
    return _removed.get();
}

- (NSString *)sourceForIdentifier:(NSString *)identifier
{
    return [_identifierToSourceMap objectForKey:identifier];
}

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_inserted addObject:attachment];
    if (source)
        [_identifierToSourceMap setObject:source forKey:attachment.uniqueIdentifier];
}

- (void)_webView:(WKWebView *)webView didRemoveAttachment:(_WKAttachment *)attachment
{
    [_removed addObject:attachment];
}

@end

namespace TestWebKitAPI {

class ObserveAttachmentUpdatesForScope {
public:
    ObserveAttachmentUpdatesForScope(TestWKWebView *webView)
        : m_webView(webView)
    {
        m_previousDelegate = webView.UIDelegate;
        m_observer = adoptNS([[AttachmentUpdateObserver alloc] init]);
        webView.UIDelegate = m_observer.get();
    }

    ~ObserveAttachmentUpdatesForScope()
    {
        [m_webView setUIDelegate:m_previousDelegate.get()];
    }

    AttachmentUpdateObserver *observer() const { return m_observer.get(); }

    void expectAttachmentUpdates(NSArray<_WKAttachment *> *removed, NSArray<_WKAttachment *> *inserted)
    {
        BOOL removedAttachmentsMatch = [observer().removed isEqual:removed];
        if (!removedAttachmentsMatch)
            NSLog(@"Expected removed attachments: %@ to match %@.", observer().removed, removed);
        EXPECT_TRUE(removedAttachmentsMatch);

        BOOL insertedAttachmentsMatch = [observer().inserted isEqual:inserted];
        if (!insertedAttachmentsMatch)
            NSLog(@"Expected inserted attachments: %@ to match %@.", observer().inserted, inserted);
        EXPECT_TRUE(insertedAttachmentsMatch);
    }

    void expectSourceForIdentifier(NSString *expectedSource, NSString *identifier)
    {
        NSString *observedSource = [observer() sourceForIdentifier:identifier];
        BOOL success = observedSource == expectedSource || [observedSource isEqualToString:expectedSource];
        EXPECT_TRUE(success);
        if (!success)
            NSLog(@"Expected source: %@ but observed: %@", expectedSource, observedSource);
    }

private:
    RetainPtr<AttachmentUpdateObserver> m_observer;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<id> m_previousDelegate;
};

}

@interface TestWKWebView (AttachmentTesting)
@end

static RetainPtr<TestWKWebView> webViewForTestingAttachments(CGSize webViewSize, WKWebViewConfiguration *configuration)
{
    configuration._attachmentElementEnabled = YES;
    WKPreferencesSetCustomPasteboardDataEnabled((WKPreferencesRef)[configuration preferences], YES);

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, webViewSize.width, webViewSize.height) configuration:configuration]);
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width, initial-scale=1'><script>focus = () => document.body.focus()</script><body onload=focus() contenteditable></body>"];

    return webView;
}

static RetainPtr<TestWKWebView> webViewForTestingAttachments(CGSize webViewSize)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    return webViewForTestingAttachments(webViewSize, configuration.get());
}

static RetainPtr<TestWKWebView> webViewForTestingAttachments()
{
    return webViewForTestingAttachments(CGSizeMake(500, 500));
}

static NSData *testZIPData()
{
    NSURL *zipFileURL = [[NSBundle mainBundle] URLForResource:@"compressed-files" withExtension:@"zip" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:zipFileURL];
}

static NSData *testHTMLData()
{
    return [@"<a href='#'>This is some HTML data</a>" dataUsingEncoding:NSUTF8StringEncoding];
}

static NSURL *testImageFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testImageData()
{
    return [NSData dataWithContentsOfURL:testImageFileURL()];
}

static NSData *testVideoData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

static NSURL *testPDFFileURL()
{
    return [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"];
}

static NSData *testPDFData()
{
    return [NSData dataWithContentsOfURL:testPDFFileURL()];
}

static _WKAttachmentDisplayOptions *displayOptionsWithMode(_WKAttachmentDisplayMode mode)
{
    _WKAttachmentDisplayOptions *options = [[[_WKAttachmentDisplayOptions alloc] init] autorelease];
    options.mode = mode;
    return options;
}

@implementation TestWKWebView (AttachmentTesting)

- (NSArray<NSString *> *)tagsInBody
{
    return [self objectByEvaluatingJavaScript:@"Array.from(document.body.getElementsByTagName('*')).map(e => e.tagName)"];
}

- (void)expectElementTagsInOrder:(NSArray<NSString *> *)tagNames
{
    auto remainingTags = adoptNS([tagNames mutableCopy]);
    NSArray<NSString *> *tagsInBody = self.tagsInBody;
    for (NSString *tag in tagsInBody.reverseObjectEnumerator) {
        if ([tag isEqualToString:[remainingTags lastObject]])
            [remainingTags removeLastObject];
        if (![remainingTags count])
            break;
    }
    EXPECT_EQ([remainingTags count], 0U);
    if ([remainingTags count])
        NSLog(@"Expected to find ordered tags: %@ in: %@", tagNames, tagsInBody);
}

- (void)expectElementTag:(NSString *)tagName toComeBefore:(NSString *)otherTagName
{
    [self expectElementTagsInOrder:@[tagName, otherTagName]];
}

- (BOOL)_synchronouslyExecuteEditCommand:(NSString *)command argument:(NSString *)argument
{
    __block bool done = false;
    __block bool success;
    [self _executeEditCommand:command argument:argument completion:^(BOOL completionSuccess) {
        done = true;
        success = completionSuccess;
    }];
    TestWebKitAPI::Util::run(&done);
    return success;
}

- (_WKAttachment *)synchronouslyInsertAttachmentWithFilename:(NSString *)filename contentType:(NSString *)contentType data:(NSData *)data options:(_WKAttachmentDisplayOptions *)options
{
    __block bool done = false;
    RetainPtr<_WKAttachment> attachment = [self _insertAttachmentWithFilename:filename contentType:contentType data:data options:options completion:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return attachment.autorelease();
}

- (CGSize)attachmentElementSize
{
    __block CGSize size;
    __block bool doneEvaluatingScript = false;
    [self evaluateJavaScript:@"r = document.querySelector('attachment').getBoundingClientRect(); [r.width, r.height]" completionHandler:^(NSArray<NSNumber *> *sizeResult, NSError *) {
        size = CGSizeMake(sizeResult.firstObject.floatValue, sizeResult.lastObject.floatValue);
        doneEvaluatingScript = true;
    }];
    TestWebKitAPI::Util::run(&doneEvaluatingScript);
    return size;
}

- (void)waitForAttachmentElementSizeToBecome:(CGSize)expectedSize
{
    while ([[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantPast]]) {
        if (CGSizeEqualToSize(self.attachmentElementSize, expectedSize))
            break;
    }
}

- (BOOL)hasAttribute:(NSString *)attributeName forQuerySelector:(NSString *)querySelector
{
    return [self stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('%@').hasAttribute('%@')", querySelector, attributeName]].boolValue;
}

- (NSString *)valueOfAttribute:(NSString *)attributeName forQuerySelector:(NSString *)querySelector
{
    return [self stringByEvaluatingJavaScript:[NSString stringWithFormat:@"document.querySelector('%@').getAttribute('%@')", querySelector, attributeName]];
}

- (void)expectUpdatesAfterCommand:(NSString *)command withArgument:(NSString *)argument expectedRemovals:(NSArray<_WKAttachment *> *)removed expectedInsertions:(NSArray<_WKAttachment *> *)inserted
{
    TestWebKitAPI::ObserveAttachmentUpdatesForScope observer(self);
    EXPECT_TRUE([self _synchronouslyExecuteEditCommand:command argument:argument]);
    observer.expectAttachmentUpdates(removed, inserted);
}

@end

@implementation NSData (AttachmentTesting)

- (NSString *)shortDescription
{
    return [NSString stringWithFormat:@"<%tu bytes>", self.length];
}

@end

@implementation _WKAttachment (AttachmentTesting)

- (void)synchronouslySetDisplayOptions:(_WKAttachmentDisplayOptions *)options error:(NSError **)error
{
    __block RetainPtr<NSError> resultError;
    __block bool done = false;
    [self setDisplayOptions:options completion:^(NSError *error) {
        resultError = error;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();
}

- (_WKAttachmentInfo *)synchronouslyRequestInfo:(NSError **)error
{
    __block RetainPtr<_WKAttachmentInfo> result;
    __block RetainPtr<NSError> resultError;
    __block bool done = false;
    [self requestInfo:^(_WKAttachmentInfo *info, NSError *error) {
        result = info;
        resultError = error;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();

    return result.autorelease();
}

- (NSData *)synchronouslyRequestData:(NSError **)error
{
    return [self synchronouslyRequestInfo:error].data;
}

- (void)synchronouslySetData:(NSData *)data newContentType:(NSString *)newContentType newFilename:(NSString *)newFilename error:(NSError **)error
{
    __block RetainPtr<NSError> resultError;
    __block bool done = false;
    [self setData:data newContentType:newContentType newFilename:newFilename completion:^(NSError *error) {
        resultError = error;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();
}

- (void)expectRequestedDataToBe:(NSData *)expectedData
{
    NSError *requestError = nil;
    _WKAttachmentInfo *info = [self synchronouslyRequestInfo:&requestError];

    BOOL observedDataIsEqualToExpectedData = [info.data isEqualToData:expectedData] || info.data == expectedData;
    EXPECT_TRUE(observedDataIsEqualToExpectedData);
    if (!observedDataIsEqualToExpectedData) {
        NSLog(@"Expected data: %@ but observed: %@ for %@", [expectedData shortDescription], [info.data shortDescription], self);
        NSLog(@"Observed error: %@ while reading data for %@", requestError, self);
    }
}

@end

#pragma mark - Platform testing helper functions

#if PLATFORM(IOS)

typedef void(^ItemProviderDataLoadHandler)(NSData *, NSError *);

@implementation NSItemProvider (AttachmentTesting)

- (void)registerData:(NSData *)data type:(NSString *)type
{
    [self registerDataRepresentationForTypeIdentifier:type visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[protectedData = retainPtr(data)] (ItemProviderDataLoadHandler completionHandler) -> NSProgress * {
        completionHandler(protectedData.get(), nil);
        return nil;
    }];
}

- (void)expectType:(NSString *)type withData:(NSData *)expectedData
{
    BOOL containsType = [self.registeredTypeIdentifiers containsObject:type];
    EXPECT_TRUE(containsType);
    if (!containsType) {
        NSLog(@"Expected: %@ to contain %@", self, type);
        return;
    }

    __block bool done = false;
    [self loadDataRepresentationForTypeIdentifier:type completionHandler:^(NSData *observedData, NSError *error) {
        EXPECT_TRUE([observedData isEqualToData:expectedData]);
        if (![observedData isEqualToData:expectedData])
            NSLog(@"Expected data: <%tu bytes> to be equal to data: <%tu bytes>", observedData.length, expectedData.length);
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Encountered error when loading data: %@", error);
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
}

@end

#endif // PLATFORM(IOS)

void platformCopyRichTextWithMultipleAttachments()
{
    auto image = adoptNS([[NSTextAttachment alloc] initWithData:testImageData() ofType:(NSString *)kUTTypePNG]);
    auto pdf = adoptNS([[NSTextAttachment alloc] initWithData:testPDFData() ofType:(NSString *)kUTTypePDF]);
    auto zip = adoptNS([[NSTextAttachment alloc] initWithData:testZIPData() ofType:(NSString *)kUTTypeZipArchive]);

    auto richText = adoptNS([[NSMutableAttributedString alloc] init]);
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:image.get()]];
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:pdf.get()]];
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:zip.get()]];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ richText.get() ]];
#elif PLATFORM(IOS)
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:richText.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [UIPasteboard generalPasteboard].itemProviders = @[ item.get() ];
#endif
}

void platformCopyRichTextWithImage()
{
    auto richText = adoptNS([[NSMutableAttributedString alloc] init]);
    auto image = adoptNS([[NSTextAttachment alloc] initWithData:testImageData() ofType:(NSString *)kUTTypePNG]);

    [richText appendAttributedString:[[[NSAttributedString alloc] initWithString:@"Lorem ipsum "] autorelease]];
    [richText appendAttributedString:[NSAttributedString attributedStringWithAttachment:image.get()]];
    [richText appendAttributedString:[[[NSAttributedString alloc] initWithString:@" dolor sit amet."] autorelease]];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ richText.get() ]];
#elif PLATFORM(IOS)
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:richText.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [UIPasteboard generalPasteboard].itemProviders = @[ item.get() ];
#endif
}

void platformCopyPNG()
{
#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:@[NSPasteboardTypePNG] owner:nil];
    [pasteboard setData:testImageData() forType:NSPasteboardTypePNG];
#elif PLATFORM(IOS)
    UIPasteboard *pasteboard = [UIPasteboard generalPasteboard];
    auto item = adoptNS([[UIItemProvider alloc] init]);
    [item setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [item registerData:testImageData() type:(NSString *)kUTTypePNG];
    pasteboard.itemProviders = @[ item.get() ];
#endif
}

#if PLATFORM(MAC)
typedef NSImage PlatformImage;
#else
typedef UIImage PlatformImage;
#endif

PlatformImage *platformImageWithData(NSData *data)
{
#if PLATFORM(MAC)
    return [[[NSImage alloc] initWithData:data] autorelease];
#else
    return [UIImage imageWithData:data];
#endif
}

namespace TestWebKitAPI {

#pragma mark - Platform-agnostic tests

TEST(WKAttachmentTests, AttachmentElementInsertion)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> firstAttachment;
    RetainPtr<_WKAttachment> secondAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        // Use the given content type for the attachment element's type.
        firstAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo" contentType:@"text/html" data:testHTMLData() options:nil];
        EXPECT_WK_STREQ(@"foo", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/html", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"38 bytes", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[ firstAttachment.get() ]);
    }

    _WKAttachmentInfo *info = [firstAttachment synchronouslyRequestInfo:nil];
    EXPECT_TRUE([info.data isEqualToData:testHTMLData()]);
    EXPECT_TRUE([info.contentType isEqualToString:@"text/html"]);
    EXPECT_TRUE([info.name isEqualToString:@"foo"]);
    EXPECT_EQ(info.filePath.length, 0U);

    {
        ObserveAttachmentUpdatesForScope scope(webView.get());
        // Since no content type is explicitly specified, compute it from the file extension.
        [webView _executeEditCommand:@"DeleteBackward" argument:nil completion:nil];
        secondAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"bar.png" contentType:nil data:testImageData() options:nil];
        EXPECT_WK_STREQ(@"bar.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"37 KB", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
        scope.expectAttachmentUpdates(@[ firstAttachment.get() ], @[ secondAttachment.get() ]);
    }

    [firstAttachment expectRequestedDataToBe:nil];
    [secondAttachment expectRequestedDataToBe:testImageData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingAndDeletingNewline)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView stringByEvaluatingJavaScript:@"getSelection().collapse(document.body)"];
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    _WKAttachmentInfo *info = [attachment synchronouslyRequestInfo:nil];
    EXPECT_TRUE([info.data isEqualToData:testHTMLData()]);
    EXPECT_TRUE([info.contentType isEqualToString:@"text/plain"]);
    EXPECT_TRUE([info.name isEqualToString:@"foo.txt"]);
    EXPECT_EQ(info.filePath.length, 0U);

    [webView expectUpdatesAfterCommand:@"DeleteForward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenUndoingAndRedoing)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> htmlData = testHTMLData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:nil];

    [webView expectUpdatesAfterCommand:@"Redo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [attachment expectRequestedDataToBe:htmlData.get()];

    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:nil];

    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [attachment expectRequestedDataToBe:htmlData.get()];

    [webView expectUpdatesAfterCommand:@"Redo" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenChangingFontStyles)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    [webView _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Hello"];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertText" withArgument:@"World" expectedRemovals:@[] expectedInsertions:@[]];
    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView expectUpdatesAfterCommand:@"ToggleBold" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"ToggleItalic" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"ToggleUnderline" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);

    // Inserting text should delete the current selection, removing the attachment in the process.
    [webView expectUpdatesAfterCommand:@"InsertText" withArgument:@"foo" expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingLists)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    // This edit command behaves more like a "toggle", and will actually break us out of the list we just inserted.
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"InsertUnorderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"InsertUnorderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingRichMarkup)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"InsertHTML" argument:@"<div><strong><attachment src='cid:123-4567' title='a'></attachment></strong></div>"];
        attachment = observer.observer().inserted[0];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
        observer.expectSourceForIdentifier(@"cid:123-4567", [attachment uniqueIdentifier]);
    }
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').remove()"];
        [webView waitForNextPresentationUpdate];
        observer.expectAttachmentUpdates(@[attachment.get()], @[ ]);
    }
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenCuttingAndPasting)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Cut" argument:nil];
        observer.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
    [attachment expectRequestedDataToBe:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [attachment expectRequestedDataToBe:testHTMLData()];
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentbloburl" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentpath" forQuerySelector:@"attachment"]);
    EXPECT_FALSE([webView hasAttribute:@"webkitattachmentid" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTests, AttachmentDataForEmptyFile)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"empty.txt" contentType:@"text/plain" data:[NSData data] options:nil];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [attachment expectRequestedDataToBe:[NSData data]];
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    {
        ObserveAttachmentUpdatesForScope scope(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        scope.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, InsertedImageSizeIsClampedByMaxWidth)
{
    auto webView = webViewForTestingAttachments(CGSizeMake(100, 100));
    RetainPtr<NSData> imageData = testImageData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"icon.png" contentType:@"image/png" data:imageData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        [attachment expectRequestedDataToBe:imageData.get()];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(84, 67)];
}

TEST(WKAttachmentTests, MultipleSimultaneousAttachmentDataRequests)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> htmlData = testHTMLData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:htmlData.get() options:nil];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    __block RetainPtr<NSData> dataForFirstRequest;
    __block RetainPtr<NSData> dataForSecondRequest;
    __block bool done = false;
    [attachment requestInfo:^(_WKAttachmentInfo *info, NSError *error) {
        EXPECT_TRUE(!error);
        dataForFirstRequest = info.data;
    }];
    [attachment requestInfo:^(_WKAttachmentInfo *info, NSError *error) {
        EXPECT_TRUE(!error);
        dataForSecondRequest = info.data;
        done = true;
    }];

    Util::run(&done);

    EXPECT_TRUE([dataForFirstRequest isEqualToData:htmlData.get()]);
    EXPECT_TRUE([dataForSecondRequest isEqualToData:htmlData.get()]);
}

TEST(WKAttachmentTests, InPlaceImageAttachmentToggleDisplayMode)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> imageData = testImageData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"icon.png" contentType:@"image/png" data:imageData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon)];
        [attachment expectRequestedDataToBe:imageData.get()];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    CGSize iconAttachmentSize = [webView attachmentElementSize];

    [attachment synchronouslySetDisplayOptions:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace) error:nil];
    [attachment expectRequestedDataToBe:imageData.get()];
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(215, 174)];

    [attachment synchronouslySetDisplayOptions:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon) error:nil];
    [attachment expectRequestedDataToBe:imageData.get()];
    [webView waitForAttachmentElementSizeToBecome:iconAttachmentSize];

    [attachment synchronouslySetDisplayOptions:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace) error:nil];
    [attachment expectRequestedDataToBe:imageData.get()];
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(215, 174)];
}

TEST(WKAttachmentTests, InPlaceImageAttachmentParagraphInsertion)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> imageData = testImageData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"icon.png" contentType:@"image/png" data:imageData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView stringByEvaluatingJavaScript:@"getSelection().collapse(document.body)"];
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    [attachment expectRequestedDataToBe:imageData.get()];
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(215, 174)];

    [webView expectUpdatesAfterCommand:@"DeleteForward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
}

TEST(WKAttachmentTests, InPlaceVideoAttachmentInsertionWithinList)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> videoData = testVideoData();
    RetainPtr<_WKAttachment> attachment;

    [webView _synchronouslyExecuteEditCommand:@"InsertOrderedList" argument:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"test.mp4" contentType:@"video/mp4" data:videoData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(320, 240)];

    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(320, 240)];
    [attachment expectRequestedDataToBe:videoData.get()];
}

TEST(WKAttachmentTests, InPlacePDFAttachmentCutAndPaste)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> pdfData = testPDFData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"test.pdf" contentType:@"application/pdf" data:pdfData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
        [webView waitForAttachmentElementSizeToBecome:CGSizeMake(130, 29)];
    }

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView expectUpdatesAfterCommand:@"Cut" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];

    [webView expectUpdatesAfterCommand:@"Paste" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(130, 29)];
    [attachment expectRequestedDataToBe:pdfData.get()];
}

TEST(WKAttachmentTests, ChangeAttachmentDataAndFileInformation)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        RetainPtr<NSData> pdfData = testPDFData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"test.pdf" contentType:@"application/pdf" data:pdfData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon)];
        [attachment expectRequestedDataToBe:pdfData.get()];
        EXPECT_WK_STREQ(@"test.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    {
        RetainPtr<NSData> imageData = testImageData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetData:imageData.get() newContentType:@"image/png" newFilename:@"icon.png" error:nil];
        [attachment expectRequestedDataToBe:imageData.get()];
        EXPECT_WK_STREQ(@"icon.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[]);
    }
    {
        RetainPtr<NSData> textData = testHTMLData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetDisplayOptions:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon) error:nil];
        // The new content type should be inferred from the file name.
        [attachment synchronouslySetData:textData.get() newContentType:nil newFilename:@"foo.txt" error:nil];
        [attachment expectRequestedDataToBe:textData.get()];
        EXPECT_WK_STREQ(@"foo.txt", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/plain", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[]);
    }
    {
        RetainPtr<NSData> secondTextData = [@"Hello world" dataUsingEncoding:NSUTF8StringEncoding];
        ObserveAttachmentUpdatesForScope observer(webView.get());
        // Both the previous file name and type should be inferred.
        [attachment synchronouslySetData:secondTextData.get() newContentType:nil newFilename:nil error:nil];
        [attachment expectRequestedDataToBe:secondTextData.get()];
        EXPECT_WK_STREQ(@"foo.txt", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/plain", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[]);
    }
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
}

TEST(WKAttachmentTests, ChangeAttachmentDataUpdatesWithInPlaceDisplay)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        RetainPtr<NSData> pdfData = testPDFData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"test.pdf" contentType:@"application/pdf" data:pdfData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        [webView waitForAttachmentElementSizeToBecome:CGSizeMake(130, 29)];
        [attachment expectRequestedDataToBe:pdfData.get()];
        EXPECT_WK_STREQ(@"test.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    {
        RetainPtr<NSData> videoData = testVideoData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetData:videoData.get() newContentType:@"video/mp4" newFilename:@"test.mp4" error:nil];
        [webView waitForAttachmentElementSizeToBecome:CGSizeMake(320, 240)];
        [attachment expectRequestedDataToBe:videoData.get()];
        EXPECT_WK_STREQ(@"test.mp4", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"video/mp4", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[]);
    }
    {
        RetainPtr<NSData> imageData = testImageData();
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [attachment synchronouslySetData:imageData.get() newContentType:@"image/png" newFilename:@"icon.png" error:nil];
        [webView waitForAttachmentElementSizeToBecome:CGSizeMake(215, 174)];
        [attachment expectRequestedDataToBe:imageData.get()];
        EXPECT_WK_STREQ(@"icon.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[], @[]);
    }
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
}

TEST(WKAttachmentTests, InsertPastedImageAsAttachment)
{
    platformCopyPNG();

    RetainPtr<_WKAttachment> attachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(1U, observer.observer().inserted.count);
        attachment = observer.observer().inserted[0];
    }

    auto size = platformImageWithData([attachment synchronouslyRequestData:nil]).size;
    EXPECT_EQ(215., size.width);
    EXPECT_EQ(174., size.height);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
}

TEST(WKAttachmentTests, InsertPastedAttributedStringContainingImage)
{
    platformCopyRichTextWithImage();

    RetainPtr<_WKAttachment> attachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(1U, observer.observer().inserted.count);
        attachment = observer.observer().inserted[0];
    }

    [attachment expectRequestedDataToBe:testImageData()];
    EXPECT_WK_STREQ("Lorem ipsum  dolor sit amet.", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
    EXPECT_WK_STREQ("image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
}

TEST(WKAttachmentTests, InsertPastedAttributedStringContainingMultipleAttachments)
{
    platformCopyRichTextWithMultipleAttachments();

    RetainPtr<_WKAttachment> imageAttachment;
    RetainPtr<_WKAttachment> zipAttachment;
    RetainPtr<_WKAttachment> pdfAttachment;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(3U, observer.observer().inserted.count);
        for (_WKAttachment *attachment in observer.observer().inserted) {
            NSData *data = [attachment synchronouslyRequestData:nil];
            if ([data isEqualToData:testZIPData()])
                zipAttachment = attachment;
            else if ([data isEqualToData:testPDFData()])
                pdfAttachment = attachment;
            else if ([data isEqualToData:testImageData()])
                imageAttachment = attachment;
        }
    }

    EXPECT_TRUE(zipAttachment && imageAttachment && pdfAttachment);
    EXPECT_EQ(3, [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment').length"].integerValue);
    EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"]);
    EXPECT_WK_STREQ("application/pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"]);

    NSString *zipAttachmentType = [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[2].getAttribute('type')"];
#if USES_MODERN_ATTRIBUTED_STRING_CONVERSION
    EXPECT_WK_STREQ("application/zip", zipAttachmentType);
#else
    EXPECT_WK_STREQ("application/octet-stream", zipAttachmentType);
#endif

    EXPECT_WK_STREQ([imageAttachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].uniqueIdentifier"]);
    EXPECT_WK_STREQ([pdfAttachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].uniqueIdentifier"]);
    EXPECT_WK_STREQ([zipAttachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[2].uniqueIdentifier"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        NSArray<_WKAttachment *> *removedAttachments = [observer.observer() removed];
        EXPECT_EQ(3U, removedAttachments.count);
        EXPECT_TRUE([removedAttachments containsObject:zipAttachment.get()]);
        EXPECT_TRUE([removedAttachments containsObject:imageAttachment.get()]);
        EXPECT_TRUE([removedAttachments containsObject:pdfAttachment.get()]);
    }
}

TEST(WKAttachmentTests, DoNotInsertDataURLImagesAsAttachments)
{
    auto webContentSourceView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 100, 100)]);
    [webContentSourceView synchronouslyLoadTestPageNamed:@"apple-data-url"];
    [webContentSourceView selectAll:nil];
    [webContentSourceView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().inserted.count);
    }

    EXPECT_FALSE([webView stringByEvaluatingJavaScript:@"Boolean(document.querySelector('attachment'))"].boolValue);
    EXPECT_EQ(1990, [webView stringByEvaluatingJavaScript:@"document.querySelector('img').src.length"].integerValue);
    EXPECT_WK_STREQ("This is an apple", [webView stringByEvaluatingJavaScript:@"document.body.textContent"]);
}

TEST(WKAttachmentTests, InsertAndRemoveDuplicateAttachment)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> data = testHTMLData();
    RetainPtr<_WKAttachment> originalAttachment;
    RetainPtr<_WKAttachment> pastedAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        originalAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:data.get() options:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        observer.expectAttachmentUpdates(@[], @[originalAttachment.get()]);
    }
    [webView selectAll:nil];
    [webView _executeEditCommand:@"Copy" argument:nil completion:nil];
    [webView evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(1U, observer.observer().inserted.count);
        pastedAttachment = observer.observer().inserted.firstObject;
        EXPECT_FALSE([pastedAttachment isEqual:originalAttachment.get()]);
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[pastedAttachment.get()], @[]);
        [originalAttachment expectRequestedDataToBe:data.get()];
    }
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[originalAttachment.get()], @[]);
    }
}

TEST(WKAttachmentTests, InsertDuplicateAttachmentAndUpdateData)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> originalData = testHTMLData();
    RetainPtr<_WKAttachment> originalAttachment;
    RetainPtr<_WKAttachment> pastedAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        originalAttachment = [webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:originalData.get() options:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        observer.expectAttachmentUpdates(@[], @[originalAttachment.get()]);
    }
    [webView selectAll:nil];
    [webView _executeEditCommand:@"Copy" argument:nil completion:nil];
    [webView evaluateJavaScript:@"getSelection().collapseToEnd()" completionHandler:nil];
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().removed.count);
        EXPECT_EQ(1U, observer.observer().inserted.count);
        pastedAttachment = observer.observer().inserted.firstObject;
        EXPECT_FALSE([pastedAttachment isEqual:originalAttachment.get()]);
    }
    auto updatedData = retainPtr([@"HELLO WORLD" dataUsingEncoding:NSUTF8StringEncoding]);
    [originalAttachment synchronouslySetData:updatedData.get() newContentType:nil newFilename:nil error:nil];
    [originalAttachment expectRequestedDataToBe:updatedData.get()];
    [pastedAttachment expectRequestedDataToBe:originalData.get()];
}

TEST(WKAttachmentTests, InjectedBundleReplaceURLsWhenPastingAttributedString)
{
    platformCopyRichTextWithMultipleAttachments();

    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"]);
    [[configuration processPool] _setObject:@{ @"image/png" : @"cid:foo-bar" } forBundleParameter:@"MIMETypeToReplacementURLMap"];
    auto webView = webViewForTestingAttachments(CGSizeMake(500, 500), configuration.get());
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(2U, observer.observer().inserted.count);
    }
    [webView expectElementTagsInOrder:@[ @"IMG", @"ATTACHMENT", @"ATTACHMENT" ]];
    EXPECT_WK_STREQ("cid:foo-bar", [webView valueOfAttribute:@"src" forQuerySelector:@"img"]);
}

TEST(WKAttachmentTests, InjectedBundleReplaceURLWhenPastingImage)
{
    platformCopyPNG();

    NSString *replacementURL = @"cid:foo-bar";
    auto configuration = retainPtr([WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"BundleEditingDelegatePlugIn"]);
    [[configuration processPool] _setObject:@{ @"image/tiff" : replacementURL, @"image/png" : replacementURL } forBundleParameter:@"MIMETypeToReplacementURLMap"];
    auto webView = webViewForTestingAttachments(CGSizeMake(500, 500), configuration.get());
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        EXPECT_EQ(0U, observer.observer().inserted.count);
    }
    EXPECT_WK_STREQ("cid:foo-bar", [webView valueOfAttribute:@"src" forQuerySelector:@"img"]);
}

#pragma mark - Platform-specific tests

#if PLATFORM(MAC)

TEST(WKAttachmentTestsMac, InsertPastedFileURLsAsAttachments)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard declareTypes:@[NSFilenamesPboardType] owner:nil];
    [pasteboard setPropertyList:@[testPDFFileURL().path, testImageFileURL().path] forType:NSFilenamesPboardType];

    RetainPtr<NSArray<_WKAttachment *>> insertedAttachments;
    auto webView = webViewForTestingAttachments();
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
        insertedAttachments = [observer.observer() inserted];
        EXPECT_EQ(2U, [insertedAttachments count]);
    }

    NSArray<NSData *> *expectedAttachmentData = @[ testPDFData(), testImageData() ];
    EXPECT_TRUE([expectedAttachmentData containsObject:[[insertedAttachments firstObject] synchronouslyRequestData:nil]]);
    EXPECT_TRUE([expectedAttachmentData containsObject:[[insertedAttachments lastObject] synchronouslyRequestData:nil]]);
    EXPECT_WK_STREQ("application/pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"]);
    EXPECT_WK_STREQ("test.pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('title')"]);
    EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"]);
    EXPECT_WK_STREQ("icon.png", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('title')"]);

    for (_WKAttachment *attachment in insertedAttachments.get()) {
        _WKAttachmentInfo *info = [attachment synchronouslyRequestInfo:nil];
        EXPECT_GT(info.filePath.length, 0U);
    }

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        NSArray<_WKAttachment *> *removedAttachments = [observer.observer() removed];
        EXPECT_EQ(2U, removedAttachments.count);
        EXPECT_TRUE([removedAttachments containsObject:[insertedAttachments firstObject]]);
        EXPECT_TRUE([removedAttachments.lastObject isEqual:[insertedAttachments lastObject]]);
    }
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS)

TEST(WKAttachmentTestsIOS, InsertDroppedImageAsAttachment)
{
    auto webView = webViewForTestingAttachments();
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerData:testImageData() type:(NSString *)kUTTypePNG];
    [draggingSimulator setExternalItemProviders:@[ item.get() ]];
    [draggingSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(1U, [draggingSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [draggingSimulator removedAttachments].count);
    auto attachment = retainPtr([draggingSimulator insertedAttachments].firstObject);
    [attachment expectRequestedDataToBe:testImageData()];
    EXPECT_WK_STREQ("public.png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
}

TEST(WKAttachmentTestsIOS, InsertDroppedAttributedStringContainingAttachment)
{
    auto webView = webViewForTestingAttachments();
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    auto image = adoptNS([[NSTextAttachment alloc] initWithData:testImageData() ofType:(NSString *)kUTTypePNG]);
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:[NSAttributedString attributedStringWithAttachment:image.get()] visibility:NSItemProviderRepresentationVisibilityAll];

    [draggingSimulator setExternalItemProviders:@[ item.get() ]];
    [draggingSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(1U, [draggingSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [draggingSimulator removedAttachments].count);
    auto attachment = retainPtr([draggingSimulator insertedAttachments].firstObject);

    auto size = platformImageWithData([attachment synchronouslyRequestData:nil]).size;
    EXPECT_EQ(215., size.width);
    EXPECT_EQ(174., size.height);
    EXPECT_WK_STREQ("image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        observer.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
}

TEST(WKAttachmentTestsIOS, InsertDroppedRichAndPlainTextFilesAsAttachments)
{
    // Here, both rich text and plain text are content types that WebKit already understands how to insert in editable
    // areas in the absence of attachment elements. However, due to the explicitly set attachment presentation style
    // on the item providers, we should instead treat them as dropped files and insert attachment elements.
    // This exercises the scenario of dragging rich and plain text files from Files to Mail.
    auto richTextItem = adoptNS([[NSItemProvider alloc] init]);
    auto richText = adoptNS([[NSAttributedString alloc] initWithString:@"Hello world" attributes:@{ NSFontAttributeName: [UIFont boldSystemFontOfSize:12] }]);
    [richTextItem setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [richTextItem registerObject:richText.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [richTextItem setSuggestedName:@"hello.rtf"];

    auto plainTextItem = adoptNS([[NSItemProvider alloc] init]);
    [plainTextItem setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [plainTextItem registerObject:@"Hello world" visibility:NSItemProviderRepresentationVisibilityAll];
    [plainTextItem setSuggestedName:@"world.txt"];

    auto webView = webViewForTestingAttachments();
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator setExternalItemProviders:@[ richTextItem.get(), plainTextItem.get() ]];
    [draggingSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(2U, [draggingSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [draggingSimulator removedAttachments].count);

    for (_WKAttachment *attachment in [draggingSimulator insertedAttachments]) {
        NSError *error = nil;
        EXPECT_GT([attachment synchronouslyRequestData:&error].length, 0U);
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Error: %@", error);
    }

    EXPECT_EQ(2, [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment').length"].intValue);
    EXPECT_WK_STREQ("hello.rtf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('title')"]);
    EXPECT_WK_STREQ("text/rtf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"]);
    EXPECT_WK_STREQ("world.txt", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('title')"]);
    EXPECT_WK_STREQ("text/plain", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"]);
}

TEST(WKAttachmentTestsIOS, InsertDroppedZipArchiveAsAttachment)
{
    // Since WebKit doesn't have any default DOM representation for ZIP archives, we should fall back to inserting
    // attachment elements. This exercises the flow of dragging a ZIP file from an app that doesn't specify a preferred
    // presentation style (e.g. Notes) into Mail.
    auto item = adoptNS([[NSItemProvider alloc] init]);
    NSData *data = testZIPData();
    [item registerData:data type:(NSString *)kUTTypeZipArchive];
    [item setSuggestedName:@"archive.zip"];

    auto webView = webViewForTestingAttachments();
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator setExternalItemProviders:@[ item.get() ]];
    [draggingSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(1U, [draggingSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [draggingSimulator removedAttachments].count);
    [[draggingSimulator insertedAttachments].firstObject expectRequestedDataToBe:data];
    EXPECT_EQ(1, [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment').length"].intValue);
    EXPECT_WK_STREQ("archive.zip", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/zip", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
}

TEST(WKAttachmentTestsIOS, InsertDroppedItemProvidersInOrder)
{
    // Tests that item providers are inserted in the order they are specified. In this case, the two inserted attachments
    // should be separated by a link.
    auto firstAttachmentItem = adoptNS([[NSItemProvider alloc] init]);
    [firstAttachmentItem setPreferredPresentationStyle:UIPreferredPresentationStyleAttachment];
    [firstAttachmentItem registerObject:@"FIRST" visibility:NSItemProviderRepresentationVisibilityAll];
    [firstAttachmentItem setSuggestedName:@"first.txt"];

    auto inlineTextItem = adoptNS([[NSItemProvider alloc] init]);
    auto appleURL = retainPtr([NSURL URLWithString:@"https://www.apple.com/"]);
    [inlineTextItem registerObject:appleURL.get() visibility:NSItemProviderRepresentationVisibilityAll];

    auto secondAttachmentItem = adoptNS([[NSItemProvider alloc] init]);
    [secondAttachmentItem registerData:testPDFData() type:(NSString *)kUTTypePDF];
    [secondAttachmentItem setSuggestedName:@"second.pdf"];

    auto webView = webViewForTestingAttachments();
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator setExternalItemProviders:@[ firstAttachmentItem.get(), inlineTextItem.get(), secondAttachmentItem.get() ]];
    [draggingSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    EXPECT_EQ(2U, [draggingSimulator insertedAttachments].count);
    EXPECT_EQ(0U, [draggingSimulator removedAttachments].count);

    for (_WKAttachment *attachment in [draggingSimulator insertedAttachments]) {
        NSError *error = nil;
        EXPECT_GT([attachment synchronouslyRequestData:&error].length, 0U);
        EXPECT_TRUE(!error);
        if (error)
            NSLog(@"Error: %@", error);
    }

    [webView expectElementTagsInOrder:@[ @"ATTACHMENT", @"A", @"ATTACHMENT" ]];

    EXPECT_WK_STREQ("first.txt", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('title')"]);
    EXPECT_WK_STREQ("text/plain", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[0].getAttribute('type')"]);
    EXPECT_WK_STREQ([appleURL absoluteString], [webView valueOfAttribute:@"href" forQuerySelector:@"a"]);
    EXPECT_WK_STREQ("second.pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('title')"]);
    EXPECT_WK_STREQ("application/pdf", [webView stringByEvaluatingJavaScript:@"document.querySelectorAll('attachment')[1].getAttribute('type')"]);
}

TEST(WKAttachmentTestsIOS, DragAttachmentInsertedAsFile)
{
    auto item = adoptNS([[NSItemProvider alloc] init]);
    auto data = retainPtr(testPDFData());
    [item registerData:data.get() type:(NSString *)kUTTypePDF];
    [item setSuggestedName:@"document.pdf"];

    auto webView = webViewForTestingAttachments();
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator setExternalItemProviders:@[ item.get() ]];
    [draggingSimulator runFrom:CGPointZero to:CGPointMake(50, 50)];

    // First, verify that the attachment was successfully dropped.
    EXPECT_EQ(1U, [draggingSimulator insertedAttachments].count);
    _WKAttachment *attachment = [draggingSimulator insertedAttachments].firstObject;
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("document.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView evaluateJavaScript:@"getSelection().removeAllRanges()" completionHandler:nil];
    [draggingSimulator setExternalItemProviders:@[ ]];
    [draggingSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(-100, -100)];

    // Next, verify that dragging the attachment produces an item provider with a PDF attachment.
    EXPECT_EQ(1U, [draggingSimulator sourceItemProviders].count);
    NSItemProvider *itemProvider = [draggingSimulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleAttachment, itemProvider.preferredPresentationStyle);
    [itemProvider expectType:(NSString *)kUTTypePDF withData:data.get()];
    EXPECT_WK_STREQ("document.pdf", [itemProvider suggestedName]);
    [draggingSimulator endDataTransfer];
}

TEST(WKAttachmentTestsIOS, DragAttachmentInsertedAsData)
{
    auto webView = webViewForTestingAttachments();
    auto data = retainPtr(testPDFData());
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"document.pdf" contentType:@"application/pdf" data:data.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }

    // First, verify that the attachment was successfully inserted from raw data.
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("document.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView evaluateJavaScript:@"getSelection().removeAllRanges()" completionHandler:nil];
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(-100, -100)];

    // Next, verify that dragging the attachment produces an item provider with a PDF attachment.
    EXPECT_EQ(1U, [draggingSimulator sourceItemProviders].count);
    NSItemProvider *itemProvider = [draggingSimulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleAttachment, itemProvider.preferredPresentationStyle);
    [itemProvider expectType:(NSString *)kUTTypePDF withData:data.get()];
    EXPECT_WK_STREQ("document.pdf", [itemProvider suggestedName]);
    [draggingSimulator endDataTransfer];
}

TEST(WKAttachmentTestsIOS, DragInPlaceVideoAttachmentElement)
{
    auto webView = webViewForTestingAttachments();
    auto data = retainPtr(testVideoData());
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"video.mp4" contentType:@"video/mp4" data:data.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }

    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(320, 240)];
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("video.mp4", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("video/mp4", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView evaluateJavaScript:@"getSelection().removeAllRanges()" completionHandler:nil];
    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(-100, -100)];

    EXPECT_EQ(1U, [draggingSimulator sourceItemProviders].count);
    NSItemProvider *itemProvider = [draggingSimulator sourceItemProviders].firstObject;
    EXPECT_EQ(UIPreferredPresentationStyleAttachment, itemProvider.preferredPresentationStyle);
    [itemProvider expectType:(NSString *)kUTTypeMPEG4 withData:data.get()];
    EXPECT_WK_STREQ("video.mp4", [itemProvider suggestedName]);
    [draggingSimulator endDataTransfer];
}

TEST(WKAttachmentTestsIOS, MoveAttachmentElementAsIconByDragging)
{
    auto webView = webViewForTestingAttachments();
    auto data = retainPtr(testPDFData());
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"document.pdf" contentType:@"application/pdf" data:data.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }

    [webView _executeEditCommand:@"InsertParagraph" argument:nil completion:nil];
    [webView _executeEditCommand:@"InsertHTML" argument:@"<strong>text</strong>" completion:nil];
    [webView _synchronouslyExecuteEditCommand:@"InsertParagraph" argument:nil];
    [webView expectElementTag:@"ATTACHMENT" toComeBefore:@"STRONG"];

    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(25, 425)];

    attachment = [[draggingSimulator insertedAttachments] firstObject];
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("document.pdf", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("application/pdf", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView expectElementTag:@"STRONG" toComeBefore:@"ATTACHMENT"];
    [draggingSimulator endDataTransfer];
}

TEST(WKAttachmentTestsIOS, MoveInPlaceAttachmentElementByDragging)
{
    auto webView = webViewForTestingAttachments();
    auto data = retainPtr(testImageData());
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = [webView synchronouslyInsertAttachmentWithFilename:@"icon.png" contentType:@"image/png" data:data.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)];
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }

    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(215, 174)];
    [webView _executeEditCommand:@"InsertParagraph" argument:nil completion:nil];
    [webView _executeEditCommand:@"InsertHTML" argument:@"<strong>text</strong>" completion:nil];
    [webView _synchronouslyExecuteEditCommand:@"InsertParagraph" argument:nil];
    [webView expectElementTag:@"ATTACHMENT" toComeBefore:@"STRONG"];

    auto draggingSimulator = adoptNS([[DataInteractionSimulator alloc] initWithWebView:webView.get()]);
    [draggingSimulator runFrom:CGPointMake(25, 25) to:CGPointMake(25, 425)];

    attachment = [[draggingSimulator insertedAttachments] firstObject];
    [attachment expectRequestedDataToBe:data.get()];
    EXPECT_WK_STREQ("icon.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
    EXPECT_WK_STREQ("image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);

    [webView expectElementTag:@"STRONG" toComeBefore:@"ATTACHMENT"];
    [draggingSimulator endDataTransfer];
}

#endif // PLATFORM(IOS)

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED && !PLATFORM(WATCHOS) && !PLATFORM(TVOS)
