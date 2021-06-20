/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MarkedText.h"

#include "Document.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "ElementRuleCollector.h"
#include "HighlightData.h"
#include "HighlightRegister.h"
#include "RenderBoxModelObject.h"
#include "RenderText.h"
#include "RenderedDocumentMarker.h"
#include "RuntimeEnabledFeatures.h"
#include "TextBoxSelectableRange.h"
#include <algorithm>
#include <wtf/HashSet.h>

namespace WebCore {

Vector<MarkedText> MarkedText::subdivide(const Vector<MarkedText>& markedTexts, OverlapStrategy overlapStrategy)
{
    if (markedTexts.isEmpty())
        return { };

    struct Offset {
        enum Kind { Begin, End };
        Kind kind;
        unsigned value; // Copy of markedText.startOffset/endOffset to avoid the need to branch based on kind.
        const MarkedText* markedText;
    };

    // 1. Build table of all offsets.
    Vector<Offset> offsets;
    ASSERT(markedTexts.size() < std::numeric_limits<unsigned>::max() / 2);
    unsigned numberOfMarkedTexts = markedTexts.size();
    unsigned numberOfOffsets = 2 * numberOfMarkedTexts;
    offsets.reserveInitialCapacity(numberOfOffsets);
    for (auto& markedText : markedTexts) {
        offsets.uncheckedAppend({ Offset::Begin, markedText.startOffset, &markedText });
        offsets.uncheckedAppend({ Offset::End, markedText.endOffset, &markedText });
    }

    // 2. Sort offsets such that begin offsets are in paint order and end offsets are in reverse paint order.
    std::sort(offsets.begin(), offsets.end(), [] (const Offset& a, const Offset& b) {
        return a.value < b.value || (a.value == b.value && a.kind == b.kind && a.kind == Offset::Begin && a.markedText->type < b.markedText->type)
        || (a.value == b.value && a.kind == b.kind && a.kind == Offset::End && a.markedText->type > b.markedText->type);
    });

    // 3. Compute intersection.
    Vector<MarkedText> result;
    result.reserveInitialCapacity(numberOfMarkedTexts);
    HashSet<const MarkedText*> processedMarkedTexts;
    unsigned offsetSoFar = offsets[0].value;
    for (unsigned i = 1; i < numberOfOffsets; ++i) {
        if (offsets[i].value > offsets[i - 1].value) {
            if (overlapStrategy == OverlapStrategy::Frontmost) {
                std::optional<unsigned> frontmost;
                for (unsigned j = 0; j < i; ++j) {
                    if (!processedMarkedTexts.contains(offsets[j].markedText) && (!frontmost || offsets[j].markedText->type > offsets[*frontmost].markedText->type))
                        frontmost = j;
                }
                if (frontmost)
                    result.append({ offsetSoFar, offsets[i].value, offsets[*frontmost].markedText->type, offsets[*frontmost].markedText->marker, offsets[*frontmost].markedText->highlightName });
            } else {
                // The appended marked texts may not be in paint order. We will fix this up at the end of this function.
                for (unsigned j = 0; j < i; ++j) {
                    if (!processedMarkedTexts.contains(offsets[j].markedText))
                        result.append({ offsetSoFar, offsets[i].value, offsets[j].markedText->type, offsets[j].markedText->marker, offsets[j].markedText->highlightName });
                }
            }
            offsetSoFar = offsets[i].value;
        }
        if (offsets[i].kind == Offset::End)
            processedMarkedTexts.add(offsets[i].markedText);
    }
    // Fix up; sort the marked texts so that they are in paint order.
    if (overlapStrategy == OverlapStrategy::None)
        std::sort(result.begin(), result.end(), [] (const MarkedText& a, const MarkedText& b) { return a.startOffset < b.startOffset || (a.startOffset == b.startOffset && a.type < b.type); });
    return result;
}

Vector<MarkedText> MarkedText::collectForHighlights(RenderText& renderer, RenderBoxModelObject& parentRenderer, const TextBoxSelectableRange& selectableRange, PaintPhase phase)
{
    Vector<MarkedText> markedTexts;
    HighlightData highlightData;
    if (RuntimeEnabledFeatures::sharedFeatures().highlightAPIEnabled()) {
        auto& parentStyle = parentRenderer.style();
        if (auto highlightRegister = renderer.document().highlightRegisterIfExists()) {
            for (auto& highlight : highlightRegister->map()) {
                auto renderStyle = parentRenderer.getUncachedPseudoStyle({ PseudoId::Highlight, highlight.key }, &parentStyle);
                if (!renderStyle)
                    continue;
                if (renderStyle->textDecorationsInEffect().isEmpty() && phase == PaintPhase::Decoration)
                    continue;
                for (auto& rangeData : highlight.value->rangesData()) {
                    if (!highlightData.setRenderRange(rangeData))
                        continue;

                    auto [highlightStart, highlightEnd] = highlightData.rangeForTextBox(renderer, selectableRange);
                    if (highlightStart < highlightEnd)
                        markedTexts.append({ highlightStart, highlightEnd, MarkedText::Highlight, nullptr, highlight.key });
                }
            }
        }
    }
#if ENABLE(APP_HIGHLIGHTS)
    if (auto appHighlightRegister = renderer.document().appHighlightRegisterIfExists()) {
        if (appHighlightRegister->highlightsVisibility() == HighlightVisibility::Visible) {
            for (auto& highlight : appHighlightRegister->map()) {
                for (auto& rangeData : highlight.value->rangesData()) {
                    if (!highlightData.setRenderRange(rangeData))
                        continue;

                    auto [highlightStart, highlightEnd] = highlightData.rangeForTextBox(renderer, selectableRange);
                    if (highlightStart < highlightEnd)
                        markedTexts.append({ highlightStart, highlightEnd, MarkedText::AppHighlight });
                }
            }
        }
    }
#endif
    return markedTexts;
}

Vector<MarkedText> MarkedText::collectForDocumentMarkers(RenderText& renderer, const TextBoxSelectableRange& selectableRange, PaintPhase phase)
{
    if (!renderer.textNode())
        return { };

    Vector<RenderedDocumentMarker*> markers = renderer.document().markers().markersFor(*renderer.textNode());

    auto markedTextTypeForMarkerType = [] (DocumentMarker::MarkerType type) {
        switch (type) {
        case DocumentMarker::Spelling:
            return MarkedText::SpellingError;
        case DocumentMarker::Grammar:
            return MarkedText::GrammarError;
        case DocumentMarker::CorrectionIndicator:
            return MarkedText::Correction;
        case DocumentMarker::TextMatch:
            return MarkedText::TextMatch;
        case DocumentMarker::DictationAlternatives:
            return MarkedText::DictationAlternatives;
#if PLATFORM(IOS_FAMILY)
        case DocumentMarker::DictationPhraseWithAlternatives:
            return MarkedText::DictationPhraseWithAlternatives;
#endif
        default:
            return MarkedText::Unmarked;
        }
    };

    Vector<MarkedText> markedTexts;
    markedTexts.reserveInitialCapacity(markers.size());

    // Give any document markers that touch this run a chance to draw before the text has been drawn.
    // Note end() points at the last char, not one past it like endOffset and ranges do.
    for (auto* marker : markers) {
        // Collect either the background markers or the foreground markers, but not both
        switch (marker->type()) {
        case DocumentMarker::Grammar:
        case DocumentMarker::Spelling:
        case DocumentMarker::CorrectionIndicator:
        case DocumentMarker::Replacement:
        case DocumentMarker::DictationAlternatives:
#if PLATFORM(IOS_FAMILY)
        // FIXME: Remove the PLATFORM(IOS_FAMILY)-guard.
        case DocumentMarker::DictationPhraseWithAlternatives:
#endif
            if (phase != MarkedText::PaintPhase::Decoration)
                continue;
            break;
        case DocumentMarker::TextMatch:
            if (!renderer.frame().editor().markedTextMatchesAreHighlighted())
                continue;
            if (phase == MarkedText::PaintPhase::Decoration)
                continue;
            break;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        case DocumentMarker::TelephoneNumber:
            if (!renderer.frame().editor().markedTextMatchesAreHighlighted())
                continue;
            if (phase != MarkedText::PaintPhase::Background)
                continue;
            break;
#endif
        default:
            continue;
        }

        if (marker->endOffset() <= selectableRange.start) {
            // Marker is completely before this run. This might be a marker that sits before the
            // first run we draw, or markers that were within runs we skipped due to truncation.
            continue;
        }

        if (marker->startOffset() >= selectableRange.start + selectableRange.length) {
            // Marker is completely after this run, bail. A later run will paint it.
            break;
        }

        // Marker intersects this run. Collect it.
        switch (marker->type()) {
        case DocumentMarker::Spelling:
        case DocumentMarker::CorrectionIndicator:
        case DocumentMarker::DictationAlternatives:
        case DocumentMarker::Grammar:
#if PLATFORM(IOS_FAMILY)
        // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
        case DocumentMarker::DictationPhraseWithAlternatives:
#endif
        case DocumentMarker::TextMatch: {
            auto [clampedStart, clampedEnd] = selectableRange.clamp(marker->startOffset(), marker->endOffset());
            markedTexts.uncheckedAppend({ clampedStart, clampedEnd, markedTextTypeForMarkerType(marker->type()), marker });
            break;
        }
        case DocumentMarker::Replacement:
            break;
#if ENABLE(TELEPHONE_NUMBER_DETECTION)
        case DocumentMarker::TelephoneNumber:
            break;
#endif
        default:
            ASSERT_NOT_REACHED();
        }
    }
    return markedTexts;
}

Vector<MarkedText> MarkedText::collectForDraggedContent(RenderText& renderer, const TextBoxSelectableRange& selectableRange)
{
    auto draggedContentRanges = renderer.draggedContentRangesBetweenOffsets(selectableRange.start, selectableRange.start + selectableRange.length);

    return draggedContentRanges.map([&](const auto& range) -> MarkedText {
        return { selectableRange.clamp(range.first), selectableRange.clamp(range.second), MarkedText::DraggedContent };
    });
}

}
