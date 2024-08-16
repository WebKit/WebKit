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

#if ENABLE(WRITING_TOOLS)

#import "InstanceMethodSwizzler.h"
#import "PlatformUtilities.h"
#import "Test.h"
#import "TestInputDelegate.h"
#import "TestNavigationDelegate.h"
#import "TestUIDelegate.h"
#import "TestWKWebView.h"
#import "UIKitSPIForTesting.h"
#import "WKWebViewConfigurationExtras.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <WebCore/ColorCocoa.h>
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <WebKit/WKMenuItemIdentifiersPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/WebKit.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <pal/spi/cocoa/WritingToolsUISPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/unicode/CharacterNames.h>
#import <pal/cocoa/WritingToolsUISoftLink.h>

#if PLATFORM(MAC)

@interface NSMenu (Extras)
- (NSMenuItem *)itemWithIdentifier:(NSString *)identifier;
@end

@implementation NSMenu (Extras)

- (NSMenuItem *)itemWithIdentifier:(NSString *)identifier
{
    for (NSInteger index = 0; index < self.numberOfItems; ++index) {
        NSMenuItem *item = [self itemAtIndex:index];
        if ([item.identifier isEqualToString:identifier])
            return item;
    }
    return nil;
}

@end

#endif

@interface NSString (Extras)
- (NSString *)_withVisibleReplacementCharacters;
@end

@implementation NSString (Extras)

- (NSString *)_withVisibleReplacementCharacters
{
    return [self stringByReplacingOccurrencesOfString:@"\xEF\xBF\xBC" withString:@"<replacement char>"];
}

@end

@implementation _WKAttachment (AttachmentTesting)

- (BOOL)synchronouslySetFileWrapper:(NSFileWrapper *)fileWrapper newContentType:(NSString *)newContentType
{
    __block RetainPtr<NSError> resultError;
    __block bool done = false;

    [self setFileWrapper:fileWrapper contentType:newContentType completion:^(NSError *error) {
        resultError = error;
        done = true;
    }];

    TestWebKitAPI::Util::run(&done);

    return !!resultError;
}

@end

// FIXME: (rdar://130540028) Remove uses of the old WritingToolsAllowedInputOptions API in favor of the new WritingToolsResultOptions API, and remove staging.
#if PLATFORM(IOS_FAMILY)
@protocol UITextInput_Staging130540028 <UITextInput>

- (PlatformWritingToolsResultOptions)allowedWritingToolsResultOptions;

@end
#else
@protocol NSTextInputTraits_Staging130540028 <NSTextInputTraits>

- (PlatformWritingToolsResultOptions)allowedWritingToolsResultOptions;

@end
#endif

@interface WritingToolsWKWebView : TestWKWebView

@end

@implementation WritingToolsWKWebView {
#if PLATFORM(IOS_FAMILY)
    RetainPtr<TestInputDelegate> _inputDelegate;
#endif
}

- (instancetype)initWithHTMLString:(NSString *)string
{
    return [self initWithHTMLString:string writingToolsBehavior:PlatformWritingToolsBehaviorComplete];
}

- (instancetype)initWithHTMLString:(NSString *)string writingToolsBehavior:(PlatformWritingToolsBehavior)behavior
{
    return [self initWithHTMLString:string writingToolsBehavior:behavior attachmentElementEnabled:NO];
}

- (instancetype)initWithHTMLString:(NSString *)string writingToolsBehavior:(PlatformWritingToolsBehavior)behavior attachmentElementEnabled:(BOOL)attachmentElementEnabled
{
    auto configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"WebProcessPlugInWithInternals" configureJSCForTesting:YES];
    [configuration setWritingToolsBehavior:behavior];
    [configuration _setAttachmentElementEnabled:attachmentElementEnabled];

    if (!(self = [super initWithFrame:CGRectMake(0, 0, 320, 568) configuration:configuration]))
        return nil;

#if PLATFORM(IOS_FAMILY)
    _inputDelegate = adoptNS([[TestInputDelegate alloc] init]);
    [_inputDelegate setFocusStartsInputSessionPolicyHandler:[&] (WKWebView *, id<_WKFocusedElementInfo>) -> _WKFocusStartsInputSessionPolicy {
        return _WKFocusStartsInputSessionPolicyAllow;
    }];
    [self _setInputDelegate:_inputDelegate.get()];
#endif

    [self synchronouslyLoadHTMLString:string];

    return self;
}

- (void)focusDocumentBodyAndSelectAll
{
    NSString *script = @"document.body.focus()";
#if PLATFORM(IOS_FAMILY)
    [self evaluateJavaScriptAndWaitForInputSessionToChange:script];
#else
    [self stringByEvaluatingJavaScript:script];
#endif
    [self waitForNextPresentationUpdate];

    [self stringByEvaluatingJavaScript:@"document.execCommand('selectAll', false, null)"];
    [self waitForNextPresentationUpdate];
}

- (id<WTWritingToolsDelegate>)writingToolsDelegate
{
#if PLATFORM(IOS_FAMILY)
    return (id<WTWritingToolsDelegate>)[self textInputContentView];
#else
    return (id<WTWritingToolsDelegate>)self;
#endif
}

- (PlatformWritingToolsResultOptions)allowedWritingToolsResultOptionsForTesting
{
#if PLATFORM(IOS_FAMILY)
    return [(id<UITextInput_Staging130540028>)[self textInputContentView] allowedWritingToolsResultOptions];
#else
    return [(id<NSTextInputTraits_Staging130540028>)self allowedWritingToolsResultOptions];
#endif
}

- (PlatformWritingToolsBehavior)writingToolsBehaviorForTesting
{
#if PLATFORM(IOS_FAMILY)
    return [[self textInputContentView] writingToolsBehavior];
#else
    return [(id<NSTextInputTraits>)self writingToolsBehavior];
#endif
}

- (NSString *)contentsAsStringWithoutNBSP
{
    auto string = String { [self contentsAsString] };
    auto updatedString = makeStringByReplacingAll(string, noBreakSpace, space);
    return (NSString *)updatedString;
}

- (NSUInteger)transparentContentMarkerCount:(NSString *)evaluateNodeExpression
{
    NSString *scriptToEvaluate = [NSString stringWithFormat:@"internals.markerCountForNode((() => %@)(), 'transparentcontent')", evaluateNodeExpression];
    NSNumber *result = [self objectByEvaluatingJavaScript:scriptToEvaluate];
    return result.unsignedIntegerValue;
}

- (NSString *)ensureAttachmentForImageElement
{
    __block bool doneWaitingForAttachmentInsertion = false;
    [self performAfterReceivingMessage:@"inserted" action:^{
        doneWaitingForAttachmentInsertion = true;
    }];

    const char *scriptForEnsuringAttachmentIdentifier = \
        "const identifier = HTMLAttachmentElement.getAttachmentIdentifier(document.querySelector('img'));"
        "setTimeout(() => webkit.messageHandlers.testHandler.postMessage('inserted'), 0);"
        "identifier";

    RetainPtr attachmentIdentifier = [self stringByEvaluatingJavaScript:@(scriptForEnsuringAttachmentIdentifier)];
    TestWebKitAPI::Util::run(&doneWaitingForAttachmentInsertion);

    return attachmentIdentifier.autorelease();
}

- (void)attemptEditingForTesting
{
    [self stringByEvaluatingJavaScript:@"document.execCommand('selectAll', false, null)"];
    [self _synchronouslyExecuteEditCommand:@"DeleteBackward" argument:nil];
    [self _synchronouslyExecuteEditCommand:@"InsertText" argument:@"Test"];
}

- (void)waitForContentValue:(NSString *)expectedValue {
    do {
        if ([[self contentsAsStringWithoutNBSP] isEqual:expectedValue])
            break;

        TestWebKitAPI::Util::runFor(0.1_s);
    } while (true);
}

- (void)waitForSelectionValue:(NSString *)expectedValue {
    do {
        if ([[self stringByEvaluatingJavaScript:@"window.getSelection().toString()"] isEqual: expectedValue])
            break;

        TestWebKitAPI::Util::runFor(0.1_s);
    } while (true);
};

@end

struct ColorExpectation {
    CGFloat red { 0 };
    CGFloat green { 0 };
    CGFloat blue { 0 };
    CGFloat alpha { 0 };
};

static void checkColor(WebCore::CocoaColor *color, std::optional<ColorExpectation> expectation)
{
    if (!expectation) {
        EXPECT_NULL(color);
        return;
    }

    EXPECT_NOT_NULL(color);

    CGFloat observedRed = 0;
    CGFloat observedGreen = 0;
    CGFloat observedBlue = 0;
    CGFloat observedAlpha = 0;
    [color getRed:&observedRed green:&observedGreen blue:&observedBlue alpha:&observedAlpha];
    EXPECT_EQ(expectation->red, std::round(observedRed * 255));
    EXPECT_EQ(expectation->green, std::round(observedGreen * 255));
    EXPECT_EQ(expectation->blue, std::round(observedBlue * 255));
    EXPECT_LT(std::abs(expectation->alpha - observedAlpha), std::numeric_limits<CGFloat>::epsilon());
}

