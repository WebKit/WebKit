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
#import "Helpers/cocoa/TestResourceLoadDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "Helpers/cocoa/WKWebViewConfigurationExtras.h"
#import "InstanceMethodSwizzler.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKUIDelegatePrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <WebKit/_WKAppHighlight.h>
#import <WebKit/_WKAppHighlightDelegate.h>
#import <WebKit/_WKAttachment.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKResourceLoadInfo.h>
#import <WebKit/_WKWebsiteDataStoreConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import "TestInputDelegate.h"
#import "UIKitSPIForTesting.h"
#import <WebKit/_WKTextInputContext.h>
#endif

#if ENABLE(MULTI_REPRESENTATION_HEIC)
#import <UIFoundation/NSAdaptiveImageGlyph.h>
#endif

#if PLATFORM(MAC)
#import "Helpers/mac/AppKitSPI.h"
#import "Helpers/mac/LocalEventMonitorSwizzler.h"
#import "Helpers/mac/WKWebViewForTestingImmediateActions.h"
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebKit/_WKHitTestResult.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>

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

@interface SiteIsolationMouseMoveOverElementDelegate : NSObject <WKUIDelegatePrivate>
@property (nonatomic, copy) void (^mouseDidMoveOverElement)(_WKHitTestResult *, NSEventModifierFlags);
@end

@implementation SiteIsolationMouseMoveOverElementDelegate
- (void)_webView:(WKWebView *)webView mouseDidMoveOverElement:(_WKHitTestResult *)hitTestResult withFlags:(NSEventModifierFlags)flags userInfo:(id<NSSecureCoding>)userInfo
{
    if (_mouseDidMoveOverElement)
        _mouseDidMoveOverElement(hitTestResult, flags);
}
@end
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
@interface UIView (SiteIsolationDictationStreamingOpacity)
- (void)_setDictationStreamingOpacity:(CGFloat)opacity forHypothesisText:(NSString *)hypothesisText streamingRange:(NSRange)streamingRange;
- (void)_clearDictationStreamingOpacity;
@end

// UIWKGestureType stands in for WKBEGestureType, which is BEGestureType with BrowserEngineKit and
// UIWKGestureType without; both are NSInteger-backed, and these handlers ignore the value.
@interface UIView (SiteIsolationPointBasedSelection)
- (void)changeSelectionWithTouchesFrom:(CGPoint)from to:(CGPoint)to withGesture:(UIWKGestureType)gestureType withState:(UIGestureRecognizerState)gestureState;
- (void)selectPositionAtBoundary:(UITextGranularity)granularity inDirection:(UITextDirection)direction fromPoint:(CGPoint)point completionHandler:(void (^)(void))completionHandler;
@end
#endif

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

#if ENABLE(APP_HIGHLIGHTS)
@interface SiteIsolationAppHighlightDelegate : NSObject <_WKAppHighlightDelegate>
- (NSArray<_WKAppHighlight *> *)storedHighlights;
@end

@implementation SiteIsolationAppHighlightDelegate {
    RetainPtr<NSMutableArray<_WKAppHighlight *>> _storedHighlights;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;
    _storedHighlights = adoptNS([[NSMutableArray alloc] init]);
    return self;
}

- (void)_webView:(WKWebView *)webView storeAppHighlight:(_WKAppHighlight *)highlight inNewGroup:(BOOL)inNewGroup requestOriginatedInApp:(BOOL)requestOriginatedInApp
{
    [_storedHighlights addObject:highlight];
}

- (NSArray<_WKAppHighlight *> *)storedHighlights
{
    return _storedHighlights.get();
}

@end
#endif // ENABLE(APP_HIGHLIGHTS)

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

TEST(SiteIsolation, SelectionGeometryInCrossOriginIframeUsesMainFrameCoordinates)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0; height: 2000px'><iframe id='iframe' style='position: absolute; left: 100px; top: 150px; width: 400px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body contenteditable style='margin: 0'>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);

    // The iframe is at (100, 150) in the main frame, so rects left relative to the iframe would be near the origin.
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);
    NSRect caretRect = NSZeroRect;
    EXPECT_TRUE(Util::waitFor([&] {
        caretRect = [webView _caretRectForTesting];
        return !NSIsEmptyRect(caretRect);
    }));
    EXPECT_NEAR(NSMinX(caretRect), 100, 2);
    EXPECT_NEAR(NSMinY(caretRect), 150, 2);

    auto selectTextAndWaitForSelectionBounds = [&] {
        [webView objectByEvaluatingJavaScript:@"getSelection().removeAllRanges()" inFrame:childFrame.get()];
        EXPECT_TRUE(Util::waitFor([&] {
            return ![webView _selectionRectsForTesting].count;
        }));
        [webView objectByEvaluatingJavaScript:@"getSelection().selectAllChildren(document.body)" inFrame:childFrame.get()];
        NSRect bounds = NSZeroRect;
        EXPECT_TRUE(Util::waitFor([&] {
            bounds = NSZeroRect;
            RetainPtr<NSArray<NSValue *>> rects = [webView _selectionRectsForTesting];
            for (NSValue *rect in rects.get())
                bounds = NSUnionRect(bounds, rect.rectValue);
            return !NSIsEmptyRect(bounds);
        }));
        return bounds;
    };

    auto selectionBounds = selectTextAndWaitForSelectionBounds();
    EXPECT_NEAR(NSMinX(selectionBounds), 100, 2);
    EXPECT_NEAR(NSMinY(selectionBounds), 150, 2);

    [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 100)"];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue] == 100;
    }));
    [webView waitForNextPresentationUpdate];

    selectionBounds = selectTextAndWaitForSelectionBounds();
    EXPECT_NEAR(NSMinX(selectionBounds), 100, 2);
    EXPECT_NEAR(NSMinY(selectionBounds), 50, 2);
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

TEST(SiteIsolation, SetMediaTypeAfterCrossOriginIframeLoads)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto mainFrameMatchesPrint = [&] {
        return [[webView objectByEvaluatingJavaScript:@"matchMedia('print').matches"] boolValue];
    };
    auto childFrameMatchesPrint = [&] {
        return [[webView objectByEvaluatingJavaScript:@"matchMedia('print').matches" inFrame:childFrame.get()] boolValue];
    };
    EXPECT_FALSE(mainFrameMatchesPrint());
    EXPECT_FALSE(childFrameMatchesPrint());

    webView.get().mediaType = @"print";
    EXPECT_TRUE(Util::waitFor([&] {
        return mainFrameMatchesPrint();
    }));
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameMatchesPrint();
    }));

    webView.get().mediaType = nil;
    EXPECT_TRUE(Util::waitFor([&] {
        return !mainFrameMatchesPrint() && !childFrameMatchesPrint();
    }));
}

