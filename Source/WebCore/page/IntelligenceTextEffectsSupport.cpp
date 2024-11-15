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

#include "config.h"
#include "IntelligenceTextEffectsSupport.h"

#include "CharacterRange.h"
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "FloatRect.h"
#include "RenderedDocumentMarker.h"
#include "SimpleRange.h"
#include "TextIndicator.h"
#include "TextIterator.h"
#include <wtf/UUID.h>

namespace WebCore {
namespace IntelligenceTextEffectsSupport {

#if ENABLE(WRITING_TOOLS)
Vector<FloatRect> writingToolsTextSuggestionRectsInRootViewCoordinates(Document& document, const SimpleRange& scope, const CharacterRange& range)
{
    auto resolvedRange = resolveCharacterRange(scope, range);

    Vector<FloatRect> textRectsInRootViewCoordinates;

    auto& markers = document.markers();
    markers.forEach(resolvedRange, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&](auto& node, auto& marker) {
        auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());

        auto markerRange = makeSimpleRange(node, marker);

        auto rect = document.view()->contentsToRootView(unionRect(RenderObject::absoluteTextRects(markerRange, { })));
        textRectsInRootViewCoordinates.append(WTFMove(rect));

        return false;
    });

    return textRectsInRootViewCoordinates;
}
#endif

void updateTextVisibility(Document& document, const SimpleRange& scope, const CharacterRange& range, bool visible, const WTF::UUID& identifier)
{
    if (visible) {
        document.markers().removeMarkers({ WebCore::DocumentMarker::Type::TransparentContent }, [identifier](auto& marker) {
            auto& data = std::get<WebCore::DocumentMarker::TransparentContentData>(marker.data());
            return data.uuid == identifier ? WebCore::FilterMarkerResult::Remove : WebCore::FilterMarkerResult::Keep;
        });
    } else {
        auto resolvedRange = resolveCharacterRange(scope, range);
        document.markers().addTransparentContentMarker(resolvedRange, identifier);
    }
}

std::optional<TextIndicatorData> textPreviewDataForRange(Document&, const SimpleRange& scope, const CharacterRange& range)
{
    auto resolvedRange = resolveCharacterRange(scope, range);

    static constexpr OptionSet textIndicatorOptions {
        TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        TextIndicatorOption::ExpandClipBeyondVisibleRect,
        TextIndicatorOption::SkipReplacedContent,
        TextIndicatorOption::RespectTextColor,
        TextIndicatorOption::DoNotClipToVisibleRect,
#if PLATFORM(VISION)
        TextIndicatorOption::SnapshotContentAt3xBaseScale,
#endif
    };

    RefPtr textIndicator = WebCore::TextIndicator::createWithRange(resolvedRange, textIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, { });
    if (!textIndicator)
        return std::nullopt;

    return textIndicator->data();
}

#if ENABLE(WRITING_TOOLS)
void decorateWritingToolsTextReplacements(Document& document, const SimpleRange& scope, const CharacterRange& range)
{
    auto resolvedRange = resolveCharacterRange(scope, range);

    auto& markers = document.markers();

    Vector<std::tuple<SimpleRange, DocumentMarker::WritingToolsTextSuggestionData>> markersToReinsert;

    markers.forEach(resolvedRange, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&](auto& node, auto& marker) {
        auto range = makeSimpleRange(node, marker);
        auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());

        markersToReinsert.append({ range, data });

        return false;
    });

    markers.removeMarkers(resolvedRange, { DocumentMarker::Type::WritingToolsTextSuggestion });

    for (const auto& [range, oldData] : markersToReinsert) {
        auto newData = DocumentMarker::WritingToolsTextSuggestionData { oldData.originalText, oldData.suggestionID, oldData.state, DocumentMarker::WritingToolsTextSuggestionData::Decoration::Underline };
        markers.addMarker(range, DocumentMarker::Type::WritingToolsTextSuggestion, newData);
    }
}
#endif

} // namespace IntelligenceTextEffectsSupport
} // namespace WebCore