TEST(WritingTools, ProofreadingAcceptReject)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>"]);

    constexpr unsigned start = 5;
    constexpr unsigned end = 9;

    [webView waitForNextPresentationUpdate];

    __auto_type modifySelection = ^(unsigned start, unsigned end) {
        NSString *modifySelectionJavascript = [NSString stringWithFormat:@""
            "(() => {"
            "  const first = document.getElementById('p').childNodes[0].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(first, %u);"
            "  range.setEnd(first, %u);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();", start, end];

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    modifySelection(start, end);

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        // Test `willBeginWritingToolsSession`.

        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", contexts.firstObject.attributedText.string);

        EXPECT_EQ(5UL, contexts.firstObject.range.location);
        EXPECT_EQ(end - start, contexts.firstObject.range.length);

        // Test `didReceiveSuggestions`.

        auto firstSuggestion = adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(0, 4) replacement:@"ZZZZ"]);
        auto secondSuggestion = adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(10, 4) replacement:@"YYYY"]);

        auto suggestions = [NSMutableArray array];
        [suggestions addObject:firstSuggestion.get()];
        [suggestions addObject:secondSuggestion.get()];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        auto selectionAfterReceivingSuggestions = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"ZZZZ BBBB YYYY", selectionAfterReceivingSuggestions);

        modifySelection(0, 0);

        NSString *hasFirstMarker = [webView stringByEvaluatingJavaScript:@"internals.hasWritingToolsTextSuggestionMarker(0, 4);"];
        EXPECT_WK_STREQ("1", hasFirstMarker);

        NSString *firstReplacementOriginalString = [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('p').childNodes[0].firstChild, 'writingtoolstextsuggestion', 0);"];
        EXPECT_WK_STREQ(@"('AAAA', state: 0)", firstReplacementOriginalString);

        modifySelection(0, 0);

        NSString *hasSecondMarker = [webView stringByEvaluatingJavaScript:@"internals.hasWritingToolsTextSuggestionMarker(10, 4);"];
        EXPECT_WK_STREQ("1", hasSecondMarker);

        NSString *secondReplacementOriginalString = [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('p').childNodes[0].firstChild, 'writingtoolstextsuggestion', 1);"];
        EXPECT_WK_STREQ(@"('CCCC', state: 0)", secondReplacementOriginalString);

        EXPECT_WK_STREQ(@"ZZZZ BBBB YYYY", [webView contentsAsString]);

        [[webView writingToolsDelegate] proofreadingSession:session.get() didUpdateState:WTTextSuggestionStateReviewing forSuggestionWithUUID:[firstSuggestion uuid] inContext:contexts.firstObject];
        auto selectionAfterReviewing = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"ZZZZ", selectionAfterReviewing);

        [[webView writingToolsDelegate] proofreadingSession:session.get() didUpdateState:WTTextSuggestionStateRejected forSuggestionWithUUID:[secondSuggestion uuid] inContext:contexts.firstObject];
        NSString *hasSecondMarkerAfterStateUpdate = [webView stringByEvaluatingJavaScript:@"internals.hasWritingToolsTextSuggestionMarker(10, 4);"];
        EXPECT_WK_STREQ("0", hasSecondMarkerAfterStateUpdate);

        EXPECT_WK_STREQ(@"ZZZZ BBBB CCCC", [webView contentsAsString]);

        auto selectionBeforeEnd = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        auto selectionAfterEnd = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(selectionBeforeEnd, selectionAfterEnd);

        NSString *hasFirstMarkerAfterEnding = [webView stringByEvaluatingJavaScript:@"internals.hasWritingToolsTextSuggestionMarker(0, 4);"];
        EXPECT_WK_STREQ("0", hasFirstMarkerAfterEnding);

        NSString *hasSecondMarkerAfterEnding = [webView stringByEvaluatingJavaScript:@"internals.hasWritingToolsTextSuggestionMarker(10, 4);"];
        EXPECT_WK_STREQ("0", hasSecondMarkerAfterEnding);

        EXPECT_WK_STREQ(@"ZZZZ BBBB CCCC", [webView contentsAsString]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingWithStreamingSuggestions)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>AAAA BBBB CCCC DDDD</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *originalText = @"AAAA BBBB CCCC DDDD";
    NSString *proofreadText = @"ZZ BBBB CCCC YYYYY";

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:@[ adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(0, 4) replacement:@"ZZ"]).get() ] processedRange:NSMakeRange(0, 4) inContext:contexts.firstObject finished:NO];
        TestWebKitAPI::Util::runFor(0.1_s);

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:@[ adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(15, 4) replacement:@"YYYYY"]).get() ] processedRange:NSMakeRange(15, 4) inContext:contexts.firstObject finished:YES];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingWithLongReplacement)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>I don't thin so. I didn't quite here him.</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *originalText = @"I don't thin so. I didn't quite here him.";
    NSString *proofreadText = @"I don't someveryveryverylongword so. I didn't quite hear him.";

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        auto suggestions = [NSMutableArray array];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(8, 4) replacement:@"someveryveryverylongword"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(32, 4) replacement:@"hear"]).get()];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(@"('thin', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('here', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 1);"]);

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:NO];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingShowOriginal)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>I don't thin so. I didn't quite here him.</p><p id='second'>Who's over they're. I could come their.</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *originalText = @"I don't thin so. I didn't quite here him.\n\nWho's over they're. I could come their.";
    NSString *proofreadText = @"I don't think so. I didn't quite hear him.\n\nWho's over there. I could come there.";

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        auto suggestions = [NSMutableArray array];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(8, 4) replacement:@"think"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(32, 4) replacement:@"hear"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(54, 7) replacement:@"there"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(76, 5) replacement:@"there"]).get()];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(@"('thin', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('here', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 1);"]);
        EXPECT_WK_STREQ(@"('they're', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('second').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('their', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('second').childNodes[0], 'writingtoolstextsuggestion', 1);"]);

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowOriginal];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(@"('think', state: 1)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('hear', state: 1)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 1);"]);
        EXPECT_WK_STREQ(@"('there', state: 1)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('second').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('there', state: 1)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('second').childNodes[0], 'writingtoolstextsuggestion', 1);"]);

        EXPECT_WK_STREQ(originalText, [webView contentsAsString]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowRewritten];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(@"('thin', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('here', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('first').childNodes[0], 'writingtoolstextsuggestion', 1);"]);
        EXPECT_WK_STREQ(@"('they're', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('second').childNodes[0], 'writingtoolstextsuggestion', 0);"]);
        EXPECT_WK_STREQ(@"('their', state: 0)", [webView stringByEvaluatingJavaScript:@"internals.markerDescriptionForNode(document.getElementById('second').childNodes[0], 'writingtoolstextsuggestion', 1);"]);

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:NO];
        EXPECT_WK_STREQ(originalText, [webView contentsAsString]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingRevert)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>Look, Ive been really invested on watchin all the dragon ball sagas, i think iv done a prety good job since now.</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *originalText = @"Look, Ive been really invested on watchin all the dragon ball sagas, i think iv done a prety good job since now.";
    NSString *proofreadText = @"Look, I've been really invested in watching all the Dragon Ball sagas. I think I've done a pretty good job since now.";

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        auto suggestions = [NSMutableArray array];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(6, 3) replacement:@"I've"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(31, 10) replacement:@"in watching"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(50, 11) replacement:@"Dragon Ball"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(62, 8) replacement:@"sagas. I"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(77, 2) replacement:@"I've"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(87, 5) replacement:@"pretty"]).get()];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:NO];
        EXPECT_WK_STREQ(originalText, [webView contentsAsString]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingRevertWithSuggestionAtEndOfText)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>Hey wanna go to the movies this weekend</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *originalText = @"Hey wanna go to the movies this weekend";
    NSString *proofreadText = @"Hey, wanna go to the movies this weekend?";

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        auto suggestions = [NSMutableArray array];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(0, 3) replacement:@"Hey,"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(32, 7) replacement:@"weekend?"]).get()];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:NO];
        EXPECT_WK_STREQ(originalText, [webView contentsAsString]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingWithImage)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p>AAAA BBBB</p><img src='sunset-in-cupertino-200px.png'></img><p>CCCC DDDD</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    // FIXME: Figure out which one of these newline positions is correct, and fix the discrepency;
    // this is just a consequence of testing equality using `_contentsAsAttributedString`.
    NSString *originalText = @"AAAA BBBB\n\n<replacement char>\nCCCC DDDD";
    NSString *proofreadText = @"XXXX BBBB\n<replacement char>\nZZZZ DDDD\n";

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, [contexts.firstObject.attributedText.string _withVisibleReplacementCharacters]);

        auto suggestions = [NSMutableArray array];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(0, 4) replacement:@"XXXX"]).get()];
        [suggestions addObject:adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(13, 4) replacement:@"ZZZZ"]).get()];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        [webView waitForNextPresentationUpdate];

        [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *string, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
            EXPECT_WK_STREQ(proofreadText, [string.string _withVisibleReplacementCharacters]);
            finished = true;
        }];
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ProofreadingWithAttemptedEditing)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:nil]);

    NSString *originalText = @"I frequetly misspell thigs.";
    NSString *proofreadText = @"I frequently misspell things.";

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:[NSString stringWithFormat:@"<body contenteditable><p>%@</p></body>", originalText]]);

    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ(originalText, [webView contentsAsString]);

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:@[ adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(2, 9) replacement:@"frequently"]).get() ] processedRange:NSMakeRange(0, 9) inContext:contexts.firstObject finished:NO];
        TestWebKitAPI::Util::runFor(0.1_s);

        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ([originalText stringByReplacingOccurrencesOfString:@"frequetly" withString:@"frequently"], [webView contentsAsString]);

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:@[ adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(21, 5) replacement:@"things"]).get() ] processedRange:NSMakeRange(0, 27) inContext:contexts.firstObject finished:YES];

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ("Test", [webView contentsAsString]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];
        EXPECT_WK_STREQ("Test", [webView contentsAsString]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithAttemptedEditing)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    NSString *source = @"<html><head></head><body contenteditable dir=\"auto\" style=\"overflow-wrap: break-word; -webkit-nbsp-mode: space; line-break: after-white-space;\"><div>An NSAttributedString object manage character strings and associated sets of attributes (for example, font and kerning) that apply to individual characters or ranges of characters in the string. An association of characters and their attributes is called an attributed string. The cluster's two public classes, NSAttributedString and NSMutableAttributedString, declare the programmatic interface for read-only attbuted strings and modifiable attributed strings, respectively.</div><div><br></div><div>An attributed string identifies attributes by name, using an NSDictionary object to store a value under the specified name. You can assign any attribute name/value pair you wish to a range of characters—it is up to your application to interpret custom attributes (see Attributed String Programming Guide). If you are using attributed strings with the Core Text framework, you can also use the attribute keys defined by that framework</div></body></html>";

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:source]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        NSString *originalText = @"An NSAttributedString object manage character strings and associated sets of attributes (for example, font and kerning) that apply to individual characters or ranges of characters in the string. An association of characters and their attributes is called an attributed string. The cluster's two public classes, NSAttributedString and NSMutableAttributedString, declare the programmatic interface for read-only attbuted strings and modifiable attributed strings, respectively.\n\nAn attributed string identifies attributes by name, using an NSDictionary object to store a value under the specified name. You can assign any attribute name/value pair you wish to a range of characters—it is up to your application to interpret custom attributes (see Attributed String Programming Guide). If you are using attributed strings with the Core Text framework, you can also use the attribute keys defined by that framework";
        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ(originalText, [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"NSAttributedString is a class in the Objective-C programming language that represents a string with attributes. It allows you to set and retrieve attributes for individual characters or ranges of characters in the string. NSAttributedString is a subclass of NSMutableAttributedString, which is a class that allows you to modify the attributes of an attributed string."]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 476) inContext:contexts.firstObject finished:NO];
        TestWebKitAPI::Util::runFor(0.1_s);

        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ("NSAttributedString is a class in the Objective-C programming language that represents a string with attributes. It allows you to set and retrieve attributes for individual characters or ranges of characters in the string. NSAttributedString is a subclass of NSMutableAttributedString, which is a class that allows you to modify the attributes of an attributed string.\nAn attributed string identifies attributes by name, using an NSDictionary object to store a value under the specified name. You can assign any attribute name/value pair you wish to a range of characters—it is up to your application to interpret custom attributes (see Attributed String Programming Guide). If you are using attributed strings with the Core Text framework, you can also use the attribute keys defined by that framework", [webView contentsAsStringWithoutNBSP]);

        NSString *result = @"NSAttributedString is a class in the Objective-C programming language that represents a string with attributes. It allows you to set and retrieve attributes for individual characters or ranges of characters in the string. NSAttributedString is a subclass of NSMutableAttributedString, which is a class that allows you to modify the attributes of an attributed string.\n\nAn attributed string is a type of string that can contain attributes. Attributes are properties that can be set on a string and can be accessed and manipulated by other code. The attributes can be used to add custom information to a string, such as colors, fonts, or images. The Core Text framework provides a set of attribute keys that can be used to define the attributes that can be used in attributed strings.";

        auto longerAttributedText = adoptNS([[NSAttributedString alloc] initWithString:result]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:longerAttributedText.get() replacementRange:NSMakeRange(0, 910) inContext:contexts.firstObject finished:YES];


        [webView waitForContentValue:result];
        EXPECT_WK_STREQ(result, [webView contentsAsStringWithoutNBSP]);

        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ(result, [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];
        [webView attemptEditingForTesting];
        EXPECT_WK_STREQ("Test", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithUndoAfterEnding)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>Hey do you wanna go see a movie this weekend</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"Hey do you wanna go see a movie this weekend", contexts.firstObject.attributedText.string);

        __auto_type rewrite = ^(NSString *rewrittenText) {
            [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

            auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:rewrittenText]);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 44) inContext:contexts.firstObject finished:NO];
            TestWebKitAPI::Util::runFor(0.1_s);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 44) inContext:contexts.firstObject finished:YES];

            [webView waitForContentValue:rewrittenText];
            EXPECT_WK_STREQ(rewrittenText, [webView contentsAsStringWithoutNBSP]);
        };

        rewrite(@"A");

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];
        EXPECT_WK_STREQ(@"Hey do you wanna go see a movie this weekend", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithUndo)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>Hey do you wanna go see a movie this weekend</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"Hey do you wanna go see a movie this weekend", contexts.firstObject.attributedText.string);

        __auto_type rewrite = ^(NSString *rewrittenText) {
            [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

            auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:rewrittenText]);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 44) inContext:contexts.firstObject finished:NO];
            TestWebKitAPI::Util::runFor(0.1_s);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 44) inContext:contexts.firstObject finished:YES];

            [webView waitForContentValue:rewrittenText];
            EXPECT_WK_STREQ(rewrittenText, [webView contentsAsStringWithoutNBSP]);
        };

        rewrite(@"A");
        rewrite(@"B");
        rewrite(@"C");

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];
        EXPECT_WK_STREQ(@"B", [webView contentsAsStringWithoutNBSP]);

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];
        EXPECT_WK_STREQ(@"A", [webView contentsAsStringWithoutNBSP]);

        [webView _synchronouslyExecuteEditCommand:@"Redo" argument:@""];
        EXPECT_WK_STREQ(@"B", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithMultipleUndosAndRestarts)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>Hey do you wanna go see a movie this weekend - start</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"Hey do you wanna go see a movie this weekend - start", contexts.firstObject.attributedText.string);

        __auto_type rewrite = ^(NSString *rewrittenText) {
            [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

            auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:rewrittenText]);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 52) inContext:contexts.firstObject finished:NO];
            TestWebKitAPI::Util::runFor(0.1_s);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 52) inContext:contexts.firstObject finished:YES];

            [webView waitForContentValue:rewrittenText];
            EXPECT_WK_STREQ(rewrittenText, [webView contentsAsStringWithoutNBSP]);
        };

        rewrite(@"Would you like to see a movie? - first");
        rewrite(@"I'm going to a film, would you like to join? - second");

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];
        EXPECT_WK_STREQ(@"Would you like to see a movie? - first", [webView contentsAsStringWithoutNBSP]);

        rewrite(@"Hey I'm going to a movie and I'd love for you to join me! - third");

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];
        EXPECT_WK_STREQ(@"Would you like to see a movie? - first", [webView contentsAsStringWithoutNBSP]);

        rewrite(@"We're watching a movie if you want to come! - fourth");

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithUndoAndRestart)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>Hey do you wanna go see a movie this weekend</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    NSString *originalText = @"Hey do you wanna go see a movie this weekend";

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        __auto_type rewrite = ^(NSString *rewrittenText) {
            [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

            auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:rewrittenText]);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 44) inContext:contexts.firstObject finished:NO];
            TestWebKitAPI::Util::runFor(0.1_s);

            [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 44) inContext:contexts.firstObject finished:YES];

            [webView waitForContentValue:rewrittenText];
            EXPECT_WK_STREQ(rewrittenText, [webView contentsAsStringWithoutNBSP]);
        };

        rewrite(@"A");
        rewrite(@"B");

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowOriginal];
        [webView waitForContentValue:originalText];
        EXPECT_WK_STREQ(originalText, [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowRewritten];
        [webView waitForContentValue:@"B"];
        EXPECT_WK_STREQ(@"B", [webView contentsAsStringWithoutNBSP]);

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];
        [webView waitForContentValue:@"A"];
        EXPECT_WK_STREQ(@"A", [webView contentsAsStringWithoutNBSP]);

        rewrite(@"C");

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:NO];
        EXPECT_WK_STREQ(originalText, [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, Composition)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    constexpr unsigned start = 5;
    constexpr unsigned end = 9;

    [webView waitForNextPresentationUpdate];

    __auto_type modifySelection = ^(unsigned start, unsigned end) {
        NSString *modifySelectionJavascript = [NSString stringWithFormat:@""
            "(() => {"
            "  const first = document.getElementById('p').childNodes[0].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(first, %u);"
            "  range.setEnd(first, %u);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();", start, end];

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    modifySelection(start, end);

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", contexts.firstObject.attributedText.string);

        EXPECT_EQ(5UL, contexts.firstObject.range.location);
        EXPECT_EQ(end - start, contexts.firstObject.range.length);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", [webView contentsAsStringWithoutNBSP]);

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"ZZZZ"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(5, 4) inContext:contexts.firstObject finished:NO];

        [webView waitForContentValue:@"AAAA ZZZZ CCCC"];
        EXPECT_WK_STREQ(@"AAAA ZZZZ CCCC", [webView contentsAsStringWithoutNBSP]);

        auto longerAttributedText = adoptNS([[NSAttributedString alloc] initWithString:@"ZZZZX YYY"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:longerAttributedText.get() replacementRange:NSMakeRange(5, 9) inContext:contexts.firstObject finished:YES];

        [webView waitForContentValue:@"AAAA ZZZZX YYY"];
        EXPECT_WK_STREQ(@"AAAA ZZZZX YYY", [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowOriginal];

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowRewritten];

        EXPECT_WK_STREQ(@"AAAA ZZZZX YYY", [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        [webView waitForContentValue:@"AAAA ZZZZX YYY"];
        EXPECT_WK_STREQ(@"AAAA ZZZZX YYY", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionRevert)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", contexts.firstObject.attributedText.string);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", [webView contentsAsStringWithoutNBSP]);

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"ZZZZ"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(5, 4) inContext:contexts.firstObject finished:YES];

        [webView waitForContentValue:@"AAAA ZZZZ CCCC"];
        EXPECT_WK_STREQ(@"AAAA ZZZZ CCCC", [webView contentsAsStringWithoutNBSP]);

        [webView _synchronouslyExecuteEditCommand:@"Undo" argument:@""];

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:NO];

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithAttributedStringAttributes)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'><span style=\"color: blue\">AAAA</span> <span style=\"color: red\">BBBB</span> CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    constexpr unsigned start = 0;
    constexpr unsigned end = 4;

    [webView waitForNextPresentationUpdate];

    __auto_type modifySelection = ^(unsigned start, unsigned end) {
        NSString *modifySelectionJavascript = [NSString stringWithFormat:@""
            "(() => {"
            "  const spanElement = document.getElementById('p').childNodes[0].children[1].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(spanElement, %u);"
            "  range.setEnd(spanElement, %u);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();", start, end];

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    modifySelection(start, end);

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", contexts.firstObject.attributedText.string);

        __block size_t i = 0;
        [contexts.firstObject.attributedText enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, [contexts.firstObject.attributedText length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL *stop) {
            switch (i) {
            case 0: // "AAAA"
                EXPECT_NOT_NULL(value);
                checkColor(value, { { 0, 0, 255, 1 } });
                break;

            case 1: // " "
                EXPECT_NOT_NULL(value);
                checkColor(value, { { 0, 0, 0, 1 } });
                break;

            case 2: // "BBBB"
                EXPECT_NOT_NULL(value);
                checkColor(value, { { 255, 0, 0, 1 } });
                break;

            case 3: // " "
                EXPECT_NOT_NULL(value);
                checkColor(value, { { 0, 0, 0, 1 } });
                break;

            default:
                ASSERT_NOT_REACHED();
                break;
            }

            ++i;
        }];

        EXPECT_EQ(i, 4UL);

        EXPECT_EQ(5UL, contexts.firstObject.range.location);
        EXPECT_EQ(end - start, contexts.firstObject.range.length);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", [webView contentsAsStringWithoutNBSP]);

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"ZZZZ" attributes:@{ NSForegroundColorAttributeName : [WebCore::CocoaColor greenColor] }]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(5, 4) inContext:contexts.firstObject finished:YES];
        TestWebKitAPI::Util::runFor(0.1_s);

        [webView _getContentsAsAttributedStringWithCompletionHandler:^(NSAttributedString *string, NSDictionary<NSAttributedStringDocumentAttributeKey, id> *attributes, NSError *error) {
            EXPECT_NULL(error);

            __block size_t i = 0;
            [string enumerateAttribute:NSForegroundColorAttributeName inRange:NSMakeRange(0, [string length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL *stop) {
                switch (i) {
                case 0: // "AAAA"
                    EXPECT_NOT_NULL(value);
                    checkColor(value, { { 0, 0, 255, 1 } });
                    break;

                case 1: // " "
                    EXPECT_NULL(value);
                    break;

                case 2: // "ZZZZ"
                    EXPECT_NOT_NULL(value);

#if PLATFORM(MAC)
                    checkColor(value, { { 34, 255, 6, 1 } });
#else
                    checkColor(value, { { 0, 255, 0, 1 } });
#endif

                    break;

                case 3: // " "
                    EXPECT_NULL(value);
                    break;

                default:
                    ASSERT_NOT_REACHED();
                    break;
                }

                ++i;
            }];

            finished = true;
        }];
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithUnderline)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><a href='https://www.webkit.org'>Link</a><br><p>Text</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        RetainPtr<NSAttributedString> attributedText = contexts.firstObject.attributedText;

        EXPECT_WK_STREQ("Link", [[attributedText string] substringWithRange:NSMakeRange(0, 4)]);
        [attributedText enumerateAttribute:NSUnderlineStyleAttributeName inRange:NSMakeRange(0, 4) options:0 usingBlock:^(id value, NSRange, BOOL *) {
            EXPECT_EQ(NSUnderlineStyleSingle, [value integerValue]);
        }];

        EXPECT_WK_STREQ("Text", [[attributedText string] substringWithRange:NSMakeRange(5, 4)]);
        [attributedText enumerateAttribute:NSUnderlineStyleAttributeName inRange:NSMakeRange(5, 4) options:0 usingBlock:^(id value, NSRange, BOOL *) {
            EXPECT_NULL(value);
        }];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#if PLATFORM(MAC)