TEST(SiteIsolation, SetResourceLoadDelegateAfterCrossOriginIframeLoads)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } },
        { "/subresource"_s, { "subresource"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];

    // The subresource load below happens after the cross-origin iframe's process already exists, so the
    // delegate reaches it only if setResourceLoadClient() is sent to every content process, not just the
    // main frame's.
    RetainPtr resourceLoadDelegate = adoptNS([TestResourceLoadDelegate new]);
    __block bool sawSubresourceRequest = false;
    [resourceLoadDelegate setDidSendRequest:^(WKWebView *, _WKResourceLoadInfo *, NSURLRequest *request) {
        if ([request.URL.path isEqualToString:@"/subresource"])
            sawSubresourceRequest = true;
    }];
    webView.get()._resourceLoadDelegate = resourceLoadDelegate.get();

    [webView objectByEvaluatingJavaScript:@"fetch('/subresource'); true" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::runFor(&sawSubresourceRequest, 5_s));

    webView.get()._resourceLoadDelegate = nil;
}

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

TEST(SiteIsolation, PauseAllAnimationsAfterCrossOriginIframeLoads)
{
    RetainPtr<NSData> videoData = [NSData dataWithContentsOfFile:[NSBundle.test_resourcesBundle pathForResource:@"test-mse" ofType:@"mp4"] options:0 error:NULL];
    HTTPResponse videoResponse { videoData.get() };
    videoResponse.setHeaderField("Content-Type"_s, "video/mp4"_s);

    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body><img id='img' src='/test-mse.mp4'></body>"_s } },
        { "/test-mse.mp4"_s, videoResponse }
    }, HTTPServer::Protocol::HttpsProxy);

    // Use the internals-enabled plug-in to observe image animation state inside the cross-origin iframe, and
    // point the data store at the test HTTPS proxy, since _test_configurationWithTestPlugInClassName: doesn't set one up.
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    auto childFrameImageIsAnimating = [&] {
        return [[webView objectByEvaluatingJavaScript:@"window.internals.isImageAnimating(document.getElementById('img'))" inFrame:childFrame.get()] boolValue];
    };
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameImageIsAnimating();
    }));

    // Pausing only takes effect when the system allows animation controls, so turn that on in the iframe's process.
    [webView objectByEvaluatingJavaScript:@"window.internals.setImageAnimationEnabled(false); true" inFrame:childFrame.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return !childFrameImageIsAnimating();
    }));

    __block bool done = false;
    [webView _playAllAnimationsWithCompletionHandler:^{
        done = true;
    }];
    EXPECT_TRUE(Util::runFor(&done, 5_s));
    EXPECT_TRUE(Util::waitFor([&] {
        return childFrameImageIsAnimating();
    }));

    done = false;
    [webView _pauseAllAnimationsWithCompletionHandler:^{
        done = true;
    }];
    EXPECT_TRUE(Util::runFor(&done, 5_s));
    EXPECT_TRUE(Util::waitFor([&] {
        return !childFrameImageIsAnimating();
    }));
}


#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)

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

// Dictation acts on the focused frame's selection, so it must be sent to the process containing the focused frame.
// A text placeholder must be removed by the process whose document contains it, even if focus has moved since.

TEST(SiteIsolation, TextPlaceholderInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><input id='input'><iframe id='iframe' style='width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 8)", _WKSelectionAttributeIsCaret);

#if PLATFORM(MAC)
    using TextPlaceholder = NSTextPlaceholder;
    id<NSTextInputClient_Async> textInput = (id<NSTextInputClient_Async>)webView.get();
#else
    using TextPlaceholder = UITextPlaceholder;
    auto textInput = [webView textInputContentView];
#endif

    __block RetainPtr<TextPlaceholder> placeholder;
    __block bool done = false;
    [textInput insertTextPlaceholderWithSize:CGSizeMake(50, 100) completionHandler:^(TextPlaceholder *insertedPlaceholder) {
        placeholder = insertedPlaceholder;
        done = true;
    }];
    Util::run(&done);
    ASSERT_NOT_NULL(placeholder.get());
    EXPECT_TRUE([[webView objectByEvaluatingJavaScript:@"!!document.querySelector('div')" inFrame:childFrame.get()] boolValue]);

    [webView objectByEvaluatingJavaScript:@"input.focus()"];
    EXPECT_TRUE(Util::waitFor([&] {
        return ![[webView firstChildFrame] _isFocused];
    }));

    done = false;
    [textInput removeTextPlaceholder:placeholder.get() willInsertText:NO completionHandler:^{
        done = true;
    }];
    Util::run(&done);
    EXPECT_FALSE([[webView objectByEvaluatingJavaScript:@"!!document.querySelector('div')" inFrame:childFrame.get()] boolValue]);
}

static RetainPtr<WKWebViewConfiguration> configurationWithInternals(const HTTPServer& server)
{
    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWebsiteDataStore:[server.httpsProxyConfiguration() websiteDataStore]];
    return configuration;
}

#if PLATFORM(IOS_FAMILY)

static NSUInteger markerCountInFrame(TestWKWebView *webView, WKFrameInfo *frame, NSString *markerType)
{
    RetainPtr script = [NSString stringWithFormat:@"internals.markerCountForNode(document.body.firstChild, '%@')", markerType];
    return [[webView objectByEvaluatingJavaScript:script.get() inFrame:frame] unsignedIntegerValue];
}

TEST(SiteIsolation, DictationAlternativesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>hello world&nbsp;</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 11)", _WKSelectionAttributeIsCaret);

    RetainPtr alternatives = adoptNS([[NSTextAlternatives alloc] initWithPrimaryString:@"hello world" alternativeStrings:@[ @"👋🌎" ]]);
    [[webView textInputContentView] addTextAlternatives:alternatives.get()];
    EXPECT_TRUE(Util::waitFor([&] {
        return markerCountInFrame(webView.get(), childFrame.get(), @"dictationalternatives") == 1;
    }));

    [[webView textInputContentView] removeEmojiAlternatives];
    EXPECT_TRUE(Util::waitFor([&] {
        return !markerCountInFrame(webView.get(), childFrame.get(), @"dictationalternatives");
    }));
}

