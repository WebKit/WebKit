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

#include "Range.h"
#include "ReplaceSelectionCommand.h"
#include "UnifiedTextReplacementTypes.h"

namespace WebCore {

class Document;
class DocumentFragment;
class DocumentMarker;
class Node;
class Page;

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

    // FIXME: Refactor `TextIndicatorStyleController` in such a way so as to not explicitly depend on `UnifiedTextReplacementController`,
    // and then remove this method after doing so.
    std::optional<SimpleRange> contextRangeForSessionWithID(const UnifiedTextReplacement::Session::ID&) const;

private:
    struct RichTextState : CanMakeCheckedPtr<RichTextState> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(RichTextState);

        RichTextState(const Ref<Range>& contextRange, const Vector<Ref<ReplaceSelectionCommand>>& commands)
            : contextRange(contextRange)
            , commands(commands)
        {
        }

        Ref<Range> contextRange;
        Vector<Ref<ReplaceSelectionCommand>> commands;
    };

    struct PlainTextState : CanMakeCheckedPtr<PlainTextState> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlainTextState);

        PlainTextState(const Ref<Range>& contextRange, int replacementLocationOffset)
            : contextRange(contextRange)
            , replacementLocationOffset(replacementLocationOffset)
        {
        }

        Ref<Range> contextRange;
        int replacementLocationOffset { 0 };
    };

    template<UnifiedTextReplacement::Session::ReplacementType Type>
    struct StateFromReplacementType { };

    template<>
    struct StateFromReplacementType<UnifiedTextReplacement::Session::ReplacementType::PlainText> {
        using Value = PlainTextState;
    };

    template<>
    struct StateFromReplacementType<UnifiedTextReplacement::Session::ReplacementType::RichText> {
        using Value = RichTextState;
    };

    enum MatchStyle : bool {
        No, Yes
    };

    static CharacterRange characterRange(const SimpleRange& scope, const SimpleRange&);
    static SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange);
    static uint64_t characterCount(const SimpleRange&);
    static String plainText(const SimpleRange&);

    template<UnifiedTextReplacement::Session::ReplacementType Type>
    StateFromReplacementType<Type>::Value* stateForSession(const UnifiedTextReplacement::Session&);

    std::optional<std::tuple<Node&, DocumentMarker&>> findReplacementMarkerContainingRange(const SimpleRange&) const;
    std::optional<std::tuple<Node&, DocumentMarker&>> findReplacementMarkerByID(const SimpleRange& outerRange, const UnifiedTextReplacement::Replacement::ID&) const;

    template<typename State>
    void replaceContentsOfRangeInSessionInternal(State&, const SimpleRange&, WTF::Function<void()>&&);
    void replaceContentsOfRangeInSession(PlainTextState&, const SimpleRange&, const String&);
    void replaceContentsOfRangeInSession(RichTextState&, const SimpleRange&, RefPtr<DocumentFragment>&&, MatchStyle);

    template<UnifiedTextReplacement::Session::ReplacementType Type>
    void textReplacementSessionDidReceiveEditAction(const UnifiedTextReplacement::Session&, UnifiedTextReplacement::EditAction);

    template<UnifiedTextReplacement::Session::ReplacementType Type>
    void didEndTextReplacementSession(const UnifiedTextReplacement::Session&, bool accepted);

    RefPtr<Document> document() const;

    SingleThreadWeakPtr<Page> m_page;

    HashMap<UnifiedTextReplacement::Session::ID, std::variant<std::monostate, PlainTextState, RichTextState>> m_states;
};

} // namespace WebKit

#endif