static RetainPtr<NSAttributedString> makeTableAttributedString(NSArray<NSArray<NSString *> *> *rawTable)
{
    if (!rawTable.count)
        return nil;

    RetainPtr textTable = adoptNS([[NSTextTable alloc] init]);
    [textTable setNumberOfColumns:rawTable.firstObject.count];

    RetainPtr result = adoptNS([[NSMutableAttributedString alloc] init]);

    for (NSUInteger row = 0; row < rawTable.count; row++) {
        for (NSUInteger column = 0; column < rawTable[row].count; column++) {
            RetainPtr textTableBlock = adoptNS([[NSTextTableBlock alloc] initWithTable:textTable.get() startingRow:row rowSpan:1 startingColumn:column columnSpan:1]);

            RetainPtr paragraphStyle = adoptNS([[NSMutableParagraphStyle alloc] init]);
            [paragraphStyle setTextBlocks:@[ textTableBlock.get() ]];

            RetainPtr attributedString = adoptNS([[NSAttributedString alloc] initWithString:[rawTable[row][column] stringByAppendingString:@"\n"] attributes:@{
                NSParagraphStyleAttributeName : paragraphStyle.get()
            }]);

            [result appendAttributedString:attributedString.get()];
        }
    }

    return result;
}

TEST(WritingTools, CompositionWithTable)
{
    RetainPtr session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    RetainPtr webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@""
        "<body contenteditable>"
        "   <div>Task Name, Start Date, End Date, Status</div>"
        "   <div>Task A, 2024-01-01, 2024-01-10, In Progress</div>"
        "   <div>Task B, 2024-01-05, 2024-01-15, Completed</div>"
        "</body>"
    ]);

    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        RetainPtr attributedText = makeTableAttributedString(@[
            @[ @"Task Name", @"Start Date", @"End Date", @"Status" ],
            @[ @"Task A", @"2024-01-01", @"2024-01-10", @"In Progress" ],
            @[ @"Task B", @"2024-01-05", @"2024-01-15", @"Completed" ],
        ]);

        // Invoke `didReceiveText` multiple times to ensure multiple tables are *not* created.

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 27) inContext:contexts.firstObject finished:NO];
        TestWebKitAPI::Util::runFor(0.1_s);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 27) inContext:contexts.firstObject finished:YES];
        TestWebKitAPI::Util::runFor(0.1_s);

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('table').length"].intValue, 1);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('tr').length"].intValue, 3);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('td').length"].intValue, 12);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowOriginal];

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('table').length"].intValue, 0);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('tr').length"].intValue, 0);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('td').length"].intValue, 0);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowRewritten];

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('table').length"].intValue, 1);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('tr').length"].intValue, 3);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('td').length"].intValue, 12);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}
#endif