TEST(SiteIsolation, DictationStreamingOpacityInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>hello world</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 11)", _WKSelectionAttributeIsCaret);

    [[webView textInputContentView] _setDictationStreamingOpacity:0.5 forHypothesisText:@"hello world" streamingRange:NSMakeRange(6, 5)];
    EXPECT_TRUE(Util::waitFor([&] {
        return markerCountInFrame(webView.get(), childFrame.get(), @"dictationstreamingopacity") == 1;
    }));

    [[webView textInputContentView] _clearDictationStreamingOpacity];
    EXPECT_TRUE(Util::waitFor([&] {
        return !markerCountInFrame(webView.get(), childFrame.get(), @"dictationstreamingopacity");
    }));
}

TEST(SiteIsolation, InsertFinalDictationResultInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body, 0)", _WKSelectionAttributeIsCaret);

    // Typing right after a dictated word removes its alternatives, unless it's part of inserting the final dictation result.
    [[webView textInputContentView] willInsertFinalDictationResult];
    [webView insertText:@"wanna" alternatives:@[ @"want to" ]];
    [webView insertText:@"." alternatives:@[ ]];
    [[webView textInputContentView] didInsertFinalDictationResult];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"document.body.textContent" inFrame:childFrame.get()] isEqualToString:@"wanna."];
    }));
    EXPECT_EQ(1U, markerCountInFrame(webView.get(), childFrame.get(), @"dictationalternatives"));
}

#endif // PLATFORM(IOS_FAMILY)

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

#if PLATFORM(MAC)

TEST(SiteIsolation, DeviceScaleFactorChangeUpdatesCrossOriginIframeCompositingScale)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body><div style='will-change: transform; width: 100px; height: 100px; background: green'></div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    RetainPtr storeConfiguration = adoptNS([[_WKWebsiteDataStoreConfiguration alloc] initNonPersistentConfiguration]);
    [storeConfiguration setHTTPSProxy:[NSURL URLWithString:[NSString stringWithFormat:@"https://127.0.0.1:%d/", server.port()]]];
    [configuration setWebsiteDataStore:adoptNS([[WKWebsiteDataStore alloc] _initWithConfiguration:storeConfiguration.get()]).get()];

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    RetainPtr childFrame = [webView firstChildFrame];

    NSString *layerTreeScript = @"internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_VISIBLE_RECTS)";

    [webView _setOverrideDeviceScaleFactor:3];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:layerTreeScript inFrame:childFrame.get()] containsString:@"(contentsScale 3.00)"];
    }));

    [webView _setOverrideDeviceScaleFactor:1];
    [webView waitForNextPresentationUpdate];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:layerTreeScript inFrame:childFrame.get()] containsString:@"(contentsScale 1.00)"];
    }));
}

#endif // PLATFORM(MAC)

// Replies to a text checking request must go to the web process that made it. Only that process has the
// pending request; any other process drops the reply.

#if PLATFORM(MAC)

static unsigned synchronousTextCheckCount;
static RetainPtr<NSString> pendingExtendedCheckString;
static BlockPtr<void(NSInteger, NSArray<NSTextCheckingResult *> *)> pendingExtendedCheckCompletion;

static NSArray<NSTextCheckingResult *> *swizzledCheckStringCountingChecks(id, SEL, NSString *, NSRange, NSTextCheckingTypes, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    ++synchronousTextCheckCount;
    return @[ ];
}

static NSInteger swizzledRequestGrammarCheckingDeferringCompletion(id, SEL, NSString *stringToCheck, NSRange, NSString *, NSDictionary *, void (^completionHandler)(NSInteger, NSArray<NSTextCheckingResult *> *))
{
    pendingExtendedCheckString = stringToCheck;
    pendingExtendedCheckCompletion = makeBlockPtr(completionHandler);
    return 0;
}

// Types into the editable body of the frame (the main frame if nil), then replies to the extended proofreading
// request that follows with a grammar result the synchronous check didn't report. The web process that made the
// request responds by checking the paragraph again, so this returns whether another synchronous check arrives.
// Setup problems are reported as separate failures, so a bare false means the reply never reached the requester.
static bool extendedProofreadingReplyTriggersRecheck(TestWKWebView *webView, WKFrameInfo *frame)
{
    synchronousTextCheckCount = 0;
    pendingExtendedCheckString = nil;
    pendingExtendedCheckCompletion = nullptr;

    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body)" inFrame:frame];
    [(id<NSTextInputClient>)webView insertText:@"Let's go in then store\n" replacementRange:NSMakeRange(NSNotFound, 0)];
    if (!Util::waitFor([] { return !!pendingExtendedCheckCompletion; })) {
        ADD_FAILURE() << "No extended proofreading request was made";
        return false;
    }

    // Let every check caused by the insertion finish, so that any check after the reply is the re-check.
    [webView waitForNextPresentationUpdate];
    auto checkCountBeforeReply = synchronousTextCheckCount;

    NSRange phraseRange = [pendingExtendedCheckString rangeOfString:@"go in then"];
    if (phraseRange.location == NSNotFound) {
        ADD_FAILURE() << "The extended proofreading request didn't include the typed text: " << [pendingExtendedCheckString UTF8String];
        return false;
    }
    NSDictionary *detail = @{
        NSGrammarRange: [NSValue valueWithRange:NSMakeRange(0, phraseRange.length)],
        NSGrammarCorrections: @[ @"go in the" ],
    };
    auto completion = std::exchange(pendingExtendedCheckCompletion, nullptr);
    completion(0, @[ [NSTextCheckingResult grammarCheckingResultWithRange:phraseRange details:@[ detail ]] ]);

    return Util::waitFor([&] {
        return synchronousTextCheckCount > checkCountBeforeReply;
    });
}

