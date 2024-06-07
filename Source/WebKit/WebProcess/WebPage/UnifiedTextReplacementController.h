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
#include "WebUnifiedTextReplacementSessionData.h"

#include <WebCore/DocumentFragment.h>
#include <WebCore/Editor.h>
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
}

namespace WebKit {

enum class WebUnifiedTextReplacementSessionDataReplacementType : uint8_t;

class WebPage;

struct WebUnifiedTextReplacementContextData;

class UnifiedTextReplacementController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UnifiedTextReplacementController);

public:
    explicit UnifiedTextReplacementController(WebPage&);

    void willBeginTextReplacementSession(const std::optional<WebUnifiedTextReplacementSessionData>&, CompletionHandler<void(const Vector<WebUnifiedTextReplacementContextData>&)>&&);

    void didBeginTextReplacementSession(const WebUnifiedTextReplacementSessionData&, const Vector<WebUnifiedTextReplacementContextData>&);

    void textReplacementSessionDidReceiveReplacements(const WebUnifiedTextReplacementSessionData&, const Vector<WebTextReplacementData>&, const WebUnifiedTextReplacementContextData&, bool finished);

    void textReplacementSessionDidUpdateStateForReplacement(const WebUnifiedTextReplacementSessionData&, WebTextReplacementData::State, const WebTextReplacementData&, const WebUnifiedTextReplacementContextData&);

    void didEndTextReplacementSession(const WebUnifiedTextReplacementSessionData&, bool accepted);

    void textReplacementSessionDidReceiveTextWithReplacementRange(const WebUnifiedTextReplacementSessionData&, const WebCore::AttributedString&, const WebCore::CharacterRange&, const WebUnifiedTextReplacementContextData&, bool finished);

    void textReplacementSessionDidReceiveEditAction(const WebUnifiedTextReplacementSessionData&, WebTextReplacementData::EditAction);

    void updateStateForSelectedReplacementIfNeeded();

    std::optional<WebCore::SimpleRange> contextRangeForSessionWithUUID(const WTF::UUID&) const;

private:
    struct State {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;

        WebUnifiedTextReplacementSessionData session;
    };

    static WebCore::CharacterRange characterRange(const WebCore::SimpleRange& scope, const WebCore::SimpleRange&);
    static WebCore::SimpleRange resolveCharacterRange(const WebCore::SimpleRange& scope, WebCore::CharacterRange);
    static uint64_t characterCount(const WebCore::SimpleRange&);
    static String plainText(const WebCore::SimpleRange&);

    std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> findReplacementMarkerContainingRange(const WebCore::SimpleRange&) const;
    std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> findReplacementMarkerByUUID(const WebCore::SimpleRange& outerRange, const WTF::UUID& replacementUUID) const;

    void replaceContentsOfRangeInSessionInternal(const WTF::UUID&, const WebCore::SimpleRange&, WTF::Function<void(WebCore::Editor&)>&&);
    void replaceContentsOfRangeInSession(const WTF::UUID&, const WebCore::SimpleRange&, const String&);
    void replaceContentsOfRangeInSession(const WTF::UUID&, const WebCore::SimpleRange&, WebCore::DocumentFragment&, WebCore::Editor::MatchStyle = WebCore::Editor::MatchStyle::No);

    void textReplacementSessionPerformEditActionForPlainText(WebCore::Document&, const WebUnifiedTextReplacementSessionData&, WebTextReplacementData::EditAction);
    void textReplacementSessionPerformEditActionForRichText(WebCore::Document&, const WebUnifiedTextReplacementSessionData&, WebTextReplacementData::EditAction);

    template<WebUnifiedTextReplacementSessionData::ReplacementType Type>
    void didEndTextReplacementSession(const WebUnifiedTextReplacementSessionData&, bool accepted);

    RefPtr<WebCore::Document> document() const;

    WeakPtr<WebPage> m_webPage;

    // FIXME: Unify these states into a single `State` struct.
    HashMap<WTF::UUID, Ref<WebCore::Range>> m_contextRanges;
    HashMap<WTF::UUID, UniqueRef<State>> m_states;
    HashMap<WTF::UUID, int> m_replacementLocationOffsets;
    HashMap<WTF::UUID, Ref<WebCore::DocumentFragment>> m_originalDocumentNodes;
    HashMap<WTF::UUID, Ref<WebCore::DocumentFragment>> m_replacedDocumentNodes;
};

} // namespace WebKit

#endif