TEST(WritingTools, CompositionWithList)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p>Broccoli, peas, and carrots</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        RetainPtr list = adoptNS([[NSTextList alloc] initWithMarkerFormat:NSTextListMarkerDisc options:0]);
        RetainPtr paragraph = adoptNS([[NSParagraphStyle defaultParagraphStyle] mutableCopy]);
        [paragraph setTextLists:[NSArray arrayWithObject:list.get()]];

        RetainPtr attributes = [NSDictionary dictionaryWithObjectsAndKeys:paragraph.get(), NSParagraphStyleAttributeName, nil];
        RetainPtr attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"\tBroccoli\n\tPeas\n\tCarrots\n" attributes:attributes.get()]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 27) inContext:contexts.firstObject finished:YES];
        [webView waitForNextPresentationUpdate];

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('li').length"].intValue, 3);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('ul').length"].intValue, 1);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowOriginal];

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('li').length"].intValue, 0);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('ul').length"].intValue, 0);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowRewritten];

        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('li').length"].intValue, 3);
        EXPECT_EQ([webView stringByEvaluatingJavaScript:@"document.getElementsByTagName('ul').length"].intValue, 1);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithTextAttachment)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p>Sunset in Cupertino</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    auto waitForValue = [webView]() {
        do {
            if ([webView stringByEvaluatingJavaScript:@"document.querySelector('img').complete"].boolValue)
                break;

            TestWebKitAPI::Util::runFor(0.1_s);
        } while (true);
    };

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        RetainPtr pngData = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-200px" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
        RetainPtr attachment = adoptNS([[NSTextAttachment alloc] initWithData:pngData.get() ofType:UTTypePNG.identifier]);

        RetainPtr attributedText = [NSAttributedString attributedStringWithAttachment:attachment.get()];

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 19) inContext:contexts.firstObject finished:YES];
        TestWebKitAPI::Util::runFor(0.1_s); // this is needed to let the information load at least a bit or we can fail on a javascript issue.
        waitForValue();

        EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.querySelector('img').complete"].boolValue);
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('img').width"], "200");
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('img').height"], "150");

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