TEST(SiteIsolation, ExtendedProofreadingReplyReachesCrossOriginIframe)
{
    HTTPServer server({
        { "/control"_s, { "<body contenteditable></body>"_s } },
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckStringCountingChecks)
    };
    InstanceMethodSwizzler requestGrammarCheckingSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(requestGrammarCheckingOfString:range:language:options:completionHandler:),
        reinterpret_cast<IMP>(swizzledRequestGrammarCheckingDeferringCompletion)
    };

    RetainPtr configuration = server.httpsProxyConfiguration();
    setFeatureEnabled(configuration.get(), @"ExtendedProofreadingEnabled", true);

    // Check the whole mechanism in a main frame first, so that a failure below can only be the routing of the reply.
    {
        auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(configuration, CGRectMake(0, 0, 800, 600));
        [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/control"]]];
        [navigationDelegate waitForDidFinishNavigation];
        [webView _setContinuousSpellCheckingEnabledForTesting:YES];
        [webView _setGrammarCheckingEnabledForTesting:YES];
        [webView objectByEvaluatingJavaScript:@"document.body.focus()"];
        EXPECT_TRUE(extendedProofreadingReplyTriggersRecheck(webView.get(), nil));
    }

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView _setGrammarCheckingEnabledForTesting:YES];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:childFrame.get()];
    EXPECT_TRUE(extendedProofreadingReplyTriggersRecheck(webView.get(), childFrame.get()));

    pendingExtendedCheckCompletion = nullptr;
    pendingExtendedCheckString = nil;
}

#endif // PLATFORM(MAC)

#if ENABLE(APP_HIGHLIGHTS)

// Creating an app highlight acts on the focused frame's selection, so it must go to the focused frame's process.
// The request must also complete even when there's nothing to highlight: under site isolation the UI process
// crashes on a failed message check, and a request that's never answered is eventually cancelled with an empty
// highlight, which fails the check.

TEST(SiteIsolation, AddAppHighlightInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr delegate = adoptNS([SiteIsolationAppHighlightDelegate new]);
    [webView _setAppHighlightDelegate:delegate.get()];
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    [webView _addAppHighlight];
    EXPECT_TRUE(Util::waitFor([&] {
        return [delegate storedHighlights].count == 1;
    }));
    EXPECT_WK_STREQ("subframe text", [delegate storedHighlights].firstObject.text);
}

TEST(SiteIsolation, AddAppHighlightWithoutSelectionDoesNotCrashWhenWebProcessesExit)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr delegate = adoptNS([SiteIsolationAppHighlightDelegate new]);
    [webView _setAppHighlightDelegate:delegate.get()];

    // Nothing is selected in either frame, so whichever process gets the request has nothing to highlight.
    [webView _addAppHighlight];
    [webView waitForNextPresentationUpdate];

    // Any request that was never answered is cancelled when its process exits. That must not crash the UI process.
    pid_t mainFramePID = [webView mainFrame].info._processIdentifier;
    pid_t childFramePID = [webView firstChildFrame]._processIdentifier;
    EXPECT_NE(mainFramePID, childFramePID);
    kill(childFramePID, SIGKILL);
    kill(mainFramePID, SIGKILL);
    while (!kill(childFramePID, 0) || !kill(mainFramePID, 0))
        Util::spinRunLoop();
    Util::runFor(0.5_s);

    EXPECT_EQ(0U, [delegate storedHighlights].count);
}

#endif // ENABLE(APP_HIGHLIGHTS)

#if PLATFORM(IOS_FAMILY)

// UIKit reads text and geometry around the insertion point to drive autocorrection, predictive text, and
// accessibility. With the caret in a cross-origin iframe, those requests must go to the iframe's process, and
// any rects in the reply must be in the main frame's coordinates rather than the iframe's.

static constexpr auto mainFrameWithPositionedCrossOriginIframe = "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<body style='margin: 0'><iframe id='iframe' style='position: absolute; left: 100px; top: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;
static constexpr auto editableIframeWithText = "<body contenteditable style='margin: 0; font-size: 20px;'>hello world</body>"_s;

// The iframe in mainFrameWithPositionedCrossOriginIframe covers (100, 100) to (500, 400) in the main frame.
static void expectRectInPositionedCrossOriginIframe(CGRect rect)
{
    EXPECT_FALSE(CGRectIsEmpty(rect));
    EXPECT_GE(CGRectGetMinX(rect), 100);
    EXPECT_GE(CGRectGetMinY(rect), 100);
    EXPECT_LE(CGRectGetMaxX(rect), 500);
    EXPECT_LE(CGRectGetMaxY(rect), 400);
}

