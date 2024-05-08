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

#pragma once

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)

#include "WebTextReplacementData.h"

#include <WebCore/DocumentFragment.h>
#include <WebCore/Node.h>
#include <WebCore/Range.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/UUID.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
struct TextIndicatorData;
enum class TextIndicatorOption : uint16_t;
class DocumentMarker;
class Editor;
}

namespace WebKit {

enum class WebUnifiedTextReplacementType : uint8_t;

class WebPage;

struct WebUnifiedTextReplacementContextData;

class UnifiedTextReplacementController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UnifiedTextReplacementController);

public:
    explicit UnifiedTextReplacementController(WebPage&);

    void willBeginTextReplacementSession(const WTF::UUID&, WebUnifiedTextReplacementType, CompletionHandler<void(const Vector<WebUnifiedTextReplacementContextData>&)>&&);

    void didBeginTextReplacementSession(const WTF::UUID&, const Vector<WebUnifiedTextReplacementContextData>&);

    void textReplacementSessionDidReceiveReplacements(const WTF::UUID&, const Vector<WebTextReplacementData>&, const WebUnifiedTextReplacementContextData&, bool finished);

    void textReplacementSessionDidUpdateStateForReplacement(const WTF::UUID&, WebTextReplacementData::State, const WebTextReplacementData&, const WebUnifiedTextReplacementContextData&);

    void didEndTextReplacementSession(const WTF::UUID&, bool accepted);

    void textReplacementSessionDidReceiveTextWithReplacementRange(const WTF::UUID&, const WebCore::AttributedString&, const WebCore::CharacterRange&, const WebUnifiedTextReplacementContextData&);

    void textReplacementSessionDidReceiveEditAction(const WTF::UUID&, WebTextReplacementData::EditAction);

    void updateStateForSelectedReplacementIfNeeded();

    std::optional<WebCore::SimpleRange> contextRangeForSessionWithUUID(const WTF::UUID&) const;

    void removeTransparentMarkersForSession(const WTF::UUID&, WebCore::SimpleRange&);

private:
    std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> findReplacementMarkerContainingRange(const WebCore::SimpleRange&) const;
    std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> findReplacementMarkerByUUID(const WebCore::SimpleRange& outerRange, const WTF::UUID& replacementUUID) const;

    void replaceContentsOfRangeInSessionInternal(const WTF::UUID&, const WebCore::SimpleRange&, WTF::Function<void(WebCore::Editor&)>&&);
    void replaceContentsOfRangeInSession(const WTF::UUID&, const WebCore::SimpleRange&, const String&);
    void replaceContentsOfRangeInSession(const WTF::UUID&, const WebCore::SimpleRange&, WebCore::DocumentFragment&);

    void textReplacementSessionPerformEditActionForPlainText(WebCore::Document&, const WTF::UUID&, WebTextReplacementData::EditAction);
    void textReplacementSessionPerformEditActionForRichText(WebCore::Document&, const WTF::UUID&, WebTextReplacementData::EditAction);

    template<WebUnifiedTextReplacementType Type>
    void didEndTextReplacementSession(const WTF::UUID&, bool accepted);

    RefPtr<WebCore::Document> document() const;

    WeakPtr<WebPage> m_webPage;

    // FIXME: Unify these states into a single `State` struct.
    HashMap<WTF::UUID, Ref<WebCore::Range>> m_contextRanges;
    HashMap<WTF::UUID, WebUnifiedTextReplacementType> m_replacementTypes;
    HashMap<WTF::UUID, int> m_replacementLocationOffsets;
    HashMap<WTF::UUID, Ref<WebCore::DocumentFragment>> m_originalDocumentNodes;
    HashMap<WTF::UUID, Ref<WebCore::DocumentFragment>> m_replacedDocumentNodes;
};

} // namespace WebKit

#endif