RetainPtr<_WKAttachment> synchronouslyInsertAttachmentWithFilename(TestWKWebView *webView, NSString *filename, NSString *contentType, NSData *data)
{
    RetainPtr fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:data]);
    if (filename)
        [fileWrapper setPreferredFilename:filename];

    __block bool done = false;
    RetainPtr attachment = [webView _insertAttachmentWithFileWrapper:fileWrapper.get() contentType:contentType completion:^(BOOL) {
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return attachment;
}

TEST(WritingTools, CompositionWithImageRoundTrip)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<p>AAAA BBBB</p><img src='sunset-in-cupertino-200px.png'></img><p>CCCC DDDD</p>"]);

    [webView selectAll:nil];

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:contexts.firstObject.attributedText replacementRange:NSMakeRange(0, contexts.firstObject.attributedText.length) inContext:contexts.firstObject finished:YES];
        [webView waitForNextPresentationUpdate];

        EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"document.querySelector('img').complete"].boolValue);
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('img').width"], "200");
        EXPECT_WK_STREQ([webView stringByEvaluatingJavaScript:@"document.querySelector('img').height"], "150");

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithImageAttachmentRoundTrip)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<p>AAAA BBBB</p><img src='sunset-in-cupertino-200px.png'></img><p>CCCC DDDD</p>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete attachmentElementEnabled:YES]);

    [webView selectAll:nil];

    [webView waitForNextPresentationUpdate];

    RetainPtr attachmentIdentifier = [webView ensureAttachmentForImageElement];
    RetainPtr attachment = [webView _attachmentForIdentifier:attachmentIdentifier.get()];

    RetainPtr pngData = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"sunset-in-cupertino-200px" ofType:@"png" inDirectory:@"TestWebKitAPI.resources"]];
    RetainPtr fileWrapper = adoptNS([[NSFileWrapper alloc] initRegularFileWithContents:pngData.get()]);
    [fileWrapper setPreferredFilename:@"sunset-in-cupertino-200px.png"];

    BOOL errorOccurred = [attachment synchronouslySetFileWrapper:fileWrapper.get() newContentType:@"image/png"];
    EXPECT_EQ(NO, errorOccurred);

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:contexts.firstObject.attributedText replacementRange:NSMakeRange(0, contexts.firstObject.attributedText.length) inContext:contexts.firstObject finished:YES];
        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"internals.shadowRoot(document.querySelector('img')).querySelector('attachment').uniqueIdentifier"]);
        EXPECT_WK_STREQ("image/png", [webView stringByEvaluatingJavaScript:@"internals.shadowRoot(document.querySelector('img')).querySelector('attachment').getAttribute('type')"]);
        EXPECT_WK_STREQ("sunset-in-cupertino-200px.png", [webView stringByEvaluatingJavaScript:@"internals.shadowRoot(document.querySelector('img')).querySelector('attachment').getAttribute('title')"]);

        EXPECT_TRUE([[attachment info].fileWrapper.regularFileContents isEqualToData:pngData.get()]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithNonImageAttachmentRoundTrip)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete attachmentElementEnabled:YES]);

    __auto_type modifySelection = ^(unsigned start, unsigned end) {
        NSString *modifySelectionJavascript = [NSString stringWithFormat:@""
            "(() => {"
            "  const first = document.getElementById('p').childNodes[0].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(first, %u);"
            "  range.setEnd(first, %u);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();", start, end];

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    modifySelection(5, 9);

    RetainPtr zipData = [NSData dataWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"compressed-files" ofType:@"zip" inDirectory:@"TestWebKitAPI.resources"]];
    RetainPtr attachment = synchronouslyInsertAttachmentWithFilename(webView.get(), @"compressed-files.zip", @"application/zip", zipData.get());

    [webView selectAll:nil];

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:contexts.firstObject.attributedText replacementRange:NSMakeRange(0, contexts.firstObject.attributedText.length) inContext:contexts.firstObject finished:YES];
        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ([attachment uniqueIdentifier], [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').uniqueIdentifier"]);
        EXPECT_WK_STREQ("application/zip", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('type')"]);
        EXPECT_WK_STREQ("compressed-files.zip", [webView stringByEvaluatingJavaScript:@"document.querySelector('attachment').getAttribute('title')"]);

        EXPECT_TRUE([[attachment info].fileWrapper.regularFileContents isEqualToData:zipData.get()]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithSystemFont)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p style='font: -apple-system-body;'>Early morning in Cupertino</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        RetainPtr attributes = [contexts.firstObject.attributedText attributesAtIndex:0 effectiveRange:0];
        RetainPtr attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"Cupertino at the crack of dawn" attributes:attributes.get()]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 26) inContext:contexts.firstObject finished:YES];
        [webView waitForNextPresentationUpdate];

        EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!document.body.innerHTML.includes('.AppleSystemUIFont')"].boolValue);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithMultipleChunks)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    NSString *source = @"<html><head></head><body contenteditable dir=\"auto\" style=\"overflow-wrap: break-word; -webkit-nbsp-mode: space; line-break: after-white-space;\"><div>An NSAttributedString object manage character strings and associated sets of attributes (for example, font and kerning) that apply to individual characters or ranges of characters in the string. An association of characters and their attributes is called an attributed string. The cluster's two public classes, NSAttributedString and NSMutableAttributedString, declare the programmatic interface for read-only attbuted strings and modifiable attributed strings, respectively.</div><div><br></div><div>An attributed string identifies attributes by name, using an NSDictionary object to store a value under the specified name. You can assign any attribute name/value pair you wish to a range of characters—it is up to your application to interpret custom attributes (see Attributed String Programming Guide). If you are using attributed strings with the Core Text framework, you can also use the attribute keys defined by that framework</div></body></html>";

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:source]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        NSString *originalText = @"An NSAttributedString object manage character strings and associated sets of attributes (for example, font and kerning) that apply to individual characters or ranges of characters in the string. An association of characters and their attributes is called an attributed string. The cluster's two public classes, NSAttributedString and NSMutableAttributedString, declare the programmatic interface for read-only attbuted strings and modifiable attributed strings, respectively.\n\nAn attributed string identifies attributes by name, using an NSDictionary object to store a value under the specified name. You can assign any attribute name/value pair you wish to a range of characters—it is up to your application to interpret custom attributes (see Attributed String Programming Guide). If you are using attributed strings with the Core Text framework, you can also use the attribute keys defined by that framework";
        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"NSAttributedString is a class in the Objective-C programming language that represents a string with attributes. It allows you to set and retrieve attributes for individual characters or ranges of characters in the string. NSAttributedString is a subclass of NSMutableAttributedString, which is a class that allows you to modify the attributes of an attributed string."]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 476) inContext:contexts.firstObject finished:NO];
        TestWebKitAPI::Util::runFor(0.1_s);

        NSString *result = @"NSAttributedString is a class in the Objective-C programming language that represents a string with attributes. It allows you to set and retrieve attributes for individual characters or ranges of characters in the string. NSAttributedString is a subclass of NSMutableAttributedString, which is a class that allows you to modify the attributes of an attributed string.\n\nAn attributed string is a type of string that can contain attributes. Attributes are properties that can be set on a string and can be accessed and manipulated by other code. The attributes can be used to add custom information to a string, such as colors, fonts, or images. The Core Text framework provides a set of attribute keys that can be used to define the attributes that can be used in attributed strings.";

        auto longerAttributedText = adoptNS([[NSAttributedString alloc] initWithString:result]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:longerAttributedText.get() replacementRange:NSMakeRange(0, 910) inContext:contexts.firstObject finished:YES];

        [webView waitForContentValue:result];
        EXPECT_WK_STREQ(result, [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowOriginal];
        TestWebKitAPI::Util::runFor(0.1_s);

        [webView waitForContentValue:originalText];
        EXPECT_WK_STREQ(originalText, [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionShowRewritten];

        [webView waitForContentValue:result];
        EXPECT_WK_STREQ(result, [webView contentsAsStringWithoutNBSP]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        [webView waitForContentValue:result];
        EXPECT_WK_STREQ(result, [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithTrailingNewlines)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    NSString *source = @"<body contenteditable id='first'><p style=\"margin: 0px;\">Hey wanna go to the movies this weekend</p><p style=\"margin: 0px;\"><br></p><p style=\"margin: 0px;\">A</p></body>";

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:source]);
    [webView focusDocumentBodyAndSelectAll];

    __auto_type modifySelection = ^{
        NSString *modifySelectionJavascript = @""
            "(() => {"
            "  const first = document.getElementById('first').childNodes[0].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(first, 0);"
            "  range.setEnd(first, 39);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();";

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    modifySelection();

    EXPECT_WK_STREQ(@"Hey wanna go to the movies this weekend\n\nA", [webView contentsAsStringWithoutNBSP]);

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"Hey wanna go to the movies this weekend", contexts.firstObject.attributedText.string);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"Hey, wanna catch a flick this weekend?"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 39) inContext:contexts.firstObject finished:YES];

        [webView waitForContentValue:@"Hey, wanna catch a flick this weekend?\n\nA"];
        EXPECT_WK_STREQ(@"Hey, wanna catch a flick this weekend?\n\nA", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, CompositionWithTrailingBreaks)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    NSString *source = @"<html><head></head><body contenteditable dir=\"auto\"><div><div>On Mar 5, 224, Bob wrote:</div><div><p>A</p></div><br></div></body></html>";

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:source]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"On March 5, 224, Bob wrote:\nA\n\n"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 26) inContext:contexts.firstObject finished:YES];

        [webView waitForContentValue:@"On March 5, 224, Bob wrote:\nA\n\nA\n\n"];
        EXPECT_WK_STREQ(@"On March 5, 224, Bob wrote:\nA\n\nA\n\n", [webView contentsAsStringWithoutNBSP]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

static bool didInvokeShowDetailsForSuggestionWithUUID = false;
static bool didInvokeUpdateState = false;

@interface WKConcreteWTTextViewDelegate : NSObject <WTTextViewDelegate>

- (instancetype)initWithWritingToolsDelegate:(id<WTWritingToolsDelegate>)writingToolsDelegate suggestions:(NSArray<WTTextSuggestion *> *)suggestions expectedRects:(Vector<WebCore::IntRect>)expectedRects;

@property (nonatomic, weak) WTSession *session;

@property (nonatomic, weak) WTContext *context;

@end

@implementation WKConcreteWTTextViewDelegate {
    NSUInteger _showDetailsForSuggestionWithUUIDCount;
    NSUInteger _updateStateCount;

    id<WTWritingToolsDelegate> _writingToolsDelegate;
    NSArray<WTTextSuggestion *> *_suggestions;

    Vector<WebCore::IntRect> _expectedRects;
}

- (instancetype)initWithWritingToolsDelegate:(id<WTWritingToolsDelegate>)writingToolsDelegate suggestions:(NSArray<WTTextSuggestion *> *)suggestions expectedRects:(Vector<WebCore::IntRect>)expectedRects
{
    if (!(self = [super init]))
        return nil;

    _writingToolsDelegate = writingToolsDelegate;
    _suggestions = suggestions;
    _expectedRects = expectedRects;

    return self;
}

- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID updateState:(WTTextSuggestionState)state forSuggestionWithUUID:(NSUUID *)suggestionUUID
{
    didInvokeUpdateState = true;

    WTTextSuggestion *selectedSuggestion = nil;
    for (WTTextSuggestion *suggestion in _suggestions) {
        if ([suggestion.uuid isEqual:suggestionUUID]) {
            selectedSuggestion = suggestion;
            break;
        }
    }

    EXPECT_NOT_NULL(selectedSuggestion);

    [_writingToolsDelegate proofreadingSession:_session didUpdateState:WTTextSuggestionStateReviewing forSuggestionWithUUID:selectedSuggestion.uuid inContext:_context];
}

#if PLATFORM(MAC)
- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID showDetailsForSuggestionWithUUID:(NSUUID *)suggestionUUID relativeToRect:(CGRect)rect inView:(NSView *)sourceView
#else
- (void)proofreadingSessionWithUUID:(NSUUID *)sessionUUID showDetailsForSuggestionWithUUID:(NSUUID *)suggestionUUID relativeToRect:(CGRect)rect inView:(UIView *)sourceView
#endif
{
    WebCore::IntRect convertedRect { WebCore::FloatRect { rect } };

    auto expectedRect = _expectedRects[_showDetailsForSuggestionWithUUIDCount];

    EXPECT_EQ(convertedRect, expectedRect);

    _showDetailsForSuggestionWithUUIDCount++;

    didInvokeShowDetailsForSuggestionWithUUID = true;
}

- (void)textSystemWillBeginEditingDuringSessionWithUUID:(NSUUID *)sessionUUID
{
}

@end

TEST(WritingTools, RevealOffScreenSuggestionWhenActive)
{
    auto firstSuggestion = adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(0, 4) replacement:@"ZZZZ"]);
    auto secondSuggestion = adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(12, 4) replacement:@"YYYY"]);

    auto suggestions = [NSMutableArray array];
    [suggestions addObject:firstSuggestion.get()];
    [suggestions addObject:secondSuggestion.get()];

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable style='font-size: 40px; line-height: 1000px;'><p id='first'>AAAA</p><p id='second'>BBBB</p><p id='third'>CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *originalText = @"AAAA\n\nBBBB\n\nCCCC";
    NSString *proofreadText = @"ZZZZ\n\nBBBB\n\nYYYY";

