/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(MULTI_REPRESENTATION_HEIC)

#import "DragAndDropSimulator.h"
#import "PlatformUtilities.h"
#import "TestInputDelegate.h"
#import "TestWKWebView.h"
#import <CoreText/CTAdaptiveImageGlyphPriv.h>
#import <UIFoundation/NSAdaptiveImageGlyph.h>
#import <UIFoundation/NSAttributedString_Private.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/FontCocoa.h>
#import <WebKit/WebKitPrivate.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>
#import <pal/spi/cocoa/UIFoundationSPI.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UITextInput_Private.h>
#else
#import <AppKit/NSTextInputClient_Private.h>
#endif

#if USE(APPKIT)

static NSData *readRTFDataFromPasteboard()
{
    return [[NSPasteboard generalPasteboard] dataForType:NSPasteboardTypeRTFD];
}

#else

static NSData *readRTFDataFromPasteboard()
{
    id value = [[UIPasteboard generalPasteboard] valueForPasteboardType:UTTypeFlatRTFD.identifier];
    ASSERT([value isKindOfClass:[NSData class]]);
    return value;
}

#endif

@interface AdaptiveImageGlyphWKWebView : TestWKWebView<WKUIDelegatePrivate>
- (void)focusElementAndEnsureEditorStateUpdate:(NSString *)element;
- (BOOL)canInsertAdaptiveImageGlyphs;
- (NSAttributedString *)contentsAsAttributedString;

@property (nonatomic, readonly) NSArray<_WKAttachment *> *insertedAttachments;
@property (nonatomic, readonly) NSArray<_WKAttachment *> *removedAttachments;
@end

@implementation AdaptiveImageGlyphWKWebView {
    RetainPtr<NSMutableArray<_WKAttachment *>> _insertedAttachments;
    RetainPtr<NSMutableArray<_WKAttachment *>> _removedAttachments;
#if PLATFORM(IOS_FAMILY)
    RetainPtr<TestInputDelegate> _inputDelegate;
#endif
}

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration
{
    if (!(self = [super initWithFrame:frame configuration:configuration]))
        return nil;

    _insertedAttachments = adoptNS([[NSMutableArray alloc] init]);
    _removedAttachments = adoptNS([[NSMutableArray alloc] init]);

    [self setUIDelegate:self];

#if PLATFORM(IOS_FAMILY)
    _inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [_inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [self _setInputDelegate:_inputDelegate.get()];
#endif

    return self;
}

- (void)focusElementAndEnsureEditorStateUpdate:(NSString *)element
{
    NSString *script = [NSString stringWithFormat:@"%@.focus()", element];
#if PLATFORM(IOS_FAMILY)
    [self evaluateJavaScriptAndWaitForInputSessionToChange:script];
#else
    [self stringByEvaluatingJavaScript:script];
#endif
    [self waitForNextPresentationUpdate];
}

- (BOOL)canInsertAdaptiveImageGlyphs
{
#if PLATFORM(IOS_FAMILY)
    return [[self textInputContentView] supportsAdaptiveImageGlyph];
#else
    return [(id<NSTextInputClient>)self supportsAdaptiveImageGlyph];
#endif
}

- (void)insertAdaptiveImageGlyph:(NSAdaptiveImageGlyph *)adaptiveImageGlyph
{
#if PLATFORM(IOS_FAMILY)
    RetainPtr range = adoptNS([[UITextRange alloc] init]);
    [[self textInputContentView] insertAdaptiveImageGlyph:adaptiveImageGlyph replacementRange:range.get()];
#else
    [(id<NSTextInputClient>)self insertAdaptiveImageGlyph:adaptiveImageGlyph replacementRange:NSMakeRange(0, 0)];
#endif
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return _insertedAttachments.get();
}

- (NSArray<_WKAttachment *> *)removedAttachments
{
    return _removedAttachments.get();
}

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_insertedAttachments addObject:attachment];
}

- (void)_webView:(WKWebView *)webView didRemoveAttachment:(_WKAttachment *)attachment
{
    [_removedAttachments addObject:attachment];
}

