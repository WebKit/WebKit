/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// Overflown from SiteIsolation.mm. This file is unified with SiteIsolation.mm
// and its neighbors, so it must not rely on their file-scope helpers or reuse their names.
// Helpers shared between these files live in Helpers/cocoa/SiteIsolationTestUtilities.h.

#import "config.h"

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Test.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/HTTPServer.h"
#import "Helpers/cocoa/SiteIsolationTestUtilities.h"
#import "Helpers/cocoa/TestNavigationDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "InstanceMethodSwizzler.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKAttachment.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS_FAMILY)
#import <WebKit/_WKTextInputContext.h>
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import <UIFoundation/NSAdaptiveImageGlyph.h>
#endif

#if PLATFORM(MAC)
#import "Helpers/mac/AppKitSPI.h"
#import <WebCore/LegacyNSPasteboardTypes.h>

// Stands in for the font panel's attribute converter, and always adds a single underline.
@interface SiteIsolationUnderlineAttributeConverter : NSObject
- (NSDictionary *)convertAttributes:(NSDictionary *)attributes;
@end

@implementation SiteIsolationUnderlineAttributeConverter
- (NSDictionary *)convertAttributes:(NSDictionary *)attributes
{
    RetainPtr convertedAttributes = adoptNS([attributes mutableCopy]);
    [convertedAttributes setObject:@(NSUnderlineStyleSingle) forKey:NSUnderlineStyleAttributeName];
    return convertedAttributes.autorelease();
}
@end
#endif // PLATFORM(MAC)

@interface SiteIsolationFontAttributesListener : NSObject <WKUIDelegatePrivate>
- (NSDictionary<NSString *, id> *)lastFontAttributes;
@end

@implementation SiteIsolationFontAttributesListener {
    RetainPtr<NSDictionary> _lastFontAttributes;
}

- (void)_webView:(WKWebView *)webView didChangeFontAttributes:(NSDictionary<NSString *, id> *)fontAttributes
{
    _lastFontAttributes = fontAttributes;
}

- (NSDictionary<NSString *, id> *)lastFontAttributes
{
    return _lastFontAttributes.get();
}

@end

#if ENABLE(ATTACHMENT_ELEMENT)
@interface SiteIsolationAttachmentObserver : NSObject <WKUIDelegatePrivate>
- (NSArray<_WKAttachment *> *)insertedAttachments;
@end

@implementation SiteIsolationAttachmentObserver {
    RetainPtr<NSMutableArray<_WKAttachment *>> _insertedAttachments;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _insertedAttachments = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView didInsertAttachment:(_WKAttachment *)attachment withSource:(NSString *)source
{
    [_insertedAttachments addObject:attachment];
}

- (NSArray<_WKAttachment *> *)insertedAttachments
{
    return _insertedAttachments.get();
}

@end
#endif // ENABLE(ATTACHMENT_ELEMENT)

namespace TestWebKitAPI {

// Editing, font, and spelling commands act on the focused frame's selection, so they must be sent to
// the process containing the focused frame. These tests put the selection in a cross-origin iframe and
// check that each command takes effect there (or that its reply describes the iframe). If the command is
// sent to the main frame's process instead, it finds the main frame's empty selection and does nothing.

TEST(SiteIsolation, ListCommandsInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><ul><li>One</li><li id='item'>Two</li></ul></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(item.firstChild, 1)", _WKSelectionAttributeIsCaret);

    auto listDepth = [&] {
        return [[webView objectByEvaluatingJavaScript:@"(() => { let depth = 0; for (let element = item.parentElement; element; element = element.parentElement) { if (element.matches('ol, ul')) ++depth; } return depth; })()" inFrame:childFrame.get()] intValue];
    };
    EXPECT_EQ(1, listDepth());

    [webView _increaseListLevel:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return listDepth() == 2;
    }));

    [webView _decreaseListLevel:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return listDepth() == 1;
    }));

    [webView _changeListType:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"item.closest('ol, ul').tagName" inFrame:childFrame.get()] isEqualToString:@"OL"];
    }));
}

#if PLATFORM(MAC)

TEST(SiteIsolation, ValidateMenuItemInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // The item starts out disabled and can only become enabled when the web process that validates the
    // command replies. The main frame has no selection, so its process would report Copy as disabled.
    RetainPtr menu = adoptNS([NSMenu new]);
    [menu setAutoenablesItems:NO];
    RetainPtr item = adoptNS([NSMenuItem new]);
    [item setTarget:webView.get()];
    [item setAction:@selector(copy:)];
    [item setEnabled:NO];
    [menu addItem:item.get()];

    [webView validateUserInterfaceItem:item.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [item isEnabled];
    }));
}