// Focuses the iframe's editable body with a user gesture, so that it becomes the focused element and starts an
// input session, then puts the caret after "hello world". Focusing already leaves a caret, so the UI process may not
// have seen the caret move by the time this returns; callers that depend on UI-side editor state must wait for more.
static RetainPtr<TestInputDelegate> startInputSessionInCrossOriginIframe(TestWKWebView *webView, WKFrameInfo *frame)
{
    RetainPtr inputDelegate = adoptNS([TestInputDelegate new]);
    __block bool didStartInputSession = false;
    [inputDelegate setFocusStartsInputSessionPolicyHandler:^_WKFocusStartsInputSessionPolicy(WKWebView *, id<_WKFocusedElementInfo>) {
        didStartInputSession = true;
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [webView _setInputDelegate:inputDelegate.get()];
    [webView objectByEvaluatingJavaScriptWithUserGesture:@"document.body.focus()" inFrame:frame];
    Util::run(&didStartInputSession);
    setSelectionInFrame(webView, frame, @"getSelection().setPosition(document.body.firstChild, 11)", _WKSelectionAttributeIsCaret);
    return inputDelegate;
}

TEST(SiteIsolation, AutocorrectionContextInCrossOriginIframe)
{
    // With the out-of-process keyboard, UIKit doesn't ask the web process for autocorrection context.
    if ([UIKeyboard usesInputSystemUI])
        return;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr inputDelegate = startInputSessionInCrossOriginIframe(webView.get(), childFrame.get());

    // The UI process caches the context the iframe sent when its body was focused, and only asks a web process
    // again after it sees the selection change. Switching from a caret to a range gives us something to wait for.
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setBaseAndExtent(document.body.firstChild, 6, document.body.firstChild, 11)", _WKSelectionAttributeIsRange);

    // The request is answered by a separate message that the UI process waits for, so the wait has to be on the
    // process that got the request. Otherwise the wait times out and the context comes back empty.
    auto context = [webView autocorrectionContext];
    EXPECT_WK_STREQ("world", context.selectedText);
    EXPECT_TRUE(context.contextBeforeSelection.startsWith("hello"_s));
}

TEST(SiteIsolation, AutocorrectionRectsInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr inputDelegate = startInputSessionInCrossOriginIframe(webView.get(), childFrame.get());

    auto [firstRect, lastRect] = [webView autocorrectionRectsForString:@"world"];
    expectRectInPositionedCrossOriginIframe(firstRect);
    expectRectInPositionedCrossOriginIframe(lastRect);
}

TEST(SiteIsolation, AccessibilityRectsAtSelectionOffsetInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().selectAllChildren(document.body)", _WKSelectionAttributeIsRange);

    __block bool done = false;
    __block RetainPtr<NSArray<NSValue *>> rects;
    [webView _accessibilityRetrieveRectsAtSelectionOffset:0 withText:@"hello" completionHandler:^(NSArray<NSValue *> *result) {
        rects = result;
        done = true;
    }];
    Util::run(&done);

    ASSERT_GE([rects count], 1U);
    expectRectInPositionedCrossOriginIframe([rects firstObject].CGRectValue);
}

#if HAVE(UI_WK_DOCUMENT_CONTEXT)

TEST(SiteIsolation, DocumentEditingContextInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithPositionedCrossOriginIframe } },
        { "/iframe"_s, { editableIframeWithText } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    RetainPtr inputDelegate = startInputSessionInCrossOriginIframe(webView.get(), childFrame.get());

    RetainPtr request = adoptNS([[UIWKDocumentRequest alloc] init]);
    [request setFlags:UIWKDocumentRequestText | UIWKDocumentRequestRects];
    [request setSurroundingGranularity:UITextGranularityParagraph];
    [request setGranularityCount:1];
    RetainPtr context = [webView synchronouslyRequestDocumentContext:request.get()];

    EXPECT_TRUE([[context contextBefore] isKindOfClass:NSString.class]);
    EXPECT_WK_STREQ("hello world", (NSString *)[context contextBefore]);
    RetainPtr<NSArray<NSValue *>> characterRects = [context characterRectsForCharacterRange:NSMakeRange(0, 1)];
    ASSERT_GE([characterRects count], 1U);
    expectRectInPositionedCrossOriginIframe([characterRects firstObject].CGRectValue);
}

#endif // HAVE(UI_WK_DOCUMENT_CONTEXT)

#endif // PLATFORM(IOS_FAMILY)

// Point-based selection. The UI process hands these messages a point in web-view coordinates. The iOS
// selection gestures hit-test it and re-dispatch into the cross-origin iframe under it, so they work whether
// or not that iframe is focused; `CharacterIndexForPointAsync` resolves it in the focused frame's process.

// initial-scale=1 keeps CSS pixels equal to web-view coordinates, which the helper below relies on.
static constexpr auto pointSelectionMainFrame = "<meta name='viewport' content='initial-scale=1'><body style='margin: 0'>main frame text<iframe id='iframe' style='position: absolute; left: 100px; top: 100px; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;
static constexpr auto pointSelectionIframe = "<body contenteditable style='margin: 0; font: 20px monospace'>hello world</body>"_s;

// A point in web-view coordinates just inside the left edge of the character at `offset` in the iframe's
// first text node, so that the nearest character boundary is unambiguously `offset` itself. The iframe is
// positioned at (100, 100) and nothing is scrolled, so its client coordinates are offset by exactly that.
static CGPoint pointAtCharacterInIframe(TestWKWebView *webView, WKFrameInfo *childFrame, unsigned offset)
{
    RetainPtr script = [NSString stringWithFormat:@"(() => {"
        "let range = document.createRange();"
        "range.setStart(document.body.firstChild, %u);"
        "range.setEnd(document.body.firstChild, %u);"
        "let rect = range.getBoundingClientRect();"
        "return [rect.left + 2, rect.top + rect.height / 2];"
        "})()", offset, offset + 1];
    RetainPtr result = [webView objectByEvaluatingJavaScript:script.get() inFrame:childFrame];
    return CGPointMake(100 + [[result objectAtIndex:0] doubleValue], 100 + [[result objectAtIndex:1] doubleValue]);
}

#if PLATFORM(IOS_FAMILY)

static int selectionAnchorOffsetInFrame(TestWKWebView *webView, WKFrameInfo *frame)
{
    return [[webView objectByEvaluatingJavaScript:@"getSelection().anchorOffset" inFrame:frame] intValue];
}

TEST(SiteIsolation, SelectPositionAtBoundaryInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);
    ASSERT_EQ(0, selectionAnchorOffsetInFrame(webView.get(), childFrame.get()));

    // Starting from a point at "h", the next word boundary forward is the end of "hello".
    __block bool done = false;
    [[webView textInputContentView] selectPositionAtBoundary:UITextGranularityWord inDirection:UITextStorageDirectionForward fromPoint:pointAtCharacterInIframe(webView.get(), childFrame.get(), 0) completionHandler:^{
        done = true;
    }];
    Util::run(&done);

    EXPECT_TRUE(Util::waitFor([&] {
        return selectionAnchorOffsetInFrame(webView.get(), childFrame.get()) == 5;
    }));
    EXPECT_EQ(5, selectionAnchorOffsetInFrame(webView.get(), childFrame.get()));
}

TEST(SiteIsolation, SelectWithTwoTouchesInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 0)", _WKSelectionAttributeIsCaret);

    // Two touches at "h" and at "w" must select everything between them, in the iframe.
    CGPoint from = pointAtCharacterInIframe(webView.get(), childFrame.get(), 0);
    CGPoint to = pointAtCharacterInIframe(webView.get(), childFrame.get(), 6);
    [[webView textInputContentView] changeSelectionWithTouchesFrom:from to:to withGesture:UIWKGestureLoupe withState:UIGestureRecognizerStateEnded];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"hello "];
    }));
    EXPECT_WK_STREQ("hello ", [webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()]);
}

static NSArray<_WKTextInputContext *> *synchronouslyRequestTextInputContextsInRect(WKWebView *webView, CGRect rect)
{
    __block RetainPtr<NSArray<_WKTextInputContext *>> result;
    __block bool done = false;
    [webView _requestTextInputContextsInRect:rect completionHandler:^(NSArray<_WKTextInputContext *> *contexts) {
        result = contexts;
        done = true;
    }];
    Util::run(&done);
    return result.autorelease();
}

static UIResponder<UITextInput> *synchronouslyFocusTextInputContext(WKWebView *webView, _WKTextInputContext *context, CGPoint point)
{
    __block UIResponder<UITextInput> *result = nil;
    __block bool done = false;
    [webView _focusTextInputContext:context placeCaretAt:point completionHandler:^(UIResponder<UITextInput> *responder) {
        result = responder;
        done = true;
    }];
    Util::run(&done);
    return result;
}