- (NSAttributedString *)contentsAsAttributedString
{
    __block bool finished = false;
    __block RetainPtr<NSAttributedString> result;
    [self _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *string, NSDictionary *, NSError *) {
        result = string;
        finished = true;
    }];
    TestWebKitAPI::Util::run(&finished);
    return result.autorelease();
}

@end

static RetainPtr<WKWebViewConfiguration> configurationForAdaptiveImageGlyphWebView()
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setSupportsAdaptiveImageGlyph:YES];
    return configuration;
}

namespace TestWebKitAPI {

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphNotEditing)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphEditableView)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    EXPECT_TRUE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphContenteditable)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphContenteditablePlaintextOnly)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    [webView synchronouslyLoadHTMLString:@"<body contenteditable=\"plaintext-only\"></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphInputElement)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);

    [webView synchronouslyLoadHTMLString:@"<body><input id=\"input\"></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"input"];

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphNotEditingWithConfiguration)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configurationForAdaptiveImageGlyphWebView().get()]);

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphEditableViewWithConfiguration)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configurationForAdaptiveImageGlyphWebView().get()]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    EXPECT_TRUE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphContenteditableWithConfiguration)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configurationForAdaptiveImageGlyphWebView().get()]);

    [webView synchronouslyLoadHTMLString:@"<body contenteditable></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    EXPECT_TRUE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphContenteditablePlaintextOnlyWithConfiguration)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configurationForAdaptiveImageGlyphWebView().get()]);

    [webView synchronouslyLoadHTMLString:@"<body contenteditable=\"plaintext-only\"></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, SupportsAdaptiveImageGlyphInputElementWithConfiguration)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configurationForAdaptiveImageGlyphWebView().get()]);

    [webView synchronouslyLoadHTMLString:@"<body><input id=\"input\"></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"input"];

    EXPECT_FALSE([webView canInsertAdaptiveImageGlyphs]);
}

TEST(AdaptiveImageGlyph, InsertAdaptiveImageGlyphAsPictureElement)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];

    // Has <picture> element.
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!!document.querySelector('picture')"].boolValue);

    // <picture> element has two children, a <source> and an <img>.
    EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children.length"].intValue, 2);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[0].tagName"] isEqualToString:@"SOURCE"]);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[1].tagName"] isEqualToString:@"IMG"]);

    // <source> has an srcset and a type.
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[0].srcset"] hasPrefix:@"blob:"]);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[0].type"] isEqualToString:@"image/x-apple-adaptive-glyph"]);

    // <img> has an src.
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[1].src"] hasPrefix:@"blob:"]);

    // <img> has alt text.
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[1].alt"], "dog in a spacesuit");

    // <img> uses <source>.
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[1].currentSrc"], [webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[0].srcset"]);

    // <img> has completed loading.
    EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.querySelector('picture').children[1].complete"].boolValue);

    // <img> has non-zero dimensions.
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture').children[1]).width"], "21px");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture').children[1]).height"], "26.25px");
}

TEST(AdaptiveImageGlyph, InsertAdaptiveImageGlyphAtLargerFontSize)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body style='font-size: 64px;'></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture').children[1]).width"], "64px");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture').children[1]).height"], "84px");
}

TEST(AdaptiveImageGlyph, InsertAdaptiveImageGlyphMatchStyle)
{
    RetainPtr webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body><span style='font-size: 64px;'>Test</span></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    NSString *setSelectionJavaScript = @""
        "(() => {"
        "  const range = document.createRange();"
        "  range.setStart(document.querySelector('span').firstChild, 4);"
        "  range.setEnd(document.querySelector('span').firstChild, 4);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";
    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture').children[1]).fontSize"], "64px");
}