TEST(SiteIsolation, ChangeFontAttributesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    RetainPtr converter = adoptNS([SiteIsolationUnderlineAttributeConverter new]);
    [webView changeAttributes:converter.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"document.queryCommandState('underline')" inFrame:childFrame.get()] boolValue];
    }));
}

TEST(SiteIsolation, TypingAttributesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable style='font-size: 37px'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 3)", _WKSelectionAttributeIsCaret);

    __block bool done = false;
    __block RetainPtr<NSDictionary> typingAttributes;
    [static_cast<id<NSTextInputClient_Async_Staging_44648564>>(webView.get()) typingAttributesWithCompletionHandler:^(NSDictionary<NSString *, id> *attributes) {
        typingAttributes = attributes;
        done = true;
    }];
    Util::run(&done);

    NSFont *font = [typingAttributes objectForKey:NSFontAttributeName];
    EXPECT_NOT_NULL(font);
    EXPECT_EQ(37, font.pointSize);
}

TEST(SiteIsolation, AttributedSubstringInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);

    __block bool done = false;
    __block RetainPtr<NSString> substring;
    [static_cast<id<NSTextInputClient_Async>>(webView.get()) attributedSubstringForProposedRange:NSMakeRange(0, 8) completionHandler:^(NSAttributedString *string, NSRange) {
        substring = string.string;
        done = true;
    }];
    Util::run(&done);

    EXPECT_WK_STREQ("subframe", substring.get());
}

TEST(SiteIsolation, ChangeSpellingInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>teh</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // changeSpelling: reads the replacement from the sender's selected cell, like the spelling panel's guess list.
    [webView changeSpelling:[NSTextField labelWithString:@"the"]];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"the"];
    }));
}

static NSArray<NSTextCheckingResult *> *swizzledCheckStringReportingMisspelledWord(id, SEL, NSString *stringToCheck, NSRange, NSTextCheckingTypes types, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    if (!(types & NSTextCheckingTypeSpelling))
        return @[ ];

    NSRange misspelledRange = [stringToCheck rangeOfString:@"teh"];
    if (misspelledRange.location == NSNotFound)
        return @[ ];

    return @[ [NSTextCheckingResult spellCheckingResultWithRange:misspelledRange] ];
}

TEST(SiteIsolation, CheckSpellingInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>hello teh world</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringReportingMisspelledWord)
    };

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);

    // Check Spelling selects the next misspelled word after the selection.
    [webView checkSpelling:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"teh"];
    }));
}

TEST(SiteIsolation, CenterSelectionInVisibleAreaInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable style='margin: 0'><div style='height: 2000px'></div><span id='target'>target</span></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(target.firstChild, 0)", _WKSelectionAttributeIsCaret);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrame.get()] intValue]);

    // Only the iframe's own scroll position is checked; revealing the selection in ancestor frames that
    // live in other processes is a separate concern.
    [webView centerSelectionInVisibleArea:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY" inFrame:childFrame.get()] intValue] > 0;
    }));
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

TEST(SiteIsolation, BaseWritingDirectionInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><p id='paragraph'>Hello world</p></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(paragraph.firstChild, 0)", _WKSelectionAttributeIsCaret);

    [webView makeTextWritingDirectionRightToLeft:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getComputedStyle(paragraph).direction" inFrame:childFrame.get()] isEqualToString:@"rtl"];
    }));
}

TEST(SiteIsolation, ChangeFontSizeInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable><span id='target'>subframe</span></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setBaseAndExtent(target.firstChild, 0, target.firstChild, 8)", _WKSelectionAttributeIsRange);

    [webView _setFontSize:20 sender:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getComputedStyle(getSelection().getRangeAt(0).startContainer.parentElement).fontSize" inFrame:childFrame.get()] isEqualToString:@"20px"];
    }));
}

TEST(SiteIsolation, SpeakSelectionInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // If the request reaches the main frame's process, it finds no selection there and returns the main
    // frame's contents ("main frame text") instead.
    EXPECT_WK_STREQ("subframe text", [webView textForSpeakSelection]);
}

#endif // PLATFORM(IOS_FAMILY)

// Page-wide state set on the web view after load must reach every web content process, not just the
// main frame's. A cross-origin iframe's process that already exists never hears about the change, even
// though a process created later would get it from the page's creation parameters.