TEST(SiteIsolation, RequestTextInputContextsInRectCoveringCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><body><input type='password'></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);

    EXPECT_EQ(1UL, [contexts count]);
}

static RetainPtr<NSArray<_WKTextInputContext *>> textInputContextsSortedByX(TestWKWebView *webView, CGRect rect)
{
    RetainPtr contexts = [webView synchronouslyRequestTextInputContextsInRect:rect];
    return [contexts sortedArrayUsingComparator:^NSComparisonResult(_WKTextInputContext *a, _WKTextInputContext *b) {
        if (CGRectGetMinX(a.boundingRect) == CGRectGetMinX(b.boundingRect))
            return NSOrderedSame;
        return CGRectGetMinX(a.boundingRect) < CGRectGetMinX(b.boundingRect) ? NSOrderedAscending : NSOrderedDescending;
    }];
}

TEST(SiteIsolation, RequestTextInputContextsInRectCoveringOffsetCrossOriginIframes)
{
    static constexpr auto mainFrameHTML = "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<style>body { margin: 0; } iframe { position: absolute; top: 200px; width: 300px; height: 150px; border: none; }</style>"
        "<iframe style='left: 0' src='https://a.com/iframe'></iframe>"
        "<iframe style='left: 400px' src='https://b.com/iframe'></iframe>"_s;
    static constexpr auto iframeHTML = "<style>body { margin: 0; } input { position: absolute; left: 20px; top: 30px; width: 100px; height: 40px; box-sizing: border-box; }</style>"
        "<input type='text'>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameHTML } },
        { "/iframe"_s, { iframeHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    RetainPtr contexts = textInputContextsSortedByX(webView.get(), [webView bounds]);
    ASSERT_EQ(2U, [contexts count]);
    EXPECT_EQ(CGRectMake(20, 230, 100, 40), [contexts objectAtIndex:0].boundingRect);
    EXPECT_EQ(CGRectMake(420, 230, 100, 40), [contexts objectAtIndex:1].boundingRect);

    contexts = textInputContextsSortedByX(webView.get(), CGRectMake(410, 220, 120, 60));
    ASSERT_EQ(1U, [contexts count]);
    EXPECT_EQ(CGRectMake(420, 230, 100, 40), [contexts objectAtIndex:0].boundingRect);
}

TEST(SiteIsolation, FocusTextInputContextInCrossOriginIframeMovesCaret)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><input id='iframeInput' value='hello world'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);
    ASSERT_EQ(1UL, contexts.count);

    RetainPtr<_WKTextInputContext> iframeField = contexts[0];
    EXPECT_NOT_NULL(synchronouslyFocusTextInputContext(webView.get(), iframeField.get(), [iframeField boundingRect].origin));

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName" inFrame:childFrame.get()]);
    EXPECT_WK_STREQ("iframeInput", [webView stringByEvaluatingJavaScript:@"document.activeElement.id" inFrame:childFrame.get()]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, FocusTextInputContextInOffsetCrossOriginIframeMovesCaret)
{
    HTTPServer server({
        { "/mainframe"_s, { "<iframe style='margin-left: 100px; margin-top: 50px;' src='https://webkit.org/iframe'></iframe>"_s } },
        { "/iframe"_s, { "<!DOCTYPE html><input id='iframeInput' value='hello world'>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];

    NSArray<_WKTextInputContext *> *contexts = synchronouslyRequestTextInputContextsInRect(webView.get(), [webView bounds]);
    ASSERT_EQ(1UL, contexts.count);

    RetainPtr<_WKTextInputContext> iframeField = contexts[0];
    EXPECT_NOT_NULL(synchronouslyFocusTextInputContext(webView.get(), iframeField.get(), [iframeField boundingRect].origin));

    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_WK_STREQ("INPUT", [webView stringByEvaluatingJavaScript:@"document.activeElement.tagName" inFrame:childFrame.get()]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"document.activeElement.selectionStart" inFrame:childFrame.get()] intValue]);
}

TEST(SiteIsolation, SelectPositionAtBoundaryInUnfocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrame = [webView firstChildFrame];

    __block bool done = false;
    [[webView textInputContentView] selectPositionAtBoundary:UITextGranularityWord inDirection:UITextStorageDirectionForward fromPoint:pointAtCharacterInIframe(webView.get(), childFrame.get(), 0) completionHandler:^{
        done = true;
    }];
    EXPECT_TRUE(Util::runFor(&done, 5_s));

    EXPECT_TRUE(Util::waitFor([&] {
        return selectionAnchorOffsetInFrame(webView.get(), childFrame.get()) == 5;
    }));
}

TEST(SiteIsolation, SelectWithTwoTouchesInUnfocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    RetainPtr childFrame = [webView firstChildFrame];

    CGPoint from = pointAtCharacterInIframe(webView.get(), childFrame.get(), 0);
    CGPoint to = pointAtCharacterInIframe(webView.get(), childFrame.get(), 6);
    [[webView textInputContentView] changeSelectionWithTouchesFrom:from to:to withGesture:UIWKGestureLoupe withState:UIGestureRecognizerStateEnded];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView stringByEvaluatingJavaScript:@"getSelection().toString()" inFrame:childFrame.get()] isEqualToString:@"hello "];
    }));
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

TEST(SiteIsolation, CharacterIndexForPointInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { pointSelectionMainFrame } },
        { "/iframe"_s, { pointSelectionIframe } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server);

    // characterIndexForPoint: takes a screen point, so undo what WebViewImpl will redo. WKWebView is
    // flipped on macOS, so its coordinates match the CSS pixels the helper reports.
    CGPoint pointInView = pointAtCharacterInIframe(webView.get(), childFrame.get(), 6);
    NSPoint pointInWindow = [webView convertPoint:NSPointFromCGPoint(pointInView) toView:nil];
    NSPoint point = [webView window] ? [[webView window] convertPointToScreen:pointInWindow] : pointInWindow;

    __block NSUInteger index = NSNotFound;
    __block bool done = false;
    [static_cast<id<NSTextInputClient_Async>>(webView.get()) characterIndexForPoint:point completionHandler:^(NSUInteger result) {
        index = result;
        done = true;
    }];
    Util::run(&done);

    // "w" is at offset 6 in the iframe's "hello world".
    EXPECT_EQ(6U, index);
}

#endif // PLATFORM(MAC)

