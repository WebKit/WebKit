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

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebKit.h>
#import <WebKit/WebKitPrivate.h>
#import <wtf/RetainPtr.h>

#if WK_API_ENABLED

@interface AttachmentUpdateObserver : NSObject <WKUIDelegatePrivate>
@property (nonatomic, readonly) NSArray *inserted;
@property (nonatomic, readonly) NSArray *removed;
@end

@implementation AttachmentUpdateObserver {
    RetainPtr<NSMutableArray<_WKAttachment *>> _inserted;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removed;
}

- (instancetype)init
{
    if (self = [super init]) {
        _inserted = adoptNS([[NSMutableArray alloc] init]);
        _removed = adoptNS([[NSMutableArray alloc] init]);
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

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment
{
    [_inserted addObject:attachment];
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
        m_previousDelegate = retainPtr(webView.UIDelegate);
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

private:
    RetainPtr<AttachmentUpdateObserver> m_observer;
    RetainPtr<TestWKWebView> m_webView;
    RetainPtr<id> m_previousDelegate;
};

}

@interface TestWKWebView (AttachmentTesting)
@end

static RetainPtr<TestWKWebView> webViewForTestingAttachments()
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body><script>document.body.focus();</script>"];

    return webView;
}

static NSData *testHTMLData()
{
    return [@"<a href='#'>This is some HTML data</a>" dataUsingEncoding:NSUTF8StringEncoding];
}

static NSData *testImageData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"icon" withExtension:@"png" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

static NSData *testVideoData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"mp4" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

static NSData *testPDFData()
{
    NSURL *url = [[NSBundle mainBundle] URLForResource:@"test" withExtension:@"pdf" subdirectory:@"TestWebKitAPI.resources"];
    return [NSData dataWithContentsOfURL:url];
}

static _WKAttachmentDisplayOptions *displayOptionsWithMode(_WKAttachmentDisplayMode mode)
{
    _WKAttachmentDisplayOptions *options = [[[_WKAttachmentDisplayOptions alloc] init] autorelease];
    options.mode = mode;
    return options;
}

@implementation TestWKWebView (AttachmentTesting)

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
        resultError = retainPtr(error);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();
}

- (NSData *)synchronouslyRequestData:(NSError **)error
{
    __block RetainPtr<NSData> result;
    __block RetainPtr<NSError> resultError;
    __block bool done = false;
    [self requestData:^(NSData *data, NSError *error) {
        result = retainPtr(data);
        resultError = retainPtr(error);
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    if (error)
        *error = resultError.autorelease();

    return result.autorelease();
}

- (void)expectRequestedDataToBe:(NSData *)expectedData
{
    NSError *dataRequestError = nil;
    NSData *observedData = [self synchronouslyRequestData:&dataRequestError];
    BOOL observedDataIsEqualToExpectedData = [observedData isEqualToData:expectedData] || observedData == expectedData;
    EXPECT_TRUE(observedDataIsEqualToExpectedData);
    if (!observedDataIsEqualToExpectedData) {
        NSLog(@"Expected data: %@ but observed: %@ for %@", [expectedData shortDescription], [observedData shortDescription], self);
        NSLog(@"Observed error: %@ while reading data for %@", dataRequestError, self);
    }
}

@end

namespace TestWebKitAPI {

TEST(WKAttachmentTests, AttachmentElementInsertion)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> firstAttachment;
    RetainPtr<_WKAttachment> secondAttachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        // Use the given content type for the attachment element's type.
        firstAttachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo" contentType:@"text/html" data:testHTMLData() options:nil]);
        EXPECT_WK_STREQ(@"foo", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"text/html", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"38 bytes", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
        observer.expectAttachmentUpdates(@[ ], @[ firstAttachment.get() ]);
    }

    [firstAttachment expectRequestedDataToBe:testHTMLData()];

    {
        ObserveAttachmentUpdatesForScope scope(webView.get());
        // Since no content type is explicitly specified, compute it from the file extension.
        [webView _executeEditCommand:@"DeleteBackward" argument:nil completion:nil];
        secondAttachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"bar.png" contentType:nil data:testImageData() options:nil]);
        EXPECT_WK_STREQ(@"bar.png", [webView valueOfAttribute:@"title" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"image/png", [webView valueOfAttribute:@"type" forQuerySelector:@"attachment"]);
        EXPECT_WK_STREQ(@"37 KB", [webView valueOfAttribute:@"subtitle" forQuerySelector:@"attachment"]);
        scope.expectAttachmentUpdates(@[ firstAttachment.get() ], @[ secondAttachment.get() ]);
    }

    [firstAttachment expectRequestedDataToBe:nil];
    [secondAttachment expectRequestedDataToBe:testImageData()];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingAndDeletingNewline)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil]);
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView stringByEvaluatingJavaScript:@"getSelection().collapse(document.body)"];
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];

    [webView expectUpdatesAfterCommand:@"DeleteForward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenUndoingAndRedoing)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> htmlData = retainPtr(testHTMLData());
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil]);
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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil]);
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertText" withArgument:@"World" expectedRemovals:@[] expectedInsertions:@[]];
    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView expectUpdatesAfterCommand:@"ToggleBold" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"ToggleItalic" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"ToggleUnderline" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];

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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil]);
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    // This edit command behaves more like a "toggle", and will actually break us out of the list we just inserted.
    [webView expectUpdatesAfterCommand:@"InsertOrderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"InsertUnorderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"InsertUnorderedList" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [attachment expectRequestedDataToBe:testHTMLData()];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenInsertingRichMarkup)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"InsertHTML" argument:@"<div><strong><attachment title='a' webkitattachmentid='a06fec41-9aa0-4c2c-ba3a-0149b54aad99'></attachment></strong></div>"];
        attachment = observer.observer().inserted[0];
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [attachment expectRequestedDataToBe:testHTMLData()];
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
}