TEST(SiteIsolation, SetEditableAfterCrossOriginIframeLoads)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto mainFrameIsEditable = [&] {
        return [[webView objectByEvaluatingJavaScript:@"document.body.isContentEditable"] boolValue];
    };
    auto childFrameIsEditable = [&] {
        return [[webView objectByEvaluatingJavaScript:@"document.body.isContentEditable" inFrame:childFrame.get()] boolValue];
    };
    EXPECT_FALSE(mainFrameIsEditable());
    EXPECT_FALSE(childFrameIsEditable());

    [webView _setEditable:YES];
    EXPECT_TRUE(Util::waitFor([&] {
        return mainFrameIsEditable();
    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameIsEditable();
    }));

    [webView _setEditable:NO];
    EXPECT_TRUE(Util::waitFor([&] {
        return !mainFrameIsEditable() && !childFrameIsEditable();
    }));
}

TEST(SiteIsolation, FontAttributesDelegateSetAfterFocusingCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable style='font-size: 37px'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 3)", _WKSelectionAttributeIsCaret);

    // Let any editor state updates for the new selection arrive before the delegate is set, so that the
    // only thing that can report font attributes is the web process learning that they are now needed.
    [webView waitForNextPresentationUpdate];

    // Setting a delegate that wants font attributes tells the web processes to start computing them and
    // to send a fresh editor state. Only the focused iframe's process can report its font.
    RetainPtr listener = adoptNS([SiteIsolationFontAttributesListener new]);
    [webView setUIDelegate:listener.get()];
    EXPECT_TRUE(Util::waitFor([&] {
#if PLATFORM(MAC)
        NSFont *font = [listener lastFontAttributes][NSFontAttributeName];
#else
        UIFont *font = [listener lastFontAttributes][NSFontAttributeName];
#endif
        return font.pointSize == 37;
    }));
}

#if PLATFORM(IOS_FAMILY)

TEST(SiteIsolation, SelectionChangesInCrossOriginIframeAreIgnoredDuringTextInteraction)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><textarea style='display: block; width: 200px; height: 50px;'></textarea><iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 3)", _WKSelectionAttributeIsCaret);

    RetainPtr contexts = [webView synchronouslyRequestTextInputContextsInRect:[webView bounds]];
    ASSERT_GE([contexts count], 1U);
    RetainPtr context = [contexts firstObject];

    // While a text interaction is in progress, every web process must report its selection changes as
    // ignorable, so the UI process doesn't update its selection UI in the middle of the interaction.
    [webView _willBeginTextInteractionInTextInputContext:context.get()];
    [webView objectByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)" inFrame:childFrame.get()];
    [webView waitForNextPresentationUpdate];
    EXPECT_EQ(_WKSelectionAttributeIsCaret, [webView _selectionAttributes]);

    // Finishing the interaction makes the web processes report their current selection again.
    [webView _didFinishTextInteractionInTextInputContext:context.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return [webView _selectionAttributes] == _WKSelectionAttributeIsRange;
    }));
}

#endif // PLATFORM(IOS_FAMILY)

// Pasteboard, Services, and content insertion act on the focused frame's selection, so they must be sent to
// the process containing the focused frame, and any pasteboard access must be granted to that process.

#if PLATFORM(MAC)

TEST(SiteIsolation, WriteSelectionToPasteboardInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    // Services ask for the selection with a synchronous request per type. Plain text and web archive data
    // come from different messages, so check both. The main frame has no selection, so its process would
    // return nothing for either.
    RetainPtr stringPasteboard = [NSPasteboard pasteboardWithUniqueName];
    [webView writeSelectionToPasteboard:stringPasteboard.get() types:@[ WebCore::legacyStringPasteboardTypeSingleton() ]];
    EXPECT_WK_STREQ("subframe text", [stringPasteboard stringForType:WebCore::legacyStringPasteboardTypeSingleton()]);

    RetainPtr dataPasteboard = [NSPasteboard pasteboardWithUniqueName];
    [webView writeSelectionToPasteboard:dataPasteboard.get() types:@[ UTTypeWebArchive.identifier ]];
    EXPECT_GT([dataPasteboard dataForType:UTTypeWebArchive.identifier].length, 0U);

    [stringPasteboard releaseGlobally];
    [dataPasteboard releaseGlobally];
}

TEST(SiteIsolation, ReadSelectionFromPasteboardInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>original text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    RetainPtr pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard clearContents];
    [pasteboard setString:@"pasted text" forType:NSPasteboardTypeString];

    // This fails if the request goes to the main frame's process, which has no selection. It also fails if
    // the iframe's process isn't granted access to the pasteboard, in which case it reads nothing.
    EXPECT_TRUE([webView readSelectionFromPasteboard:pasteboard.get()]);
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"pasted text"];
    }));

    [pasteboard releaseGlobally];
}

#endif // PLATFORM(MAC)

#if ENABLE(MULTI_REPRESENTATION_HEIC)