#if HAVE(REDESIGNED_TEXT_CURSOR) && PLATFORM(MAC)

TEST(SiteIsolation, DictationCaretStateInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameTextWithCrossOriginIframe } },
        { "/iframe"_s, { "<body contenteditable>subframe text</body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    RetainPtr configuration = configurationWithInternals(server);
    auto [webView, navigationDelegate, childFrame] = webViewWithFocusedCrossOriginIframe(server, configuration.get());
    setSelectionInFrame(webView.get(), childFrame.get(), @"getSelection().setPosition(document.body.firstChild, 8)", _WKSelectionAttributeIsCaret);

    auto isCaretBlinkingSuspendedInIframe = [&] {
        return [[webView objectByEvaluatingJavaScript:@"internals.isCaretBlinkingSuspended()" inFrame:childFrame.get()] boolValue];
    };
    ASSERT_FALSE(isCaretBlinkingSuspendedInIframe());

    [[NSNotificationCenter defaultCenter] postNotificationName:@"_NSTextInputContextDictationDidPauseNotification" object:nil];
    EXPECT_TRUE(Util::waitFor(isCaretBlinkingSuspendedInIframe));

    // Changing the caret animator type replaces the animator, and a new animator's blinking isn't suspended.
    [[NSNotificationCenter defaultCenter] postNotificationName:@"_NSTextInputContextDictationDidStartNotification" object:nil];
    EXPECT_TRUE(Util::waitFor([&] {
        return !isCaretBlinkingSuspendedInIframe();
    }));
}

#endif // HAVE(REDESIGNED_TEXT_CURSOR) && PLATFORM(MAC)

#if PLATFORM(MAC)

// The immediate-action (force-click) hit test starts in the main frame's process, which hands it to the process of a
// cross-origin iframe under the point. That process must answer, whether or not anything in it is focused, and the
// main frame's process must not act on the hit test it handed off. Once it answers, Look Up must be offered.

static constexpr auto mainFrameWithCrossOriginIframeAtTopLeft = "<body style='margin: 0'><iframe id='iframe' style='position: absolute; left: 0; top: 0; width: 400px; height: 300px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s;

static std::pair<RetainPtr<WKWebViewForTestingImmediateActions>, RetainPtr<TestNavigationDelegate>> immediateActionWebViewWithCrossOriginIframe(const HTTPServer& server)
{
    RetainPtr configuration = server.httpsProxyConfiguration();
    enableSiteIsolation(configuration.get());
    RetainPtr webView = adoptNS([[WKWebViewForTestingImmediateActions alloc] initWithFrame:NSMakeRect(0, 0, 500, 500) configuration:configuration.get()]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate allowAnyTLSCertificate];
    [webView setNavigationDelegate:navigationDelegate.get()];

    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    return { WTF::move(webView), WTF::move(navigationDelegate) };
}

// Focuses the iframe, so that its process answers the immediate-action hit test even without the fix for
// ImmediateActionInUnfocusedCrossOriginIframe.
static RetainPtr<WKFrameInfo> focusCrossOriginIframe(TestWKWebView *webView)
{
    [webView evaluateJavaScript:@"document.getElementById('iframe').focus()" completionHandler:nil];
    RetainPtr childFrame = [webView firstChildFrame];
    while (![childFrame _isFocused]) {
        Util::spinRunLoop();
        childFrame = [webView firstChildFrame];
    }
    return childFrame;
}

TEST(SiteIsolation, ImmediateActionInUnfocusedCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    RetainPtr childFrame = [webView firstChildFrame];
    EXPECT_FALSE([childFrame _isFocused]);

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);
    EXPECT_TRUE([[hitTestResult frameInfo]._handle isEqual:[childFrame _handle]]);
}

TEST(SiteIsolation, ImmediateActionInCrossOriginIframeDoesNotDispatchForceWillBeginInParent)
{
    static constexpr auto countForceWillBegin = "<script>window.forceWillBeginCount = 0; addEventListener('webkitmouseforcewillbegin', () => window.forceWillBeginCount++, true);</script>"_s;
    HTTPServer server({
        { "/mainframe"_s, { makeString(countForceWillBegin, mainFrameWithCrossOriginIframeAtTopLeft) } },
        { "/iframe"_s, { makeString(countForceWillBegin, "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s) } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);

    RetainPtr childFrame = focusCrossOriginIframe(webView.get());

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);

    EXPECT_EQ(1, [[webView objectByEvaluatingJavaScript:@"window.forceWillBeginCount" inFrame:childFrame.get()] intValue]);
    EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"window.forceWillBeginCount"] intValue]);
}

TEST(SiteIsolation, ImmediateActionOffersLookUpInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    focusCrossOriginIframe(webView.get());

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);
    EXPECT_NOT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionLookupText);
}

// Geometry in an immediate-action hit test result must be in the main frame's coordinates, like it is without site
// isolation, so the UI process can anchor link previews and highlights to it.
TEST(SiteIsolation, ImmediateActionElementBoundingBoxInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { "<body style='margin: 0'><iframe id='iframe' style='position: absolute; left: 100px; top: 100px; width: 300px; height: 200px; border: none;' src='https://webkit.org/iframe'></iframe></body>"_s } },
        { "/iframe"_s, { "<body style='margin: 0'><div id='text' style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    RetainPtr childFrame = [webView firstChildFrame];

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(116, 116)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);

    // The hit is on the text node, so the box is the text's.
    RetainPtr textRect = [webView objectByEvaluatingJavaScript:@"(() => { const range = document.createRange(); range.selectNodeContents(document.getElementById('text').firstChild); const rect = range.getBoundingClientRect(); return [rect.x, rect.y, rect.width, rect.height]; })()" inFrame:childFrame.get()];
    CGRect boundingBox = [hitTestResult elementBoundingBox];
    EXPECT_NEAR(CGRectGetMinX(boundingBox), 100 + [[textRect objectAtIndex:0] doubleValue], 1);
    EXPECT_NEAR(CGRectGetMinY(boundingBox), 100 + [[textRect objectAtIndex:1] doubleValue], 1);
    EXPECT_NEAR(CGRectGetWidth(boundingBox), [[textRect objectAtIndex:2] doubleValue], 1);
    EXPECT_NEAR(CGRectGetHeight(boundingBox), [[textRect objectAtIndex:3] doubleValue], 1);
}

