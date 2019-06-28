/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "TextCheckingControllerProxy.h"

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)

#import "ArgumentCoders.h"
#import "TextCheckingControllerProxyMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/DocumentMarker.h>
#import <WebCore/DocumentMarkerController.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/FocusController.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderedDocumentMarker.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>

// FIXME: Remove this after rdar://problem/48914153 is resolved.
#if PLATFORM(MACCATALYST)
typedef NS_ENUM(NSInteger, NSSpellingState) {
    NSSpellingStateSpellingFlag = (1 << 0),
    NSSpellingStateGrammarFlag = (1 << 1)
};
#endif

namespace WebKit {
using namespace WebCore;

TextCheckingControllerProxy::TextCheckingControllerProxy(WebPage& page)
    : m_page(page)
{
    WebProcess::singleton().addMessageReceiver(Messages::TextCheckingControllerProxy::messageReceiverName(), m_page.pageID(), *this);
}

TextCheckingControllerProxy::~TextCheckingControllerProxy()
{
    WebProcess::singleton().removeMessageReceiver(Messages::TextCheckingControllerProxy::messageReceiverName(), m_page.pageID());
}

static OptionSet<DocumentMarker::MarkerType> relevantMarkerTypes()
{
    return { DocumentMarker::PlatformTextChecking, DocumentMarker::Spelling, DocumentMarker::Grammar };
}

Optional<TextCheckingControllerProxy::RangeAndOffset> TextCheckingControllerProxy::rangeAndOffsetRelativeToSelection(int64_t offset, uint64_t length)
{
    Frame& frame = m_page.corePage()->focusController().focusedOrMainFrame();
    const FrameSelection& frameSelection = frame.selection();
    const VisibleSelection& selection = frameSelection.selection();
    if (selection.isNone())
        return WTF::nullopt;

    auto root = frameSelection.rootEditableElementOrDocumentElement();
    auto range = selection.toNormalizedRange();
    range->collapse(true);

    size_t selectionLocation;
    size_t selectionLength;
    TextIterator::getLocationAndLengthFromRange(root, range.get(), selectionLocation, selectionLength);
    selectionLocation += offset;

    return {{ TextIterator::rangeFromLocationAndLength(root, selectionLocation, length), selectionLocation }};
}

void TextCheckingControllerProxy::replaceRelativeToSelection(AttributedString annotatedString, int64_t selectionOffset, uint64_t length, uint64_t relativeReplacementLocation, uint64_t relativeReplacementLength)
{
    Frame& frame = m_page.corePage()->focusController().focusedOrMainFrame();
    FrameSelection& frameSelection = frame.selection();
    auto root = frameSelection.rootEditableElementOrDocumentElement();

    auto rangeAndOffset = rangeAndOffsetRelativeToSelection(selectionOffset, length);
    if (!rangeAndOffset)
        return;
    auto range = rangeAndOffset->range;
    if (!range)
        return;
    auto locationInRoot = rangeAndOffset->locationInRoot;

    auto& markers = frame.document()->markers();
    markers.removeMarkers(*range, relevantMarkerTypes());

    if (relativeReplacementLocation != NSNotFound) {
        auto rangeAndOffsetOfReplacement = rangeAndOffsetRelativeToSelection(selectionOffset + relativeReplacementLocation, relativeReplacementLength);
        if (rangeAndOffsetOfReplacement) {
            auto replacementRange = rangeAndOffsetOfReplacement->range;
            if (replacementRange) {
                bool restoreSelection = frameSelection.selection().isRange();
                frame.editor().replaceRangeForSpellChecking(*replacementRange, [[annotatedString.string string] substringWithRange:NSMakeRange(relativeReplacementLocation, relativeReplacementLength)]);

                size_t selectionLocationToRestore = locationInRoot - selectionOffset;
                if (restoreSelection && selectionLocationToRestore > locationInRoot + relativeReplacementLocation + relativeReplacementLength) {
                    auto selectionToRestore = TextIterator::rangeFromLocationAndLength(root, selectionLocationToRestore, 0);
                    frameSelection.moveTo(selectionToRestore.get());
                }
            }
        }
    }

    [annotatedString.string enumerateAttributesInRange:NSMakeRange(0, [annotatedString.string length]) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attrs, NSRange attributeRange, BOOL *stop) {
        auto attributeCoreRange = TextIterator::rangeFromLocationAndLength(root, locationInRoot + attributeRange.location, attributeRange.length);
        if (!attributeCoreRange)
            return;

        [attrs enumerateKeysAndObjectsUsingBlock:^(NSAttributedStringKey key, id value, BOOL *stop) {
            if (![value isKindOfClass:[NSString class]])
                return;
            markers.addPlatformTextCheckingMarker(*attributeCoreRange, key, (NSString *)value);

            // FIXME: Switch to constants after rdar://problem/48914153 is resolved.
            if ([key isEqualToString:@"NSSpellingState"]) {
                NSSpellingState spellingState = (NSSpellingState)[value integerValue];
                if (spellingState & NSSpellingStateSpellingFlag)
                    markers.addMarker(*attributeCoreRange, DocumentMarker::Spelling);
                if (spellingState & NSSpellingStateGrammarFlag) {
                    NSString *userDescription = [attrs objectForKey:@"NSGrammarUserDescription"];
                    markers.addMarker(*attributeCoreRange, DocumentMarker::Grammar, userDescription);
                }
            }
        }];
    }];
}