#if PLATFORM(MAC)
    const Vector<WebCore::IntRect> expectedRects {
        { { 8, 264 }, { 116, 46 } },
    };
#else
    const Vector<WebCore::IntRect> expectedRects {
        { { 8, 2568 }, { 116, 45 } },
    };
#endif

    auto textViewDelegate = adoptNS([[WKConcreteWTTextViewDelegate alloc] initWithWritingToolsDelegate:[webView writingToolsDelegate] suggestions:suggestions expectedRects:expectedRects]);

    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:(id<WTTextViewDelegate>)textViewDelegate.get()]);
    [textViewDelegate setSession:session.get()];

    [webView selectAll:nil];

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);
        EXPECT_WK_STREQ(originalText, contexts.firstObject.attributedText.string);

        [textViewDelegate setContext:contexts.firstObject];

        EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue]);

        // FIXME: This method should not result in the scroll position changing.
        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        [webView objectByEvaluatingJavaScript:@"window.scrollTo(0, 0);"];
        EXPECT_EQ(0, [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue]);

        [textViewDelegate proofreadingSessionWithUUID:[session uuid] updateState:WTTextSuggestionStateReviewing forSuggestionWithUUID:[secondSuggestion uuid]];

        [webView waitForNextPresentationUpdate];

        EXPECT_WK_STREQ(proofreadText, [webView contentsAsString]);

#if PLATFORM(MAC)
        EXPECT_EQ(2304, [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue]);
#else
        EXPECT_EQ(1708, [[webView objectByEvaluatingJavaScript:@"window.scrollY"] intValue]);
#endif

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ShowDetailsForSuggestions)
{
    auto firstSuggestion = adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(0, 4) replacement:@"ZZZZ"]);
    auto secondSuggestion = adoptNS([[WTTextSuggestion alloc] initWithOriginalRange:NSMakeRange(10, 4) replacement:@"YYYY"]);

    auto suggestions = [NSMutableArray array];
    [suggestions addObject:firstSuggestion.get()];
    [suggestions addObject:secondSuggestion.get()];

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

#if PLATFORM(MAC)
    const Vector<WebCore::IntRect> expectedRects {
        { { 8, 9 }, { 40, 18 } },
        { { 97, 9 }, { 47, 18 } },
    };
#else
    const Vector<WebCore::IntRect> expectedRects {
        { { 8, 9 }, { 40, 19 } },
        { { 97, 9 }, { 47, 19 } },
    };
#endif

    auto textViewDelegate = adoptNS([[WKConcreteWTTextViewDelegate alloc] initWithWritingToolsDelegate:[webView writingToolsDelegate] suggestions:suggestions expectedRects:expectedRects]);

    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeProofreading textViewDelegate:(id<WTTextViewDelegate>)textViewDelegate.get()]);
    [textViewDelegate setSession:session.get()];

    constexpr unsigned start = 5;
    constexpr unsigned end = 9;

    [webView waitForNextPresentationUpdate];

    __auto_type modifySelection = ^(unsigned start, unsigned end) {
        NSString *modifySelectionJavascript = [NSString stringWithFormat:@""
            "(() => {"
            "  const first = document.getElementById('p').childNodes[0].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(first, %u);"
            "  range.setEnd(first, %u);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();", start, end];

        [webView stringByEvaluatingJavaScript:modifySelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    modifySelection(start, end);

    __block bool finished = false;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", contexts.firstObject.attributedText.string);

        EXPECT_EQ(5UL, contexts.firstObject.range.location);
        EXPECT_EQ(end - start, contexts.firstObject.range.length);

        [textViewDelegate setContext:contexts.firstObject];

        [[webView writingToolsDelegate] proofreadingSession:session.get() didReceiveSuggestions:suggestions processedRange:NSMakeRange(NSNotFound, 0) inContext:contexts.firstObject finished:YES];

        modifySelection(1, 1);

        TestWebKitAPI::Util::run(&didInvokeShowDetailsForSuggestionWithUUID);

        didInvokeShowDetailsForSuggestionWithUUID = false;
        didInvokeUpdateState = false;

        modifySelection(6, 6);

        EXPECT_FALSE(didInvokeShowDetailsForSuggestionWithUUID);
        EXPECT_FALSE(didInvokeUpdateState);

        modifySelection(11, 11);

        TestWebKitAPI::Util::run(&didInvokeShowDetailsForSuggestionWithUUID);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, WantsInlineEditing)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable>Hello World</body>" writingToolsBehavior:PlatformWritingToolsBehaviorDefault]);

    [webView _setEditable:NO];
    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorNone);

    [webView _setEditable:YES];
    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorComplete);
}

TEST(WritingTools, WritingToolsBehaviorNonEditableWithSelection)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body>Hello World</body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete]);

    [webView selectAll:nil];
    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorLimited);
}

TEST(WritingTools, WritingToolsBehaviorWithNoSelection)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable>Hello World</body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete]);

    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorNone);
}

TEST(WritingTools, WritingToolsBehaviorEditableWithSelection)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable>Hello World</body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete]);
    [webView focusDocumentBodyAndSelectAll];

    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorComplete);
}

TEST(WritingTools, AllowedInputOptionsNonEditable)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body></body>"]);

    EXPECT_EQ(PlatformWritingToolsResultPlainText | PlatformWritingToolsResultRichText | PlatformWritingToolsResultList | PlatformWritingToolsResultTable, [webView allowedWritingToolsResultOptionsForTesting]);
}

TEST(WritingTools, AllowedInputOptionsEditable)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body></body>"]);
    [webView _setEditable:YES];
    [webView focusDocumentBodyAndSelectAll];

    EXPECT_EQ(PlatformWritingToolsResultPlainText | PlatformWritingToolsResultRichText | PlatformWritingToolsResultList | PlatformWritingToolsResultTable, [webView allowedWritingToolsResultOptionsForTesting]);
}

TEST(WritingTools, AllowedInputOptionsRichText)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    EXPECT_EQ(PlatformWritingToolsResultPlainText | PlatformWritingToolsResultRichText | PlatformWritingToolsResultList | PlatformWritingToolsResultTable, [webView allowedWritingToolsResultOptionsForTesting]);
}

TEST(WritingTools, AllowedInputOptionsPlainText)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable=\"plaintext-only\"></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    EXPECT_EQ(PlatformWritingToolsResultPlainText, [webView allowedWritingToolsResultOptionsForTesting]);
}

TEST(WritingTools, EphemeralSessionWithDifferingTextLengths)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<p>AAAA BBBB CCCC DDDD</p><img src='sunset-in-cupertino-200px.png'></img><p>CCCC DDDD</p>" writingToolsBehavior:PlatformWritingToolsBehaviorDefault]);
    [webView _setEditable:NO];
    [webView selectAll:nil];

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;

    // Temporary workaround until WritingTools API is updated for nullability.
    WTSession *session = nil;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, EphemeralSessionWithDeeplyNestedContent)
{
    NSString *text = @"The oceanic whitetip shark is a large requiem shark inhabiting tropical and warm temperate seas. It has a stocky body with long, white-tipped, rounded fins. The species is typically solitary but can congregate around food concentrations.";

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:[NSString stringWithFormat:@"<body><div><div><div><div><div><div><div><div><div><div><p>%@</p></div></div></div></div></div></div></div></div></div></div></body>", text] writingToolsBehavior:PlatformWritingToolsBehaviorDefault]);
    [webView selectAll:nil];

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:nil requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);
        EXPECT_WK_STREQ(text, contexts.firstObject.attributedText.string);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, EphemeralSession)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>This is the first sentence in a paragraph I want to rewrite. Only this sentence is selected.</p><p id='second'>This is the first sentence of the second paragraph. The previous sentence is selected, but this one is not.</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *setSelectionJavaScript = @""
        "(() => {"
        "  const range = document.createRange();"
        "  range.setStart(document.getElementById('first').childNodes[0], 61);"
        "  range.setEnd(document.getElementById('second').childNodes[0], 51);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";
    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    NSString *fullText = @"This is the first sentence in a paragraph I want to rewrite. Only this sentence is selected.\n\nThis is the first sentence of the second paragraph. The previous sentence is selected, but this one is not.";
    NSString *selectedText = @"Only this sentence is selected.\n\nThis is the first sentence of the second paragraph.";

    // Temporary workaround until WritingTools API is updated for nullability.
    WTSession *session = nil;

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        WTContext *context = contexts.firstObject;
        NSString *contextString = context.attributedText.string;
        NSRange contextRange = context.range;

        EXPECT_WK_STREQ(fullText, contextString);
        EXPECT_WK_STREQ(selectedText, [contextString substringWithRange:contextRange]);

        EXPECT_EQ(61UL, contextRange.location);
        EXPECT_EQ(84UL, contextRange.length);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#if ENABLE(WRITING_TOOLS_UI)