TEST(AdaptiveImageGlyph, InsertAndRemoveWKAttachments)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    auto attachmentSortFunction = ^(_WKAttachment *a, _WKAttachment *b) {
        return [a.info.contentType compare:b.info.contentType];
    };

    auto insertedAttachments = [[webView insertedAttachments] sortedArrayUsingComparator:attachmentSortFunction];
    EXPECT_EQ(insertedAttachments.count, 2U);
    EXPECT_EQ([[webView removedAttachments] count], 0U);

    auto heicAttachment = [insertedAttachments objectAtIndex:0];
    auto heicAttachmentInfo = heicAttachment.info;
    EXPECT_WK_STREQ(heicAttachmentInfo.contentType, "image/heic");
    EXPECT_TRUE([heicAttachmentInfo.name containsString:@"heic"]);
    EXPECT_GE(heicAttachmentInfo.data.length, 24374U);
    EXPECT_TRUE(heicAttachmentInfo.shouldPreserveFidelity);

    auto pngAttachment = [insertedAttachments objectAtIndex:1];
    auto pngAttachmentInfo =  pngAttachment.info;
    EXPECT_WK_STREQ(pngAttachmentInfo.contentType, "image/png");
    EXPECT_TRUE([pngAttachmentInfo.name containsString:@"png"]);
    EXPECT_GE(pngAttachmentInfo.data.length, 1901U);
    EXPECT_FALSE(pngAttachmentInfo.shouldPreserveFidelity);

    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('source').attachmentIdentifier"] isEqualToString:heicAttachment.uniqueIdentifier]);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"] isEqualToString:pngAttachment.uniqueIdentifier]);

    [webView _synchronouslyExecuteEditCommand:@"SelectAll" argument:nil];
    [webView _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];

    auto removedAttachments = [[webView removedAttachments] sortedArrayUsingComparator:attachmentSortFunction];
    EXPECT_EQ(removedAttachments.count, 2U);
    EXPECT_EQ([[webView insertedAttachments] count], 2U);

    auto removedHEICAttachmentInfo = [removedAttachments objectAtIndex:0].info;
    EXPECT_WK_STREQ(removedHEICAttachmentInfo.contentType, "image/heic");
    EXPECT_TRUE([removedHEICAttachmentInfo.name containsString:@"heic"]);
    EXPECT_GE(removedHEICAttachmentInfo.data.length, 24374U);
    EXPECT_TRUE(removedHEICAttachmentInfo.shouldPreserveFidelity);

    auto removedPNGAttachmentInfo =  [removedAttachments objectAtIndex:1].info;
    EXPECT_WK_STREQ(removedPNGAttachmentInfo.contentType, "image/png");
    EXPECT_TRUE([removedPNGAttachmentInfo.name containsString:@"png"]);
    EXPECT_GE(removedPNGAttachmentInfo.data.length, 1901U);
    EXPECT_FALSE(removedPNGAttachmentInfo.shouldPreserveFidelity);
}

TEST(AdaptiveImageGlyph, InsertWKAttachmentsOnPaste)
{
    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    RetainPtr font = [WebCore::CocoaFont systemFontOfSize:16];

    RetainPtr attributedString = [NSAttributedString attributedStringWithAdaptiveImageGlyph:adaptiveImageGlyph.get() attributes:@{ NSFontAttributeName : font.get() }];

#if PLATFORM(MAC)
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[ attributedString.get() ]];
#elif PLATFORM(APPLETV) || PLATFORM(WATCHOS)
    RetainPtr stringData = [attributedString dataFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:@{ } error:nil];
    [[UIPasteboard generalPasteboard] setData:stringData.get() forPasteboardType:UTTypeFlatRTFD.identifier];
#else
    auto item = adoptNS([[NSItemProvider alloc] init]);
    [item registerObject:attributedString.get() visibility:NSItemProviderRepresentationVisibilityAll];
    [UIPasteboard generalPasteboard].itemProviders = @[ item.get() ];