void TextCheckingControllerProxy::removeAnnotationRelativeToSelection(String annotation, int64_t selectionOffset, uint64_t length)
{
    Frame& frame = m_page.corePage()->focusController().focusedOrMainFrame();
    auto rangeAndOffset = rangeAndOffsetRelativeToSelection(selectionOffset, length);
    if (!rangeAndOffset)
        return;
    auto range = rangeAndOffset->range;
    if (!range)
        return;

    bool removeCoreSpellingMarkers = (annotation == String(@"NSSpellingState"));
    frame.document()->markers().filterMarkers(*range, [&] (DocumentMarker* marker) {
        if (!WTF::holds_alternative<DocumentMarker::PlatformTextCheckingData>(marker->data()))
            return false;
        auto& textCheckingData = WTF::get<DocumentMarker::PlatformTextCheckingData>(marker->data());
        return textCheckingData.key != annotation;
    }, removeCoreSpellingMarkers ? relevantMarkerTypes() : DocumentMarker::PlatformTextChecking);
}

AttributedString TextCheckingControllerProxy::annotatedSubstringBetweenPositions(const WebCore::VisiblePosition& start, const WebCore::VisiblePosition& end)
{
    RetainPtr<NSMutableAttributedString> string = adoptNS([[NSMutableAttributedString alloc] init]);
    NSUInteger stringLength = 0;

    RefPtr<Document> document = start.deepEquivalent().document();
    if (!document)
        return { };

    auto entireRange = makeRange(start, end);
    if (!entireRange)
        return { };

    RefPtr<Node> commonAncestor = entireRange->commonAncestorContainer();
    size_t entireRangeLocation;
    size_t entireRangeLength;
    TextIterator::getLocationAndLengthFromRange(commonAncestor.get(), entireRange.get(), entireRangeLocation, entireRangeLength);

    for (TextIterator it(start.deepEquivalent(), end.deepEquivalent()); !it.atEnd(); it.advance()) {
        int currentTextLength = it.text().length();
        if (!currentTextLength)
            continue;

        [string appendAttributedString:[[[NSAttributedString alloc] initWithString:it.text().createNSStringWithoutCopying().get()] autorelease]];

        RefPtr<Range> currentTextRange = it.range();
        auto markers = document->markers().markersInRange(*currentTextRange, DocumentMarker::PlatformTextChecking);
        for (const auto* marker : markers) {
            if (!WTF::holds_alternative<DocumentMarker::PlatformTextCheckingData>(marker->data()))
                continue;

            auto& textCheckingData = WTF::get<DocumentMarker::PlatformTextCheckingData>(marker->data());
            auto subrange = TextIterator::subrange(*currentTextRange, marker->startOffset(), marker->endOffset() - marker->startOffset());

            size_t subrangeLocation;
            size_t subrangeLength;
            TextIterator::getLocationAndLengthFromRange(commonAncestor.get(), &subrange.get(), subrangeLocation, subrangeLength);

            ASSERT(subrangeLocation > entireRangeLocation);
            ASSERT(subrangeLocation + subrangeLength < entireRangeLength);
            [string addAttribute:textCheckingData.key value:textCheckingData.value range:NSMakeRange(subrangeLocation - entireRangeLocation, subrangeLength)];
        }

        stringLength += currentTextLength;
    }

    return string.autorelease();
}

} // namespace WebKit

#endif // ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
