/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#import "WKContentViewInteraction.h"

#if HAVE(UIFINDINTERACTION)

#import "WKFoundTextRange.h"
#import "WKWebViewIOS.h"
#import "WebFindOptions.h"
#import "WebFoundTextRange.h"
#import "WebPageProxy.h"
#import <WebCore/FloatRect.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

@implementation WKContentView (WKFind)

- (void)find:(id)sender
{
    [self.webView find:sender];
}

- (void)findAndReplace:(id)sender
{
    [self.webView findAndReplace:sender];
}

- (void)findNext:(id)sender
{
    [self.webView findNext:sender];
}

- (void)findPrevious:(id)sender
{
    [self.webView findPrevious:sender];
}

- (void)performTextSearchWithQueryString:(NSString *)string usingOptions:(UITextSearchOptions *)options resultAggregator:(id<UITextSearchAggregator>)aggregator
{
    OptionSet<WebKit::FindOptions> findOptions;
    switch (options.wordMatchMethod) {
    case UITextSearchMatchMethodStartsWith:
        findOptions.add(WebKit::FindOptions::AtWordStarts);
        break;
    case UITextSearchMatchMethodFullWord:
        findOptions.add({ WebKit::FindOptions::AtWordStarts, WebKit::FindOptions::AtWordEnds });
        break;
    default:
        break;
    }

    if (options.stringCompareOptions & NSCaseInsensitiveSearch)
        findOptions.add(WebKit::FindOptions::CaseInsensitive);

    // The limit matches the limit set on existing WKWebView find API.
    constexpr auto maxMatches = 1000;
    protect(_page)->findTextRangesForStringMatches(string, findOptions, maxMatches, [string = retainPtr(string), aggregator = retainPtr(aggregator)](const Vector<WebKit::WebFoundTextRange> ranges) {
        for (auto& range : ranges) {
            RetainPtr textRange = [WKFoundTextRange foundTextRangeWithWebFoundTextRange:range];
            [aggregator foundRange:textRange.get() forSearchString:string.get() inDocument:nil];
        }

        [aggregator finishedSearching];
    });
}

- (void)replaceFoundTextInRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document withText:(NSString *)replacementText
{
    if (!self.supportsTextReplacement)
        return;

    auto foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range);
    if (!foundTextRange)
        return;

    protect(_page)->replaceFoundTextRangeWithString([foundTextRange webFoundTextRange], replacementText);
}

- (void)decorateFoundTextRange:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document usingStyle:(UITextSearchFoundTextStyle)style
{
    auto foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range);
    if (!foundTextRange)
        return;

    auto decorationStyle = WebKit::FindDecorationStyle::Normal;
    if (style == UITextSearchFoundTextStyleFound)
        decorationStyle = WebKit::FindDecorationStyle::Found;
    else if (style == UITextSearchFoundTextStyleHighlighted)
        decorationStyle = WebKit::FindDecorationStyle::Highlighted;

    protect(_page)->decorateTextRangeWithStyle([foundTextRange webFoundTextRange], decorationStyle);
}

- (void)scrollRangeToVisible:(UITextRange *)range inDocument:(UITextSearchDocumentIdentifier)document
{
    if (auto foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range))
        protect(_page)->scrollTextRangeToVisible([foundTextRange webFoundTextRange]);
}

- (void)clearAllDecoratedFoundText
{
    protect(_page)->clearAllDecoratedFoundText();
}

- (void)didBeginTextSearchOperation
{
    [self.webView _showFindOverlay];
    protect(_page)->didBeginTextSearchOperation();
}

- (void)didEndTextSearchOperation
{
    [self.webView _hideFindOverlay];
}

- (BOOL)supportsTextReplacement
{
    return self.webView.supportsTextReplacement;
}

- (BOOL)supportsTextReplacementForWebView
{
    return self.webView._editable;
}

- (NSInteger)offsetFromPosition:(UITextPosition *)from toPosition:(UITextPosition *)toPosition inDocument:(UITextSearchDocumentIdentifier)document
{
    return [self offsetFromPosition:from toPosition:toPosition];
}

- (NSComparisonResult)compareFoundRange:(UITextRange *)fromRange toRange:(UITextRange *)toRange inDocument:(UITextSearchDocumentIdentifier)document
{
    NSInteger offset = [self offsetFromPosition:fromRange.start toPosition:toRange.start];

    if (offset < 0)
        return NSOrderedAscending;

    if (offset > 0)
        return NSOrderedDescending;

    return NSOrderedSame;
}

- (void)requestRectForFoundTextRange:(UITextRange *)range completionHandler:(void (^)(CGRect))completionHandler
{
    if (RetainPtr foundTextRange = dynamic_objc_cast<WKFoundTextRange>(range)) {
        protect(_page)->requestRectForFoundTextRange([foundTextRange webFoundTextRange], [completionHandler = makeBlockPtr(completionHandler)](WebCore::FloatRect rect) {
            completionHandler(rect);
        });
        return;
    }

    completionHandler(CGRectZero);
}

@end

#endif // HAVE(UIFINDINTERACTION)
