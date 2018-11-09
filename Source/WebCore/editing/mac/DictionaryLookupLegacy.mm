/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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
#import "DictionaryLookup.h"

#if PLATFORM(MAC) && !ENABLE(REVEAL)

#import "Document.h"
#import "Editing.h"
#import "FocusController.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "HTMLConverter.h"
#import "HitTestResult.h"
#import "Page.h"
#import "Range.h"
#import "RenderObject.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleSelection.h"
#import "VisibleUnits.h"
#import <PDFKit/PDFKit.h>
#import <pal/spi/mac/LookupSPI.h>
#import <pal/spi/mac/NSImmediateActionGestureRecognizerSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RefPtr.h>

SOFT_LINK_CONSTANT_MAY_FAIL(Lookup, LUTermOptionDisableSearchTermIndicator, NSString *)

namespace WebCore {

static NSRange tokenRange(const String& string, NSRange range, NSDictionary **options)
{
    if (!getLULookupDefinitionModuleClass())
        return NSMakeRange(NSNotFound, 0);

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    return [classLULookupDefinitionModule tokenRangeForString:string range:range options:options];

    END_BLOCK_OBJC_EXCEPTIONS;

    return NSMakeRange(NSNotFound, 0);
}

static bool selectionContainsPosition(const VisiblePosition& position, const VisibleSelection& selection)
{
    if (!selection.isRange())
        return false;

    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return false;

    return selectedRange->contains(position);
}

std::tuple<RefPtr<Range>, NSDictionary *> DictionaryLookup::rangeForSelection(const VisibleSelection& selection)
{
    auto selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return { nullptr, nil };

    // Since we already have the range we want, we just need to grab the returned options.
    auto selectionStart = selection.visibleStart();
    auto selectionEnd = selection.visibleEnd();

    // As context, we are going to use the surrounding paragraphs of text.
    auto paragraphStart = startOfParagraph(selectionStart);
    auto paragraphEnd = endOfParagraph(selectionEnd);

    int lengthToSelectionStart = TextIterator::rangeLength(makeRange(paragraphStart, selectionStart).get());
    int lengthToSelectionEnd = TextIterator::rangeLength(makeRange(paragraphStart, selectionEnd).get());
    NSRange rangeToPass = NSMakeRange(lengthToSelectionStart, lengthToSelectionEnd - lengthToSelectionStart);

    NSDictionary *options = nil;
    tokenRange(plainText(makeRange(paragraphStart, paragraphEnd).get()), rangeToPass, &options);

    return { selectedRange, options };
}

std::tuple<RefPtr<Range>, NSDictionary *> DictionaryLookup::rangeAtHitTestResult(const HitTestResult& hitTestResult)
{
    auto* node = hitTestResult.innerNonSharedNode();
    if (!node || !node->renderer())
        return { nullptr, nil };

    auto* frame = node->document().frame();
    if (!frame)
        return { nullptr, nil };

    // Don't do anything if there is no character at the point.
    auto framePoint = hitTestResult.roundedPointInInnerNodeFrame();
    if (!frame->rangeForPoint(framePoint))
        return { nullptr, nil };

    auto position = frame->visiblePositionForPoint(framePoint);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    // If we hit the selection, use that instead of letting Lookup decide the range.
    auto selection = frame->page()->focusController().focusedOrMainFrame().selection().selection();
    if (selectionContainsPosition(position, selection))
        return rangeForSelection(selection);

    VisibleSelection selectionAccountingForLineRules { position };
    selectionAccountingForLineRules.expandUsingGranularity(WordGranularity);
    position = selectionAccountingForLineRules.start();

    // As context, we are going to use 250 characters of text before and after the point.
    auto fullCharacterRange = rangeExpandedAroundPositionByCharacters(position, 250);
    if (!fullCharacterRange)
        return { nullptr, nil };

    NSRange rangeToPass = NSMakeRange(TextIterator::rangeLength(makeRange(fullCharacterRange->startPosition(), position).get()), 0);
    NSDictionary *options = nil;
    NSRange extractedRange = tokenRange(plainText(fullCharacterRange.get()), rangeToPass, &options);

    // tokenRange sometimes returns {NSNotFound, 0} if it was unable to determine a good string.
    // FIXME (159063): We shouldn't need to check for zero length here.
    if (extractedRange.location == NSNotFound || !extractedRange.length)
        return { nullptr, nil };

    return { TextIterator::subrange(*fullCharacterRange, extractedRange.location, extractedRange.length), options };
}

static void expandSelectionByCharacters(PDFSelection *selection, NSInteger numberOfCharactersToExpand, NSInteger& charactersAddedBeforeStart, NSInteger& charactersAddedAfterEnd)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    size_t originalLength = selection.string.length;
    [selection extendSelectionAtStart:numberOfCharactersToExpand];
    
    charactersAddedBeforeStart = selection.string.length - originalLength;
    
    [selection extendSelectionAtEnd:numberOfCharactersToExpand];
    charactersAddedAfterEnd = selection.string.length - originalLength - charactersAddedBeforeStart;

    END_BLOCK_OBJC_EXCEPTIONS;
}