// The animation can begin before the hit test's reply arrives, in which case the UI process waits for it. When the
// hit is in a cross-origin iframe, the reply that matters comes from the iframe's process, not the main frame's.
TEST(SiteIsolation, ImmediateActionAnimationBeginsBeforeCrossOriginIframeAnswers)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);

    auto [hitTestResult, actionType] = [webView simulateImmediateActionBeginningAnimationImmediately:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);
    EXPECT_NOT_NULL([webView immediateActionGesture].animationController);
    EXPECT_EQ(actionType, _WKImmediateActionLookupText);
}

// Once a force click's animation has run, the mouse up that ends it only dispatches mouseup, without a click. That
// relies on the animation's progress reaching the process that did the hit test, which is the iframe's.
TEST(SiteIsolation, MouseUpAfterImmediateActionInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<script>window.events = []; for (const type of ['mousedown', 'mouseup', 'click']) addEventListener(type, () => events.push(type));</script><body style='margin: 0'><div style='font-size: 32px;'>Foobar</div></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = immediateActionWebViewWithCrossOriginIframe(server);
    RetainPtr childFrame = [webView firstChildFrame];
    auto eventsInIframe = [&] {
        return [webView stringByEvaluatingJavaScript:@"events.join()" inFrame:childFrame.get()];
    };

    NSPoint pointInWindow = [webView convertPoint:NSMakePoint(16, 16) toView:nil];
    [webView mouseDownAtPoint:pointInWindow simulatePressure:NO];
    EXPECT_TRUE(Util::waitFor([&] {
        return [eventsInIframe() isEqualToString:@"mousedown"];
    }));

    auto [hitTestResult, actionType] = [webView simulateImmediateAction:NSMakePoint(16, 16)];
    EXPECT_WK_STREQ("Foobar", [hitTestResult lookupText]);

    auto immediateActionGesture = [webView immediateActionGesture];
    [immediateActionGesture.delegate immediateActionRecognizerWillBeginAnimation:immediateActionGesture];
    [immediateActionGesture.delegate immediateActionRecognizerDidUpdateAnimation:immediateActionGesture];
    [immediateActionGesture.delegate immediateActionRecognizerDidCompleteAnimation:immediateActionGesture];

    [webView mouseUpAtPoint:pointInWindow];
    EXPECT_TRUE(Util::waitFor([&] {
        return ![eventsInIframe() isEqualToString:@"mousedown"];
    }));
    EXPECT_WK_STREQ("mousedown,mouseup", eventsInIframe());
}

#endif // PLATFORM(MAC)

#if ENABLE(ORIENTATION_EVENTS) && PLATFORM(IOS_FAMILY)

TEST(SiteIsolation, CrossSiteIFrameReceivesOrientationChangeEvent)
{
    auto mainFrameHTML = "<iframe src='https://webkit.org/subframe'></iframe>"_s;
    auto subFrameHTML = "<script>window.addEventListener('orientationchange', () => { window.gotOrientationChange = true; });</script>"_s;

    HTTPServer server({
        { "/mainframe"_s, { mainFrameHTML } },
        { "/subframe"_s, { subFrameHTML } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 800, 600));
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];

    RetainPtr childFrame = [webView firstChildFrame];
    [webView _setInterfaceOrientationOverride:UIInterfaceOrientationLandscapeRight];

    EXPECT_TRUE(Util::waitFor([&] {
        return [[webView objectByEvaluatingJavaScript:@"window.gotOrientationChange === true" inFrame:childFrame.get()] boolValue];
    }));
}

#endif // ENABLE(ORIENTATION_EVENTS) && PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC)

// Pressing or releasing a modifier key while the mouse is still re-runs the hover hit test. Over a cross-origin
// iframe, it must report what's under the mouse in the iframe, not the main frame's <iframe> element.
TEST(SiteIsolation, ModifierKeyChangeOverLinkInCrossOriginIframe)
{
    HTTPServer server({
        { "/mainframe"_s, { mainFrameWithCrossOriginIframeAtTopLeft } },
        { "/iframe"_s, { "<body style='margin: 0'><a href='https://webkit.org/destination' style='display: block; width: 400px; height: 300px;'>link label</a></body>"_s } }
    }, HTTPServer::Protocol::HttpsProxy);

    auto linkLocation = NSMakePoint(200, 150);
    LocalEventMonitorSwizzler localMonitorSwizzler;
    // The flags-changed monitor takes the mouse location from the window, which a test can't move.
    InstanceMethodSwizzler mouseLocationSwizzler {
        NSWindow.class,
        @selector(mouseLocationOutsideOfEventStream),
        imp_implementationWithBlock(^{
            return linkLocation;
        })
    };

    auto [webView, navigationDelegate] = siteIsolatedViewAndDelegate(server, CGRectMake(0, 0, 400, 300));
    struct {
        RetainPtr<_WKHitTestResult> hitTestResult;
        NSEventModifierFlags flags { 0 };
    } lastHover;
    auto* lastHoverPointer = &lastHover;
    RetainPtr uiDelegate = adoptNS([SiteIsolationMouseMoveOverElementDelegate new]);
    [uiDelegate setMouseDidMoveOverElement:^(_WKHitTestResult *hitTestResult, NSEventModifierFlags flags) {
        lastHoverPointer->hitTestResult = hitTestResult;
        lastHoverPointer->flags = flags;
    }];
    [webView setUIDelegate:uiDelegate.get()];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"https://example.com/mainframe"]]];
    [navigationDelegate waitForDidFinishNavigation];
    [webView waitForNextPresentationUpdate];
    [webView _createFlagsChangedEventMonitorForTesting];

    [webView mouseMoveToPoint:linkLocation withFlags:0];
    EXPECT_TRUE(Util::waitFor([&] {
        return [[lastHover.hitTestResult absoluteLinkURL].absoluteString isEqualToString:@"https://webkit.org/destination"];
    }));

    lastHover.hitTestResult = nil;
    localMonitorSwizzler.sendEventToMonitor([NSEvent mouseEventWithType:NSEventTypeMouseMoved location:linkLocation modifierFlags:NSEventModifierFlagCommand timestamp:0 windowNumber:[[webView hostWindow] windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0]);
    EXPECT_TRUE(Util::waitFor([&] {
        return lastHover.hitTestResult && (lastHover.flags & NSEventModifierFlagCommand);
    }));
    EXPECT_WK_STREQ("https://webkit.org/destination", [lastHover.hitTestResult absoluteLinkURL].absoluteString);
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