TEST(WritingTools, TransparencyMarkersForInlineEditing)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable>Early morning in Cupertino</body>"]);
    [webView focusDocumentBodyAndSelectAll];

    auto waitForValue = [webView](NSUInteger expectedValue) {
        do {
            if ([webView transparentContentMarkerCount:@"document.body.childNodes[0]"] == expectedValue)
                break;

            TestWebKitAPI::Util::runFor(0.1_s);
        } while (true);
    };

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {

        EXPECT_EQ(0U, [webView transparentContentMarkerCount:@"document.body.childNodes[0]"]);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        waitForValue(1U);
        EXPECT_EQ(1U, [webView transparentContentMarkerCount:@"document.body.childNodes[0]"]);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        waitForValue(0U);
        EXPECT_EQ(0U, [webView transparentContentMarkerCount:@"document.body.childNodes[0]"]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, TransparencyMarkersUsingWKWebViewSPI)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<h1 id='title'>Title</h1><p id='content'>Early morning in Cupertino</p>"]);

    auto waitForValue = [webView](NSUInteger expectedValue) {
        do {
            if ([webView transparentContentMarkerCount:@"document.getElementById('content').childNodes[0]"] == expectedValue)
                break;

            TestWebKitAPI::Util::runFor(0.1_s);
        } while (true);
    };

    EXPECT_EQ(0U, [webView transparentContentMarkerCount:@"document.getElementById('content').childNodes[0]"]);

    RetainPtr identifier = [webView _enableTextIndicatorStylingAfterElementWithID:@"title"];
    waitForValue(1U);
    EXPECT_EQ(1U, [webView transparentContentMarkerCount:@"document.getElementById('content').childNodes[0]"]);

    [webView _disableTextIndicatorStylingWithUUID:identifier.get()];
    waitForValue(0U);
    EXPECT_EQ(0U, [webView transparentContentMarkerCount:@"document.getElementById('content').childNodes[0]"]);
}

TEST(WritingTools, NoCrashWhenWebProcessTerminates)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable>Early morning in Cupertino</body>"]);
    [webView focusDocumentBodyAndSelectAll];

    auto waitForValue = [webView](NSUInteger expectedValue) {
        do {
            if ([webView transparentContentMarkerCount:@"document.body.childNodes[0]"] == expectedValue)
                break;

            TestWebKitAPI::Util::runFor(0.1_s);
        } while (true);
    };

    [webView waitForNextPresentationUpdate];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];

        waitForValue(1U);
        [webView _killWebContentProcess];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);

    __block bool webProcessTerminated = false;
    auto navigationDelegate = adoptNS([[TestNavigationDelegate alloc] init]);
    [webView setNavigationDelegate:navigationDelegate.get()];
    [navigationDelegate setWebContentProcessDidTerminate:^(WKWebView *, _WKProcessTerminationReason) {
        webProcessTerminated = true;
    }];

    [webView _killWebContentProcess];
    TestWebKitAPI::Util::run(&webProcessTerminated);
}

#endif

#if PLATFORM(MAC)

static bool didCallScheduleShowAffordanceForSelectionRect = false;

#endif

static void expectScheduleShowAffordanceForSelectionRectCalled(bool expectation)
{
#if PLATFORM(MAC)
    if (expectation)
        TestWebKitAPI::Util::run(&didCallScheduleShowAffordanceForSelectionRect);
    else {
        bool doneWaiting = false;
        RunLoop::main().dispatchAfter(300_ms, [&] {
            EXPECT_FALSE(didCallScheduleShowAffordanceForSelectionRect);
            doneWaiting = true;
        });
        TestWebKitAPI::Util::run(&doneWaiting);
    }

    didCallScheduleShowAffordanceForSelectionRect = false;
#endif
}

TEST(WritingTools, APIWithBehaviorNone)
{
    // If `PlatformWritingToolsBehaviorNone`, there should be no affordance, no context menu item, and no inline editing support.

#if PLATFORM(MAC)
    InstanceMethodSwizzler swizzler(PAL::getWTWritingToolsClass(), @selector(scheduleShowAffordanceForSelectionRect:ofView:forDelegate:), imp_implementationWithBlock(^(id object, NSRect rect, NSView *view, id delegate) {
        didCallScheduleShowAffordanceForSelectionRect = true;
    }));
#endif

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

#if PLATFORM(MAC)
    __block RetainPtr<NSMenu> proposedMenu;
    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        proposedMenu = menu;
        completion(nil);
        gotProposedMenu = true;
    }];
#endif

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>" writingToolsBehavior:PlatformWritingToolsBehaviorNone]);
    [webView setUIDelegate:delegate.get()];

    [webView focusDocumentBodyAndSelectAll];

    expectScheduleShowAffordanceForSelectionRectCalled(false);

    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorNone);

#if PLATFORM(MAC)
    [webView mouseDownAtPoint:NSMakePoint(10, 10) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(10, 10) withFlags:0 eventType:NSEventTypeRightMouseUp];
    TestWebKitAPI::Util::run(&gotProposedMenu);

    NSMenuItem *writingToolsMenuItem = [proposedMenu itemWithIdentifier:_WKMenuItemIdentifierWritingTools];
    EXPECT_NULL(writingToolsMenuItem);
#endif
}

TEST(WritingTools, APIWithBehaviorDefault)
{
    // If `PlatformWritingToolsBehaviorDefault` (or `Limited`), there should be a context menu item, but no affordance nor inline editing support.

#if PLATFORM(MAC)
    InstanceMethodSwizzler swizzler(PAL::getWTWritingToolsClass(), @selector(scheduleShowAffordanceForSelectionRect:ofView:forDelegate:), imp_implementationWithBlock(^(id object, NSRect rect, NSView *view, id delegate) {
        didCallScheduleShowAffordanceForSelectionRect = true;
    }));
#endif

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

#if PLATFORM(MAC)
    __block RetainPtr<NSMenu> proposedMenu;
    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        proposedMenu = menu;
        completion(nil);
        gotProposedMenu = true;
    }];
#endif

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>" writingToolsBehavior:PlatformWritingToolsBehaviorDefault]);
    [webView setUIDelegate:delegate.get()];

    [webView focusDocumentBodyAndSelectAll];

    expectScheduleShowAffordanceForSelectionRectCalled(false);

    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorLimited);

#if PLATFORM(MAC)
    [webView mouseDownAtPoint:NSMakePoint(10, 10) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(10, 10) withFlags:0 eventType:NSEventTypeRightMouseUp];
    TestWebKitAPI::Util::run(&gotProposedMenu);

    NSMenuItem *writingToolsMenuItem = [proposedMenu itemWithIdentifier:_WKMenuItemIdentifierWritingTools];
    EXPECT_NOT_NULL(writingToolsMenuItem);
#endif
}

TEST(WritingTools, APIWithBehaviorComplete)
{
    // If `PlatformWritingToolsBehaviorComplete`, there should be a context menu item, an affordance, and inline editing support.

#if PLATFORM(MAC)
    InstanceMethodSwizzler swizzler(PAL::getWTWritingToolsClass(), @selector(scheduleShowAffordanceForSelectionRect:ofView:forDelegate:), imp_implementationWithBlock(^(id object, NSRect rect, NSView *view, id delegate) {
        didCallScheduleShowAffordanceForSelectionRect = true;
    }));
#endif

    auto delegate = adoptNS([[TestUIDelegate alloc] init]);

#if PLATFORM(MAC)
    __block RetainPtr<NSMenu> proposedMenu;
    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        proposedMenu = menu;
        completion(nil);
        gotProposedMenu = true;
    }];
#endif

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete]);
    [webView setUIDelegate:delegate.get()];

    [webView focusDocumentBodyAndSelectAll];

    expectScheduleShowAffordanceForSelectionRectCalled(true);

    [webView waitForNextPresentationUpdate];

    EXPECT_EQ([webView writingToolsBehaviorForTesting], PlatformWritingToolsBehaviorComplete);

#if PLATFORM(MAC)
    [webView mouseDownAtPoint:NSMakePoint(10, 10) simulatePressure:NO withFlags:0 eventType:NSEventTypeRightMouseDown];
    [webView mouseUpAtPoint:NSMakePoint(10, 10) withFlags:0 eventType:NSEventTypeRightMouseUp];
    TestWebKitAPI::Util::run(&gotProposedMenu);

    NSMenuItem *writingToolsMenuItem = [proposedMenu itemWithIdentifier:_WKMenuItemIdentifierWritingTools];
    EXPECT_NOT_NULL(writingToolsMenuItem);
#endif
}

@interface IsWritingToolsAvailableKVOWrapper : NSObject

- (instancetype)initWithObservable:(NSObject *)observable keyPath:(NSString *)keyPath callback:(Function<void()>&&)callback;

@end

@implementation IsWritingToolsAvailableKVOWrapper {
    RetainPtr<NSObject> _observable;
    RetainPtr<NSString> _keyPath;
    Function<void()> _callback;
}