#endif

    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    [webView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    [webView waitForNextPresentationUpdate];

    auto attachmentSortFunction = ^(_WKAttachment *a, _WKAttachment *b) {
        return [a.info.contentType compare:b.info.contentType];
    };

    auto insertedAttachments = [[webView insertedAttachments] sortedArrayUsingComparator:attachmentSortFunction];
    EXPECT_EQ(insertedAttachments.count, 2U);
    EXPECT_EQ([[webView removedAttachments] count], 0U);

    auto heicAttachment = [insertedAttachments objectAtIndex:0];
    auto heicAttachmentInfo = heicAttachment.info;
    EXPECT_WK_STREQ(heicAttachmentInfo.contentType, "image/heic");
    EXPECT_TRUE([heicAttachmentInfo.name containsString:@"heic"]);
    EXPECT_GE(heicAttachmentInfo.data.length, 24374U);
    EXPECT_TRUE(heicAttachmentInfo.shouldPreserveFidelity);

    auto pngAttachment = [insertedAttachments objectAtIndex:1];
    auto pngAttachmentInfo =  pngAttachment.info;
    EXPECT_WK_STREQ(pngAttachmentInfo.contentType, "image/png");
    EXPECT_TRUE([pngAttachmentInfo.name containsString:@"dog in a spacesuit"]);
    EXPECT_GE(pngAttachmentInfo.data.length, 42451U);
    EXPECT_FALSE(pngAttachmentInfo.shouldPreserveFidelity);

    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('source').attachmentIdentifier"] isEqualToString:heicAttachment.uniqueIdentifier]);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"] isEqualToString:pngAttachment.uniqueIdentifier]);
}

