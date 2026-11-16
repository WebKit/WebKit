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
#import "TextEffectController.h"

#if ENABLE(WRITING_TOOLS_TEXT_EFFECTS)

#import "Chrome.h"
#import "ChromeClient.h"
#import "Document.h"
#import "DocumentMarkerController.h"
#import "DocumentMarkers.h"
#import "FocusController.h"
#import "LocalFrameInlines.h"
#import "Page.h"
#import "RenderedDocumentMarker.h"
#import "SimpleRange.h"
#import "TextIndicator.h"
#import <WebCore/WritingDirection.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextEffectController);

TextEffectController::TextEffectController(Page& page)
    : m_page(page)
{
}

void TextEffectController::addTextEffect(const SimpleRange& range, RefPtr<TextIndicator>&& textIndicator, RefPtr<TextIndicator>&& decorationIndicator)
{
    auto uuid = WTF::UUID::createVersion4();

    m_activeEffects.append({ uuid, range });

    TextEffectData data { WritingDirection::Natural };
    m_page->chrome().client().addTextEffectForID(uuid, WTF::move(data), WTF::move(textIndicator), WTF::move(decorationIndicator));
}

void TextEffectController::removeTextEffect(const SimpleRange& range)
{
    m_activeEffects.removeAllMatching([&](auto& effect) {
        if (!intersects(range, effect.range))
            return false;
        removeTransparentMarkersForTextEffectID(effect.uuid);
        m_page->chrome().client().removeTextEffectForID(effect.uuid);
        return true;
    });
}

void TextEffectController::removeAllTextEffects()
{
    for (auto& effect : m_activeEffects) {
        removeTransparentMarkersForTextEffectID(effect.uuid);
        m_page->chrome().client().removeTextEffectForID(effect.uuid);
    }

    m_activeEffects.clear();
}

void TextEffectController::updateUnderlyingTextVisibilityForTextEffectID(const WTF::UUID& uuid, bool visible, CompletionHandler<void()>&& completionHandler)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler();
        return;
    }

    if (visible)
        removeTransparentMarkersForTextEffectID(uuid);
    else {
        auto range = rangeForTextEffectID(uuid);
        if (!range) {
            completionHandler();
            return;
        }
        protect(document->markers())->addTransparentContentMarker(*range, uuid);
    }

    completionHandler();
}

void TextEffectController::createTextIndicatorForTextEffectID(const WTF::UUID& uuid, CompletionHandler<void(RefPtr<TextIndicator>&&)>&& completionHandler)
{
    auto range = rangeForTextEffectID(uuid);
    if (!range) {
        completionHandler(nullptr);
        return;
    }

    completionHandler(createTextIndicatorForRange(*range));
}

RefPtr<Document> TextEffectController::document() const
{
    if (!m_page) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return frame->document();
}

std::optional<SimpleRange> TextEffectController::rangeForTextEffectID(const WTF::UUID& uuid) const
{
    for (auto& effect : m_activeEffects) {
        if (effect.uuid == uuid)
            return effect.range;
    }
    return std::nullopt;
}

RefPtr<TextIndicator> TextEffectController::createTextIndicatorForRange(const SimpleRange& range, IncludeDocumentMarkers includeDocumentMarkers)
{
    OptionSet textIndicatorOptions {
        TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        TextIndicatorOption::ExpandClipBeyondVisibleRect,
        TextIndicatorOption::SkipReplacedContent,
        TextIndicatorOption::RespectTextColor,
    };

    if (includeDocumentMarkers == IncludeDocumentMarkers::Yes)
        textIndicatorOptions.add(TextIndicatorOption::IncludeDocumentMarkers);

    return TextIndicator::createWithRange(range, textIndicatorOptions, TextIndicatorPresentationTransition::None, { });
}

void TextEffectController::removeTransparentMarkersForTextEffectID(const WTF::UUID& uuid)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    protect(document->markers())->removeMarkers({ DocumentMarkerType::TransparentContent }, [&uuid](const DocumentMarker& marker) {
        return std::get<DocumentMarker::TransparentContentData>(marker.data()).uuid == uuid ? FilterMarkerResult::Remove : FilterMarkerResult::Keep;
    });
}

} // namespace WebCore

#endif // ENABLE(WRITING_TOOLS_TEXT_EFFECTS)