TEST(SiteIsolation, InsertAdaptiveImageGlyphInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 8)", _WKSelectionAttributeIsCaret);

    RetainPtr data = [NSData dataWithContentsOfURL:[NSBundle.test_resourcesBundle URLForResource:@"adaptive-image-glyph" withExtension:@"heic"]];
    RetainPtr adaptiveImageGlyph = adoptNS([[NSAdaptiveImageGlyph alloc] initWithImageContent:data.get()]);
#if PLATFORM(MAC)
    [(id<NSTextInputClient>)webView.get() insertAdaptiveImageGlyph:adaptiveImageGlyph.get() replacementRange:NSMakeRange(0, 0)];
#else
    RetainPtr range = adoptNS([[UITextRange alloc] init]);
    [[webView textInputContentView] insertAdaptiveImageGlyph:adaptiveImageGlyph.get() replacementRange:range.get()];
#endif

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"!!document.querySelector('picture')" inFrame:childFrame.get()] boolValue];
    }));
}

#endif // ENABLE(MULTI_REPRESENTATION_HEIC)

#if ENABLE(ATTACHMENT_ELEMENT)

// Inserting an attachment acts on the focused frame's selection, so it must go to the focused frame's process.
// Updates and icons for an existing attachment must go to the process whose document contains it, and that
// process must be able to find the element even when it isn't in the main frame's document.

static RetainPtr<WKWebViewConfiguration> attachmentEnabledConfiguration(const HTTPServer& server)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
    [configuration _setAttachmentElementEnabled:YES];
    return configuration;
}

static RetainPtr<NSFileWrapper> textFileWrapper(NSString *filename)
{
    RetainPtr fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:[@"Hello world" dataUsingEncoding:NSUTF8StringEncoding]]);
    [fileWrapper setPreferredFilename:filename];
    return fileWrapper;
}

static NSString * const attachmentTitleScript = @"document.querySelector('attachment')?.getAttribute('title') ?? ''";

TEST(SiteIsolation, InsertAttachmentInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = attachmentEnabledConfiguration(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body, 0)", _WKSelectionAttributeIsCaret);

    // The completion handler runs whether or not anything was inserted, so check the iframe's document.
    __block bool done = false;
    [webView _insertAttachmentWithFileWrapper:textFileWrapper(@"hello.txt").get() contentType:@"text/plain" completion:^(BOOL) {
        done = true;
    }];
    Util::run(&done);

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:attachmentTitleScript inFrame:childFrame.get()] isEqualToString:@"hello.txt"];
    }));
}

TEST(SiteIsolation, SetFileWrapperForAttachmentInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body><attachment title='original.txt' type='text/plain'></attachment></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(attachmentEnabledConfiguration(server), CGRectMake(0, 0, 800, 600));
    RetainPtr observer = adoptNS([SiteIsolationAttachmentObserver new]);
    [webView setUIDelegate:observer.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    // The iframe's process reports the attachment when its element is connected.
    EXPECT_TRUE(Util::waitFor([&] {
        return [observer insertedAttachments].count == 1;
    }));
    RetainPtr<_WKAttachment> attachment = [observer insertedAttachments].firstObject;

    __block bool done = false;
    [attachment setFileWrapper:textFileWrapper(@"updated.txt").get() contentType:@"text/plain" completion:^(NSError *) {
        done = true;
    }];
    Util::run(&done);

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:attachmentTitleScript inFrame:childFrame.get()] isEqualToString:@"updated.txt"];
    }));
}

#if PLATFORM(MAC)

TEST(SiteIsolation, AttachmentIconInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><attachment title='main.txt' type='text/plain'></attachment><iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body><attachment title='subframe.txt' type='text/plain'></attachment></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    // Use the internals-enabled plug-in to reach the attachment's user agent shadow tree, and point the data
    // store at the test HTTPS proxy, since _test_configurationWithTestPlugInClassName: doesn't set one up.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];
    [configuration _setAttachmentElementEnabled:YES];

    // A wide-layout attachment shows its icon in an <img> in its shadow tree, so a delivered icon is observable.
    // This has to be set on the configuration; WKWebView overwrites the preference from it at initialization.
    [configuration _setAttachmentWideLayoutEnabled:YES];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSString *iconIsLoadedScript = @"(() => {"
        "    const icon = internals.shadowRoot(document.querySelector('attachment'))?.getElementById('attachment-icon');"
        "    return !!icon && icon.src.startsWith('blob:');"
        "})()";

    // The main frame's attachment checks that icons are delivered at all, so a failure below is specific to the iframe.
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:iconIsLoadedScript] boolValue];
    }));

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:iconIsLoadedScript inFrame:childFrame.get()] boolValue];
    }));
}

#endif // PLATFORM(MAC)

#endif // ENABLE(ATTACHMENT_ELEMENT)

} // namespace TestWebKitAPI