TEST(AdaptiveImageGlyph, InsertWKAttachmentsCopyFromWebViewPasteToWebView)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto copyWebView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [copyWebView _setEditable:YES];

    [copyWebView synchronouslyLoadHTMLString:@"<body></body>"];
    [copyWebView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [copyWebView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [copyWebView waitForNextPresentationUpdate];

    [copyWebView selectAll:nil];
    [copyWebView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    auto pasteWebView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [pasteWebView _setEditable:YES];

    [pasteWebView synchronouslyLoadHTMLString:@"<body></body>"];
    [pasteWebView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    [pasteWebView _synchronouslyExecuteEditCommand:@"Paste" argument:nil];
    [pasteWebView waitForNextPresentationUpdate];

    auto attachmentSortFunction = ^(_WKAttachment *a, _WKAttachment *b) {
        return [a.info.contentType compare:b.info.contentType];
    };

    auto insertedAttachments = [[pasteWebView insertedAttachments] sortedArrayUsingComparator:attachmentSortFunction];
    EXPECT_EQ(insertedAttachments.count, 2U);
    EXPECT_EQ([[pasteWebView removedAttachments] count], 0U);

    auto heicAttachment = [insertedAttachments objectAtIndex:0];
    auto heicAttachmentInfo = heicAttachment.info;
    EXPECT_WK_STREQ(heicAttachmentInfo.contentType, "image/heic");
    EXPECT_GE(heicAttachmentInfo.data.length, 24374U);
    EXPECT_TRUE(heicAttachmentInfo.shouldPreserveFidelity);

    auto pngAttachment = [insertedAttachments objectAtIndex:1];
    auto pngAttachmentInfo = pngAttachment.info;
    EXPECT_WK_STREQ(pngAttachmentInfo.contentType, "image/png");
    EXPECT_GE(pngAttachmentInfo.data.length, 1901U);
    EXPECT_FALSE(pngAttachmentInfo.shouldPreserveFidelity);

    EXPECT_TRUE([[pasteWebView stringByEvaluatingJavaScript:@"document.querySelector('source').attachmentIdentifier"] isEqualToString:heicAttachment.uniqueIdentifier]);
    EXPECT_TRUE([[pasteWebView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"] isEqualToString:pngAttachment.uniqueIdentifier]);
}

TEST(AdaptiveImageGlyph, InsertWKAttachmentsMovingParagraphs)
{
    auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration _setAttachmentElementEnabled:YES];

    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configuration.get()]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body dir='auto' role='textbox' aria-label='Message Body'><br><div id='AppleMailSignature' dir='ltr'><!-- signature open -->Sent from my iPhone <!-- signature close --></div></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    NSString *setSelectionJavaScript = @""
    "(() => {"
    "  const element = document.getElementById('AppleMailSignature');"
    "  const range = document.createRange();"
    "  range.setStart(element.childNodes[1], 20);"
    "  range.setEnd(element.childNodes[1], 20);"
    "  "
    "  var selection = window.getSelection();"
    "  selection.removeAllRanges();"
    "  selection.addRange(range);"
    "})();";
    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    auto attachmentSortFunction = ^(_WKAttachment *a, _WKAttachment *b) {
        return [a.info.contentType compare:b.info.contentType];
    };

    auto insertedAttachments = [[webView insertedAttachments] sortedArrayUsingComparator:attachmentSortFunction];
    EXPECT_EQ(insertedAttachments.count, 2U);
    EXPECT_EQ([[webView removedAttachments] count], 0U);

    auto heicAttachment = [insertedAttachments objectAtIndex:0];
    auto heicAttachmentInfo = heicAttachment.info;
    EXPECT_WK_STREQ(heicAttachmentInfo.contentType, "image/heic");
    EXPECT_TRUE([heicAttachmentInfo.name containsString:@"heic"]);
    EXPECT_GE(heicAttachmentInfo.data.length, 24374U);
    EXPECT_TRUE(heicAttachmentInfo.shouldPreserveFidelity);

    auto pngAttachment = [insertedAttachments objectAtIndex:1];
    auto pngAttachmentInfo =  pngAttachment.info;
    EXPECT_WK_STREQ(pngAttachmentInfo.contentType, "image/png");
    EXPECT_TRUE([pngAttachmentInfo.name containsString:@"png"]);
    EXPECT_GE(pngAttachmentInfo.data.length, 1901U);
    EXPECT_FALSE(pngAttachmentInfo.shouldPreserveFidelity);

    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('source').attachmentIdentifier"] isEqualToString:heicAttachment.uniqueIdentifier]);
    EXPECT_TRUE([[webView stringByEvaluatingJavaScript:@"document.querySelector('img').attachmentIdentifier"] isEqualToString:pngAttachment.uniqueIdentifier]);
}

TEST(AdaptiveImageGlyph, InsertMultiple)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 600, 800)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body dir='auto' role='textbox' aria-label='Message Body'><div><br></div><div><br></div><div><br></div><div id='start'><br></div><div><br></div><div><br></div><div><br></div><div><br></div><br id='lineBreakAtBeginningOfSignature'><div id='AppleMailSignature' dir='ltr'><!-- signature open -->Sent from my iPhone<!-- signature close --></div></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    NSString *setSelectionJavaScript = @""
    "(() => {"
    "  const element = document.getElementById('start');"
    "  const range = document.createRange();"
    "  range.setStart(start, 0);"
    "  range.setEnd(start, 0);"
    "  "
    "  var selection = window.getSelection();"
    "  selection.removeAllRanges();"
    "  selection.addRange(range);"
    "})();";
    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];

    [webView waitForNextPresentationUpdate];

    auto verifyAdaptiveImageGlyph = [&](int index) {
        NSString *adaptiveImageGlyphSelector = [NSString stringWithFormat:@"document.querySelectorAll('#start > picture')[%i]", index];

        // <picture> element has two children, a <source> and an <img>.
        NSString *childCountJavaScript = [NSString stringWithFormat:@"%@.children.length", adaptiveImageGlyphSelector];
        EXPECT_EQ([webView stringByEvaluatingJavaScript:childCountJavaScript].intValue, 2);

        NSString *firstChildTagNameJavaScript = [NSString stringWithFormat:@"%@.children[0].tagName", adaptiveImageGlyphSelector];
        EXPECT_TRUE([[webView stringByEvaluatingJavaScript:firstChildTagNameJavaScript] isEqualToString:@"SOURCE"]);
        NSString *secondChildTagNameJavaScript = [NSString stringWithFormat:@"%@.children[1].tagName", adaptiveImageGlyphSelector];
        EXPECT_TRUE([[webView stringByEvaluatingJavaScript:secondChildTagNameJavaScript] isEqualToString:@"IMG"]);

        // <source> has an srcset and a type.
        NSString *sourceSrcsetJavaScript = [NSString stringWithFormat:@"%@.children[0].srcset", adaptiveImageGlyphSelector];
        EXPECT_TRUE([[webView stringByEvaluatingJavaScript:sourceSrcsetJavaScript] hasPrefix:@"blob:"]);
        NSString *sourceTypeJavaScript = [NSString stringWithFormat:@"%@.children[0].type", adaptiveImageGlyphSelector];
        EXPECT_TRUE([[webView stringByEvaluatingJavaScript:sourceTypeJavaScript] isEqualToString:@"image/x-apple-adaptive-glyph"]);

        // <img> has an src.
        NSString *imgSrcJavaScript = [NSString stringWithFormat:@"%@.children[1].src", adaptiveImageGlyphSelector];
        EXPECT_TRUE([[webView stringByEvaluatingJavaScript:imgSrcJavaScript] hasPrefix:@"blob:"]);

        // <img> has alt text.
        NSString *imgAltJavaScript = [NSString stringWithFormat:@"%@.children[1].alt", adaptiveImageGlyphSelector];
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:imgAltJavaScript], "dog in a spacesuit");

        // <img> uses <source>.
        NSString *imgCurrentSrcJavaScript = [NSString stringWithFormat:@"%@.children[1].currentSrc", adaptiveImageGlyphSelector];
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:imgCurrentSrcJavaScript], [webView stringByEvaluatingJavaScript:sourceSrcsetJavaScript]);

        // <img> has completed loading.
        NSString *imgCompleteJavaScript = [NSString stringWithFormat:@"%@.children[1].complete", adaptiveImageGlyphSelector];
        EXPECT_TRUE([webView stringByEvaluatingJavaScript:imgCompleteJavaScript].boolValue);

        // <img> has non-zero dimensions.
        NSString *imgWidthJavaScript = [NSString stringWithFormat:@"getComputedStyle(%@.children[1]).width", adaptiveImageGlyphSelector];
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:imgWidthJavaScript], "21px");
        NSString *imgHeightJavaScript = [NSString stringWithFormat:@"getComputedStyle(%@.children[1]).height", adaptiveImageGlyphSelector];
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:imgHeightJavaScript], "26.25px");
    };

    verifyAdaptiveImageGlyph(0);
    verifyAdaptiveImageGlyph(1);
}

