/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#import "Document.h"
#import "FocusController.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "HTMLConverter.h"
#import "HitTestResult.h"
#import "LookupSPI.h"
#import "Page.h"
#import "Range.h"
#import "RenderObject.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleSelection.h"
#import "VisibleUnits.h"
#import "WebCoreSystemInterface.h"
#import "htmlediting.h"
#import <wtf/RefPtr.h>

namespace WebCore {

bool isPositionInRange(const VisiblePosition& position, Range* range)
{
    RefPtr<Range> positionRange = makeRange(position, position);

    ExceptionCode ec = 0;
    range->compareBoundaryPoints(Range::START_TO_START, positionRange.get(), ec);
    if (ec)
        return false;

    if (!range->isPointInRange(positionRange->startContainer(), positionRange->startOffset(), ec))
        return false;
    if (ec)
        return false;

    return true;
}

bool shouldUseSelection(const VisiblePosition& position, const VisibleSelection& selection)
{
    if (!selection.isRange())
        return false;

    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return false;

    return isPositionInRange(position, selectedRange.get());
}

PassRefPtr<Range> rangeExpandedAroundPositionByCharacters(const VisiblePosition& position, int numberOfCharactersToExpand)
{
    Position start = position.deepEquivalent();
    Position end = position.deepEquivalent();
    for (int i = 0; i < numberOfCharactersToExpand; ++i) {
        if (directionOfEnclosingBlock(start) == LTR)
            start = start.previous(Character);
        else
            start = start.next(Character);

        if (directionOfEnclosingBlock(end) == LTR)
            end = end.next(Character);
        else
            end = end.previous(Character);
    }

    return makeRange(start, end);
}

PassRefPtr<Range> rangeForDictionaryLookupForSelection(const VisibleSelection& selection, NSDictionary **options)
{
    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return nullptr;

    VisiblePosition selectionStart = selection.visibleStart();
    VisiblePosition selectionEnd = selection.visibleEnd();

    // As context, we are going to use the surrounding paragraphs of text.
    VisiblePosition paragraphStart = startOfParagraph(selectionStart);
    VisiblePosition paragraphEnd = endOfParagraph(selectionEnd);

    int lengthToSelectionStart = TextIterator::rangeLength(makeRange(paragraphStart, selectionStart).get());
    int lengthToSelectionEnd = TextIterator::rangeLength(makeRange(paragraphStart, selectionEnd).get());
    NSRange rangeToPass = NSMakeRange(lengthToSelectionStart, lengthToSelectionEnd - lengthToSelectionStart);

    String fullPlainTextString = plainText(makeRange(paragraphStart, paragraphEnd).get());

    // Since we already have the range we want, we just need to grab the returned options.
    if (Class luLookupDefinitionModule = getLULookupDefinitionModuleClass())
        [luLookupDefinitionModule tokenRangeForString:fullPlainTextString range:rangeToPass options:options];

    return selectedRange.release();
}

PassRefPtr<Range> rangeForDictionaryLookupAtHitTestResult(const HitTestResult& hitTestResult, NSDictionary **options)
{
    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return nullptr;

    auto renderer = node->renderer();
    if (!renderer)
        return nullptr;

    Frame* frame = node->document().frame();
    if (!frame)
        return nullptr;

    // Don't do anything if there is no character at the point.
    if (!frame->rangeForPoint(hitTestResult.roundedPointInInnerNodeFrame()))
        return nullptr;

    VisiblePosition position = renderer->positionForPoint(hitTestResult.localPoint(), nullptr);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    VisibleSelection selection = frame->page()->focusController().focusedOrMainFrame().selection().selection();
    if (shouldUseSelection(position, selection))
        return rangeForDictionaryLookupForSelection(selection, options);

    // As context, we are going to use 250 characters of text before and after the point.
    RefPtr<Range> fullCharacterRange = rangeExpandedAroundPositionByCharacters(position, 250);
    if (!fullCharacterRange)
        return nullptr;

    NSRange rangeToPass = NSMakeRange(TextIterator::rangeLength(makeRange(fullCharacterRange->startPosition(), position).get()), 0);

    String fullPlainTextString = plainText(fullCharacterRange.get());

    NSRange extractedRange = NSMakeRange(rangeToPass.location, 0);
    if (Class luLookupDefinitionModule = getLULookupDefinitionModuleClass())
        extractedRange = [luLookupDefinitionModule tokenRangeForString:fullPlainTextString range:rangeToPass options:options];

    // This function sometimes returns {NSNotFound, 0} if it was unable to determine a good string.
    if (extractedRange.location == NSNotFound)
        return nullptr;

    return TextIterator::subrange(fullCharacterRange.get(), extractedRange.location, extractedRange.length);
}

} // namespace WebCore

#endif // PLATFORM(MAC)

