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

#if ENABLE(WRITING_TOOLS)

#include "UnifiedTextReplacementTypes.h"

namespace WebCore {

class Document;
class DocumentFragment;
class DocumentMarker;
class Editor;
class Node;
class Page;
class Range;

struct SimpleRange;

class UnifiedTextReplacementController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UnifiedTextReplacementController);

public:
    explicit UnifiedTextReplacementController(Page&);

    void willBeginTextReplacementSession(const std::optional<UnifiedTextReplacement::Session>&, CompletionHandler<void(const Vector<UnifiedTextReplacement::Context>&)>&&);

    void didBeginTextReplacementSession(const UnifiedTextReplacement::Session&, const Vector<UnifiedTextReplacement::Context>&);

    void textReplacementSessionDidReceiveReplacements(const UnifiedTextReplacement::Session&, const Vector<UnifiedTextReplacement::Replacement>&, const UnifiedTextReplacement::Context&, bool finished);

    void textReplacementSessionDidUpdateStateForReplacement(const UnifiedTextReplacement::Session&, UnifiedTextReplacement::Replacement::State, const UnifiedTextReplacement::Replacement&, const UnifiedTextReplacement::Context&);

    void didEndTextReplacementSession(const UnifiedTextReplacement::Session&, bool accepted);

    void textReplacementSessionDidReceiveTextWithReplacementRange(const UnifiedTextReplacement::Session&, const AttributedString&, const CharacterRange&, const UnifiedTextReplacement::Context&, bool finished);

    void textReplacementSessionDidReceiveEditAction(const UnifiedTextReplacement::Session&, UnifiedTextReplacement::EditAction);

    void updateStateForSelectedReplacementIfNeeded();

    WEBCORE_EXPORT std::optional<SimpleRange> contextRangeForSessionWithID(const UnifiedTextReplacement::Session::ID&) const;

private:
    enum MatchStyle : bool {
        No, Yes
    };

    static CharacterRange characterRange(const SimpleRange& scope, const SimpleRange&);
    static SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange);
    static uint64_t characterCount(const SimpleRange&);
    static String plainText(const SimpleRange&);

    std::optional<std::tuple<Node&, DocumentMarker&>> findReplacementMarkerContainingRange(const SimpleRange&) const;
    std::optional<std::tuple<Node&, DocumentMarker&>> findReplacementMarkerByID(const SimpleRange& outerRange, const UnifiedTextReplacement::Replacement::ID&) const;

    void replaceContentsOfRangeInSessionInternal(const UnifiedTextReplacement::Session::ID&, const SimpleRange&, WTF::Function<void(Editor&)>&&);
    void replaceContentsOfRangeInSession(const UnifiedTextReplacement::Session::ID&, const SimpleRange&, const String&);
    void replaceContentsOfRangeInSession(const UnifiedTextReplacement::Session::ID&, const SimpleRange&, DocumentFragment&, MatchStyle = MatchStyle::No);

    template<UnifiedTextReplacement::Session::ReplacementType Type>
    void textReplacementSessionDidReceiveEditAction(const UnifiedTextReplacement::Session&, UnifiedTextReplacement::EditAction);

    template<UnifiedTextReplacement::Session::ReplacementType Type>
    void didEndTextReplacementSession(const UnifiedTextReplacement::Session&, bool accepted);

    RefPtr<Document> document() const;

    SingleThreadWeakPtr<Page> m_page;

    // FIXME: Unify these states into a single `State` struct.
    HashMap<UnifiedTextReplacement::Session::ID, Ref<Range>> m_contextRanges;
    HashMap<UnifiedTextReplacement::Session::ID, int> m_replacementLocationOffsets;
    HashMap<UnifiedTextReplacement::Session::ID, Ref<DocumentFragment>> m_originalDocumentNodes;
    HashMap<UnifiedTextReplacement::Session::ID, Ref<DocumentFragment>> m_replacedDocumentNodes;
};

} // namespace WebKit

#endif