TEST(AdaptiveImageGlyph, InsertTextAfterAdaptiveImageGlyph)
{
    RetainPtr webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];

    [webView stringByEvaluatingJavaScript:@"document.querySelector('picture').style = 'font-size: 64px;'"];

    NSString *text = @"a";
#if PLATFORM(IOS_FAMILY)
    [[webView textInputContentView] insertText:text];
#else
    [webView insertText:text];
#endif

    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('span').textContent"], "a");
    EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('span')).fontSize"], "64px");
}

TEST(AdaptiveImageGlyph, CopyRTF)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    [webView selectAll:nil];
    [webView _synchronouslyExecuteEditCommand:@"Copy" argument:nil];

    NSData *rtfData = readRTFDataFromPasteboard();
    EXPECT_NOT_NULL(rtfData);

    RetainPtr attributedString = adoptNS([[NSAttributedString alloc] initWithData:rtfData options:@{ } documentAttributes:nil error:nullptr]);
    EXPECT_NOT_NULL(attributedString.get());

    __block BOOL foundAdaptiveImageGlyph = NO;
    [attributedString enumerateAttribute:NSAdaptiveImageGlyphAttributeName inRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL* stop) {
        if ([value isKindOfClass:NSAdaptiveImageGlyph.class]) {
            foundAdaptiveImageGlyph = YES;
            EXPECT_TRUE([[value contentIdentifier] isEqualToString:[adaptiveImageGlyph contentIdentifier]]);
            EXPECT_TRUE([[value contentDescription] isEqualToString:[adaptiveImageGlyph contentDescription]]);
            EXPECT_TRUE([[value imageContent] isEqualToData:adaptiveImageGlyphData.get()]);
        }
    }];

    EXPECT_TRUE(foundAdaptiveImageGlyph);
}

