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

#import "config.h"

#if PLATFORM(MAC)

#import "Helpers/PlatformUtilities.h"
#import "Helpers/Utilities.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import "InstanceMethodSwizzler.h"
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <pal/spi/mac/NSSpellCheckerSPI.h>
#import <wtf/RetainPtr.h>

namespace TestWebKitAPI {

static NSString * const testText = @"Let's go in then store\n";
static NSString * const grammarCorrection = @"in the";

static bool didShowCorrectionIndicator = false;
static bool didReturnGrammarMarker = false;
static RetainPtr<NSString> capturedPrimaryString;

static NSArray<NSTextCheckingResult *> *swizzledCheckString(id, SEL, NSString *stringToCheck, NSRange, NSTextCheckingTypes types, NSDictionary *, NSInteger, NSOrthography **, NSInteger *)
{
    NSRange phraseRange = [stringToCheck rangeOfString:@"go in then"];
    if (phraseRange.location == NSNotFound)
        return @[ ];

    RetainPtr results = adoptNS([[NSMutableArray alloc] init]);

    if (types & NSTextCheckingTypeSpelling) {
        // Same end offset as grammar but smaller start, so it sorts first in the marker list.
        [results addObject:[NSTextCheckingResult spellCheckingResultWithRange:phraseRange]];
    }

    if (types & NSTextCheckingTypeGrammar) {
        NSRange grammarRange = NSMakeRange(phraseRange.location + 3, phraseRange.length - 3);
        NSDictionary *detail = @{
            NSGrammarRange: [NSValue valueWithRange:NSMakeRange(0, grammarRange.length)],
            NSGrammarCorrections: @[ grammarCorrection ],
        };
        [results addObject:[NSTextCheckingResult grammarCheckingResultWithRange:grammarRange details:@[ detail ]]];
        didReturnGrammarMarker = true;
    }

    return results.autorelease();
}

using CorrectionIndicatorCompletionHandler = void (^)(NSString *);

static void swizzledShowCorrectionIndicator(id, SEL, NSCorrectionIndicatorType, NSString *primaryString, NSArray<NSString *> *, NSRect, NSView *, CorrectionIndicatorCompletionHandler completionHandler)
{
    capturedPrimaryString = primaryString;
    didShowCorrectionIndicator = true;
    if (completionHandler)
        completionHandler(nil);
}

TEST(GrammarMarkerPrecedence, GrammarTakesPrecedenceOverOverlappingSpellingMarker)
{
    didShowCorrectionIndicator = false;
    didReturnGrammarMarker = false;
    capturedPrimaryString = nil;

    InstanceMethodSwizzler checkStringSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(checkString:range:types:options:inSpellDocumentWithTag:orthography:wordCount:),
        reinterpret_cast<IMP>(swizzledCheckString)
    };

    InstanceMethodSwizzler showIndicatorSwizzler {
        NSSpellChecker.sharedSpellChecker.class,
        @selector(showCorrectionIndicatorOfType:primaryString:alternativeStrings:forStringInRect:view:completionHandler:),
        reinterpret_cast<IMP>(swizzledShowCorrectionIndicator)
    };

    RetainPtr webView = adoptNS([[TestWKWebView<NSTextInputClient> alloc] initWithFrame:NSMakeRect(0, 0, 800, 200)]);
    [webView synchronouslyLoadHTMLString:@"<body contenteditable style='font-family: monospace; font-size: 24px;'>"];
    [webView _setContinuousSpellCheckingEnabledForTesting:YES];
    [webView _setGrammarCheckingEnabledForTesting:YES];
    [webView objectByEvaluatingJavaScript:@"getSelection().setPosition(document.body)"];
    [webView insertText:testText replacementRange:NSMakeRange(0, 0)];
    [webView waitForNextPresentationUpdate];

    EXPECT_TRUE(Util::runFor(&didReturnGrammarMarker, 2_s));

    // Establish an old selection in a different word so respondToChangedSelection fires below.
    [webView objectByEvaluatingJavaScript:@"(() => { const r = document.createRange(); r.setStart(document.body.firstChild, 19); r.collapse(true); const s = getSelection(); s.removeAllRanges(); s.addRange(r); })()"];
    [webView waitForNextPresentationUpdate];

    // End-of-word for the caret in "then" is offset 16, matching both markers' endOffsets.
    [webView objectByEvaluatingJavaScript:@"(() => { const r = document.createRange(); r.setStart(document.body.firstChild, 14); r.collapse(true); const s = getSelection(); s.removeAllRanges(); s.addRange(r); })()"];

    Util::runFor(&didShowCorrectionIndicator, 3_s);

    EXPECT_TRUE(didShowCorrectionIndicator);
    EXPECT_WK_STREQ(grammarCorrection, capturedPrimaryString.get());
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC)