- (instancetype)initWithObservable:(NSObject *)observable keyPath:(NSString *)keyPath callback:(Function<void()>&&)callback
{
    if (!(self = [super init]))
        return nil;

    _observable = observable;
    _keyPath = keyPath;
    _callback = WTFMove(callback);

    [_observable addObserver:self forKeyPath:_keyPath.get() options:0 context:nil];

    return self;
}

- (void)dealloc
{
    [_observable removeObserver:self forKeyPath:_keyPath.get() context:nullptr];

    [super dealloc];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    _callback();
}

@end

static void waitForIsWritingToolsActiveToChange(TestWKWebView *webView, Function<void()>&& trigger)
{
    bool done = false;
    auto isWritingToolsActiveObserver = adoptNS([[IsWritingToolsAvailableKVOWrapper alloc] initWithObservable:webView keyPath:@"writingToolsActive" callback:[&] {
        done = true;
    }]);

    if (trigger)
        trigger();

    TestWebKitAPI::Util::run(&done);
}

TEST(WritingTools, IsWritingToolsActiveAPI)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    EXPECT_FALSE([webView isWritingToolsActive]);

    waitForIsWritingToolsActiveToChange(webView.get(), [&] {
        [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) { }];
    });

    EXPECT_TRUE([webView isWritingToolsActive]);

    waitForIsWritingToolsActiveToChange(webView.get(), [&] {
        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];
    });

    EXPECT_FALSE([webView isWritingToolsActive]);
}

TEST(WritingTools, IsWritingToolsActiveAPIWithNoInlineEditing)
{
    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    EXPECT_FALSE([webView isWritingToolsActive]);

    __block bool finished = false;

    // Temporary workaround until WritingTools API is updated for nullability.
    WTSession *session = nil;

    [[webView writingToolsDelegate] willBeginWritingToolsSession:session requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_FALSE([webView isWritingToolsActive]);
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#if PLATFORM(MAC)
TEST(WritingTools, ShowAffordance)
{
    InstanceMethodSwizzler swizzler(PAL::getWTWritingToolsClass(), @selector(scheduleShowAffordanceForSelectionRect:ofView:forDelegate:), imp_implementationWithBlock(^(id object, NSRect rect, NSView *view, id delegate) {

        didCallScheduleShowAffordanceForSelectionRect = true;
    }));

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body id='p' contenteditable><p id='first'>AAAA BBBB CCCC</p></body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete]);
    [webView focusDocumentBodyAndSelectAll];

    expectScheduleShowAffordanceForSelectionRectCalled(true);

    [webView waitForNextPresentationUpdate];

    __auto_type clearSelection = ^{
        NSString *clearSelectionJavascript = @""
            "(() => {"
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "})();";

        [webView stringByEvaluatingJavaScript:clearSelectionJavascript];
        [webView waitForNextPresentationUpdate];
    };

    clearSelection();

    expectScheduleShowAffordanceForSelectionRectCalled(true);
}

TEST(WritingTools, ShowAffordanceForMultipleLines)
{
    static const Vector<WebCore::IntRect> expectedRects {
        { { 0, 0 }, { 0, 0 } },
        { { 8, 8 }, { 139, 52 } }
    };

    __block int count = 0;

    InstanceMethodSwizzler swizzler(PAL::getWTWritingToolsClass(), @selector(scheduleShowAffordanceForSelectionRect:ofView:forDelegate:), imp_implementationWithBlock(^(id object, NSRect rect, NSView *view, id delegate) {

        auto actualRect = WebCore::IntRect { rect };
        auto expectedRect = expectedRects[count];

        EXPECT_EQ(actualRect, expectedRect);

        didCallScheduleShowAffordanceForSelectionRect = true;
        count++;
    }));

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='first'>AAAA BBBB CCCC</p><p>DDDD</p></body>" writingToolsBehavior:PlatformWritingToolsBehaviorComplete]);
    [webView focusDocumentBodyAndSelectAll];

    expectScheduleShowAffordanceForSelectionRectCalled(true);
}
#endif

TEST(WritingTools, SmartRepliesMatchStyle)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable style='font-size: 30px;'><p id='p'></p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *setSelectionJavaScript = @""
        "(() => {"
        "  const range = document.createRange();"
        "  range.setStart(document.getElementById('p'), 0);"
        "  range.setEnd(document.getElementById('p'), 0);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";
    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        RetainPtr attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"A"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 0) inContext:contexts.firstObject finished:YES];
        [webView waitForNextPresentationUpdate];

        EXPECT_TRUE([webView stringByEvaluatingJavaScript:@"!document.body.innerHTML.includes('font-size: 12px')"].boolValue);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ContextRangeFromCaretSelection)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable id='p'><p>AAAA BBBB CCCC</p><p>XXXX YYYY ZZZZ</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *setSelectionJavaScript = @""
        "(() => {"
        "  const first = document.getElementById('p').childNodes[0].firstChild;"
        "  const range = document.createRange();"
        "  range.setStart(first, 3);"
        "  range.setEnd(first, 3);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";

    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC\n\nXXXX YYYY ZZZZ", contexts.firstObject.attributedText.string);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        RetainPtr attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"DD"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 5) inContext:contexts.firstObject finished:YES];

        [webView waitForSelectionValue:@"DD"];
        auto selectionAfterDidReceiveText = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"DD", selectionAfterDidReceiveText);

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        auto selectionAfterEnd = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"DD", selectionAfterEnd);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, ContextRangeFromRangeSelection)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable id='p'><p>AAAA BBBB CCCC</p><p>XXXX YYYY ZZZZ</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *setSelectionJavaScript = @""
        "(() => {"
        "  const first = document.getElementById('p').childNodes[0].firstChild;"
        "  const range = document.createRange();"
        "  range.setStart(first, 5);"
        "  range.setEnd(first, 9);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";

    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA BBBB CCCC", contexts.firstObject.attributedText.string);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        RetainPtr attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"DD"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 5) inContext:contexts.firstObject finished:YES];

        [webView waitForSelectionValue:@"DD"];
        auto selectionAfterDidReceiveText = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"DD", selectionAfterDidReceiveText);

        // Simulate clicking/tapping away to end the session.
        NSString *setSelectionToCaret = @""
            "(() => {"
            "  const first = document.getElementById('p').childNodes[0].firstChild;"
            "  const range = document.createRange();"
            "  range.setStart(first, 0);"
            "  range.setEnd(first, 0);"
            "  "
            "  var selection = window.getSelection();"
            "  selection.removeAllRanges();"
            "  selection.addRange(range);"
            "})();";
        [webView stringByEvaluatingJavaScript:setSelectionToCaret];
        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        auto selectionAfterEnd = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"", selectionAfterEnd);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, SuggestedTextIsSelectedAfterSmartReply)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);
    [session setCompositionSessionType:WTCompositionSessionTypeSmartReply];

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p id='p'>AAAA</p><p>BBBB</p></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    NSString *setSelectionJavaScript = @""
        "(() => {"
        "  const range = document.createRange();"
        "  range.setStart(document.getElementById('p'), 0);"
        "  range.setEnd(document.getElementById('p'), 0);"
        "  "
        "  var selection = window.getSelection();"
        "  selection.removeAllRanges();"
        "  selection.addRange(range);"
        "})();";
    [webView stringByEvaluatingJavaScript:setSelectionJavaScript];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        [[webView writingToolsDelegate] writingToolsSession:session.get() didReceiveAction:WTActionCompositionRestart];
        TestWebKitAPI::Util::runFor(0.1_s);

        EXPECT_WK_STREQ(@"", contexts.firstObject.attributedText.string);

        RetainPtr attributedText = adoptNS([[NSAttributedString alloc] initWithString:@"Z"]);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 0) inContext:contexts.firstObject finished:NO];
        TestWebKitAPI::Util::runFor(0.1_s);

        [[webView writingToolsDelegate] compositionSession:session.get() didReceiveText:attributedText.get() replacementRange:NSMakeRange(0, 0) inContext:contexts.firstObject finished:YES];

        [webView waitForSelectionValue:@"Z"];

        [[webView writingToolsDelegate] didEndWritingToolsSession:session.get() accepted:YES];

        auto selectionAfterEnd = [webView stringByEvaluatingJavaScript:@"window.getSelection().toString()"];
        EXPECT_WK_STREQ(@"Z", selectionAfterEnd);

        EXPECT_WK_STREQ(@"ZAAAA\n\nBBBB", [webView contentsAsString]);

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

TEST(WritingTools, BlockquoteAndPreContentsAreIgnored)
{
    auto session = adoptNS([[WTSession alloc] initWithType:WTSessionTypeComposition textViewDelegate:nil]);

    auto webView = adoptNS([[WritingToolsWKWebView alloc] initWithHTMLString:@"<body contenteditable><p>AAAA</p><blockquote>BBBB</blockquote><p>CCCC</p><pre>DDDD</pre></body>"]);
    [webView focusDocumentBodyAndSelectAll];

    __block bool finished = false;
    [[webView writingToolsDelegate] willBeginWritingToolsSession:session.get() requestContexts:^(NSArray<WTContext *> *contexts) {
        EXPECT_EQ(1UL, contexts.count);

        EXPECT_WK_STREQ(@"AAAA\n\nBBBB\nCCCC\n\nDDDD", contexts.firstObject.attributedText.string);

        __block size_t i = 0;
        [contexts.firstObject.attributedText enumerateAttribute:WTWritingToolsPreservedAttributeName inRange:NSMakeRange(0, [contexts.firstObject.attributedText length]) options:0 usingBlock:^(id value, NSRange attributeRange, BOOL *stop) {
            switch (i) {
            case 0: // "AAAA"
                EXPECT_NULL(value);
                break;

            case 1: // "BBBB"
                EXPECT_EQ([value integerValue], 1);
                break;

            case 2: // "CCCC"
                EXPECT_NULL(value);
                break;

            case 3: // "DDDD"
                EXPECT_EQ([value integerValue], 1);
                break;

            default:
                ASSERT_NOT_REACHED();
                break;
            }

            ++i;
        }];

        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);
}

#endif