TEST(AdaptiveImageGlyph, ContentsAsAttributedString)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500)]);
    [webView _setEditable:YES];

    [webView synchronouslyLoadHTMLString:@"<body></body>"];
    [webView focusElementAndEnsureEditorStateUpdate:@"document.body"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    // Clear the content identifier -> adaptive image glyph cache in order to actually
    // test the reconstruction of adaptive image glyphs across the process boundary.
    [CTAdaptiveImageGlyph flushInstanceCache];

    RetainPtr attributedString = [webView contentsAsAttributedString];
    EXPECT_NOT_NULL(attributedString.get());

    __block BOOL foundAdaptiveImageGlyph = NO;
    [attributedString enumerateAttribute:NSAdaptiveImageGlyphAttributeName inRange:NSMakeRange(0, [attributedString length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL* stop) {
        if ([value isKindOfClass:NSAdaptiveImageGlyph.class]) {
            foundAdaptiveImageGlyph = YES;
            EXPECT_TRUE([[value contentIdentifier] isEqualToString:[adaptiveImageGlyph contentIdentifier]]);
            EXPECT_TRUE([[value contentDescription] isEqualToString:[adaptiveImageGlyph contentDescription]]);

#if HAVE(NS_EMOJI_IMAGE_STRIKE_PROVENANCE)
            RetainPtr<NSDictionary> provenance = [adaptiveImageGlyph strikes].firstObject.provenance;

            // FIXME: Remove the conditional once rdar://137757841 is in a build.
            if ([provenance count]) {
                EXPECT_WK_STREQ([provenance objectForKey:(__bridge NSString *)kCGImagePropertyIPTCCredit], "Apple Image Playground");
                EXPECT_WK_STREQ([provenance objectForKey:(__bridge NSString *)kCGImagePropertyIPTCExtDigitalSourceType], "http://cv.iptc.org/newscodes/digitalsourcetype/trainedAlgorithmicMedia");
                EXPECT_TRUE([provenance isEqualToDictionary:[value strikes].firstObject.provenance]);
            }
#endif

            // Reconstruction is lossy, so data cannot be compared directly against the original.
            EXPECT_NOT_NULL([value imageContent]);
        }
    }];

    EXPECT_TRUE(foundAdaptiveImageGlyph);
}

#if ENABLE(DRAG_SUPPORT) && !PLATFORM(MACCATALYST)

#if PLATFORM(IOS_FAMILY)

TEST(AdaptiveImageGlyph, DragAdaptiveImageGlyphFromContentEditable)
{
    auto webView = adoptNS([[AdaptiveImageGlyphWKWebView alloc] initWithFrame:CGRectMake(0, 0, 320, 500) configuration:configurationForAdaptiveImageGlyphWebView().get()]);

    [webView synchronouslyLoadHTMLString:@"<!DOCTYPE html>"
        "<head>"
            "<meta name='viewport' content='width=device-width'>"
            "<style>"
                "body { width: 100%; height: 100%; margin: 0; }"
                "#source, #target { width: 100%; height: 200px; font-size: 200px; white-space: nowrap; box-sizing: border-box; }"
                "#target { border: green 1px dashed; color: green; border-width: 5px; }"
            "</style>"
        "</head>"
        "<body>"
            "<div contenteditable id='source'></div>"
            "<code><div id='target'></div></code>"
            "<script>"
            "target.addEventListener('dragenter', event => event.preventDefault());"
            "target.addEventListener('dragover', event => event.preventDefault());"
            "target.addEventListener('drop', event => {"
                "target.textContent = 'FAIL';"
                "event.preventDefault();"
            "});"
            "</script>"
        "</body>"];

    [webView focusElementAndEnsureEditorStateUpdate:@"source"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];

    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    [webView insertAdaptiveImageGlyph:adaptiveImageGlyph.get()];
    [webView waitForNextPresentationUpdate];

    auto simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebView:webView.get()]);
    [simulator runFrom:CGPointMake(100, 100) to:CGPointMake(100, 300)];

    EXPECT_WK_STREQ("", [webView stringByEvaluatingJavaScript:@"target.textContent"]);
}

