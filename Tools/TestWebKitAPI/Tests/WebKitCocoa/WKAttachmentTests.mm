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
    [webView expectUpdatesAfterCommand:@"DeleteForward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
}

TEST(WKAttachmentTests, AttachmentUpdatesWhenUndoingAndRedoing)
{
    auto webView = webViewForTestingAttachments();
    RetainPtr<_WKAttachment> attachment;
    {
        ObserveAttachmentUpdatesForScope observer(webView.get());
        attachment = retainPtr([webView synchronouslyInsertAttachmentWithFilename:@"foo.txt" contentType:@"text/plain" data:testHTMLData() options:nil]);
        observer.expectAttachmentUpdates(@[ ], @[attachment.get()]);
    }
    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"Redo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [webView expectUpdatesAfterCommand:@"DeleteBackward" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
    [webView expectUpdatesAfterCommand:@"Undo" withArgument:nil expectedRemovals:@[] expectedInsertions:@[attachment.get()]];
    [webView expectUpdatesAfterCommand:@"Redo" withArgument:nil expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
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
    // FIXME: Additionally test underlining after <https://bugs.webkit.org/show_bug.cgi?id=179431> is addressed.

    // Inserting text should delete the current selection, removing the attachment in the process.
    [webView expectUpdatesAfterCommand:@"InsertText" withArgument:@"foo" expectedRemovals:@[attachment.get()] expectedInsertions:@[]];
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
}

} // namespace TestWebKitAPI

#endif // WK_API_ENABLED