std::tuple<NSString *, NSDictionary *> DictionaryLookup::stringForPDFSelection(PDFSelection *selection)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // Don't do anything if there is no character at the point.
    if (!selection || !selection.string.length)
        return { @"", nil };

    RetainPtr<PDFSelection> selectionForLookup = adoptNS([selection copy]);

    // As context, we are going to use 250 characters of text before and after the point.
    auto originalLength = [selectionForLookup string].length;
    NSInteger charactersAddedBeforeStart = 0;
    NSInteger charactersAddedAfterEnd = 0;
    expandSelectionByCharacters(selectionForLookup.get(), 250, charactersAddedBeforeStart, charactersAddedAfterEnd);

    auto fullPlainTextString = [selectionForLookup string];
    auto rangeToPass = NSMakeRange(charactersAddedBeforeStart, 0);

    NSDictionary *options = nil;
    auto extractedRange = tokenRange(fullPlainTextString, rangeToPass, &options);

    // This function sometimes returns {NSNotFound, 0} if it was unable to determine a good string.
    if (extractedRange.location == NSNotFound)
        return { selection.string, options };

    NSInteger lookupAddedBefore = rangeToPass.location - extractedRange.location;
    NSInteger lookupAddedAfter = (extractedRange.location + extractedRange.length) - (rangeToPass.location + originalLength);

    [selection extendSelectionAtStart:lookupAddedBefore];
    [selection extendSelectionAtEnd:lookupAddedAfter];

    ASSERT([selection.string isEqualToString:[fullPlainTextString substringWithRange:extractedRange]]);
    return { selection.string, options };

    END_BLOCK_OBJC_EXCEPTIONS;

    return { @"", nil };
}

static id <NSImmediateActionAnimationController> showPopupOrCreateAnimationController(bool createAnimationController, const DictionaryPopupInfo& dictionaryPopupInfo, NSView *view, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (!getLULookupDefinitionModuleClass())
        return nil;

    RetainPtr<NSMutableDictionary> mutableOptions = adoptNS([[NSMutableDictionary alloc] init]);
    if (NSDictionary *options = dictionaryPopupInfo.options.get())
        [mutableOptions addEntriesFromDictionary:options];

    auto textIndicator = TextIndicator::create(dictionaryPopupInfo.textIndicator);

    if (canLoadLUTermOptionDisableSearchTermIndicator() && textIndicator.get().contentImage()) {
        textIndicatorInstallationCallback(textIndicator.get());
        [mutableOptions setObject:@YES forKey:getLUTermOptionDisableSearchTermIndicator()];

        FloatRect firstTextRectInViewCoordinates = textIndicator.get().textRectsInBoundingRectCoordinates()[0];
        FloatRect textBoundingRectInViewCoordinates = textIndicator.get().textBoundingRectInRootViewCoordinates();
        if (rootViewToViewConversionCallback)
            textBoundingRectInViewCoordinates = rootViewToViewConversionCallback(textBoundingRectInViewCoordinates);
        firstTextRectInViewCoordinates.moveBy(textBoundingRectInViewCoordinates.location());
        if (createAnimationController)
            return [getLULookupDefinitionModuleClass() lookupAnimationControllerForTerm:dictionaryPopupInfo.attributedString.get() relativeToRect:firstTextRectInViewCoordinates ofView:view options:mutableOptions.get()];

        [getLULookupDefinitionModuleClass() showDefinitionForTerm:dictionaryPopupInfo.attributedString.get() relativeToRect:firstTextRectInViewCoordinates ofView:view options:mutableOptions.get()];
        return nil;
    }

    NSPoint textBaselineOrigin = dictionaryPopupInfo.origin;

    // Convert to screen coordinates.
    textBaselineOrigin = [view convertPoint:textBaselineOrigin toView:nil];
    textBaselineOrigin = [view.window convertRectToScreen:NSMakeRect(textBaselineOrigin.x, textBaselineOrigin.y, 0, 0)].origin;

    if (createAnimationController)
        return [getLULookupDefinitionModuleClass() lookupAnimationControllerForTerm:dictionaryPopupInfo.attributedString.get() atLocation:textBaselineOrigin options:mutableOptions.get()];

    [getLULookupDefinitionModuleClass() showDefinitionForTerm:dictionaryPopupInfo.attributedString.get() atLocation:textBaselineOrigin options:mutableOptions.get()];
    return nil;

    END_BLOCK_OBJC_EXCEPTIONS;
    return nil;
}

void DictionaryLookup::showPopup(const DictionaryPopupInfo& dictionaryPopupInfo, NSView *view, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    UNUSED_PARAM(clearTextIndicator);
    
    showPopupOrCreateAnimationController(false, dictionaryPopupInfo, view, textIndicatorInstallationCallback, rootViewToViewConversionCallback);
}

void DictionaryLookup::hidePopup()
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (!getLULookupDefinitionModuleClass())
        return;
    [getLULookupDefinitionModuleClass() hideDefinition];

    END_BLOCK_OBJC_EXCEPTIONS;
}

id <NSImmediateActionAnimationController> DictionaryLookup::animationControllerForPopup(const DictionaryPopupInfo& dictionaryPopupInfo, NSView *view, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    UNUSED_PARAM(clearTextIndicator);
    
    return showPopupOrCreateAnimationController(true, dictionaryPopupInfo, view, textIndicatorInstallationCallback, rootViewToViewConversionCallback);
}

} // namespace WebCore

#endif // PLATFORM(MAC) && !ENABLE(REVEAL)