#endif // PLATFORM(IOS_FAMILY)

TEST(AdaptiveImageGlyph, DropAdaptiveImageGlyphAsText)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body style='width: 100%; height: 100%;' contenteditable>"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];
    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    RetainPtr font = [WebCore::CocoaFont systemFontOfSize:36];

    RetainPtr attributedString = [NSAttributedString attributedStringWithAdaptiveImageGlyph:adaptiveImageGlyph.get() attributes:@{ NSFontAttributeName : font.get() }];
    RetainPtr RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:@{ }];

#if PLATFORM(MAC)
    RetainPtr pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[ NSRTFDPboardType ] owner:nil];
    [pasteboard setData:RTFDData.get() forType:NSRTFDPboardType];
    [simulator setExternalDragPasteboard:pasteboard.get()];
#else
    RetainPtr item = adoptNS([[NSItemProvider alloc] init]);
    [item registerDataRepresentationForTypeIdentifier:UTTypeFlatRTFD.identifier visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler(RTFDData.get(), nil);
        return nil;
    }];
    [simulator setExternalItemProviders:@[ item.get() ]];
#endif

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    EXPECT_WK_STREQ("36px", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture')).fontSize"]);
}

TEST(AdaptiveImageGlyph, DropAdaptiveImageGlyphAsSticker)
{
    RetainPtr simulator = adoptNS([[DragAndDropSimulator alloc] initWithWebViewFrame:CGRectMake(0, 0, 320, 500)]);
    RetainPtr webView = [simulator webView];
    [webView synchronouslyLoadHTMLString:@"<meta name='viewport' content='width=device-width'><body style='width: 100%; height: 100%;' contenteditable>"];

    RetainPtr adaptiveImageGlyphData = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];
    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:adaptiveImageGlyphData.get()]);

    RetainPtr font = [WebCore::CocoaFont systemFontOfSize:36];

    RetainPtr attributedString = [NSAttributedString attributedStringWithAdaptiveImageGlyph:adaptiveImageGlyph.get() attributes:@{ NSFontAttributeName : font.get() }];
    RetainPtr RTFDData = [attributedString RTFDFromRange:NSMakeRange(0, [attributedString length]) documentAttributes:@{ }];

    NSString *stickerType = @"com.apple.sticker";
#if PLATFORM(MAC)
    RetainPtr pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[ NSRTFDPboardType, stickerType ] owner:nil];
    [pasteboard setData:RTFDData.get() forType:NSRTFDPboardType];
    [simulator setExternalDragPasteboard:pasteboard.get()];
#else
    RetainPtr item = adoptNS([[NSItemProvider alloc] init]);
    [item registerDataRepresentationForTypeIdentifier:UTTypeFlatRTFD.identifier visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler(RTFDData.get(), nil);
        return nil;
    }];
    [item registerDataRepresentationForTypeIdentifier:stickerType visibility:NSItemProviderRepresentationVisibilityAll loadHandler:[&] (void (^completionHandler)(NSData *, NSError *)) -> NSProgress * {
        completionHandler(nil, nil);
        return nil;
    }];
    [simulator setExternalItemProviders:@[ item.get() ]];
#endif

    [simulator runFrom:CGPointMake(0, 0) to:CGPointMake(50, 50)];
    EXPECT_WK_STREQ("16px", [webView stringByEvaluatingJavaScript:@"getComputedStyle(document.querySelector('picture')).fontSize"]);
}

#endif // ENABLE(DRAG_SUPPORT) && !PLATFORM(MACCATALYST)

} // namespace TestWebKitAPI

#endif // ENABLE(MULTI_REPRESENTATION_HEIC)