TEST(WKAttachmentTests, AttachmentDataForEmptyFile)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"empty.txt" contentType:@"text/plain" data:[NSData data] options:nil]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [attachment expectRequestedDataToBe:[NSData data]];
    {
        ObserveAttachmentUpdatesForScope scope(webView.get());
        [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
        scope.expectAttachmentUpdates(@[attachment.get()], @[]);
    }
    [attachment expectRequestedDataToBe:nil];
}

TEST(WKAttachmentTests, MultipleSimultaneousAttachmentDataRequests)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<NSData> htmlData = testHTMLData();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:htmlData.get() options:nil]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    __block RetainPtr<NSData> dataForFirstRequest;
    __block RetainPtr<NSData> dataForSecondRequest;
    __block bool done = false;
    [attachment requestData:^(NSData *data, NSError *error) {
        EXPECT_TRUE(!error);
        dataForFirstRequest = retainPtr(data);
    }];
    [attachment requestData:^(NSData *data, NSError *error) {
        EXPECT_TRUE(!error);
        dataForSecondRequest = retainPtr(data);
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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"icon.png" contentType:@"image/png" data:imageData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeAsIcon)]);
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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"icon.png" contentType:@"image/png" data:imageData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView stringByEvaluatingJavaScript:@"getSelection().collapse(document.body)"];
    [webView expectUpdatesAfterCommand:@"InsertParagraph" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[] expectedInsertions:@[]];

    [attachment expectRequestedDataToBe:imageData.get()];
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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"test.mp4" contentType:@"video/mp4" data:videoData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)]);
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
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"test.pdf" contentType:@"application/pdf" data:pdfData.get() options:displayOptionsWithMode(_WKAttachmentDisplayModeInPlace)]);
        observer.expectAttachmentUpdates(@[], @[attachment.get()]);
        [webView waitForAttachmentElementSizeToBecome:CGSizeMake(130, 29)];
    }

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView expectUpdatesAfterCommand:@"Cut" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];

    [webView expectUpdatesAfterCommand:@"Paste" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [webView waitForAttachmentElementSizeToBecome:CGSizeMake(130, 29)];
    [attachment expectRequestedDataToBe:pdfData.get()];
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED
