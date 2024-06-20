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

#import "Range.h"
#import "ReplaceSelectionCommand.h"
#import "WritingToolsTypes.h"

namespace WebCore {

class Document;
class DocumentFragment;
class DocumentMarker;
class Node;
class Page;

struct SimpleRange;

class WritingToolsController final {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WritingToolsController);

public:
    explicit WritingToolsController(Page&);

    void willBeginWritingToolsSession(const std::optional<WritingTools::Session>&, CompletionHandler<void(const Vector<WritingTools::Context>&)>&&);

    void didBeginWritingToolsSession(const WritingTools::Session&, const Vector<WritingTools::Context>&);

    void proofreadingSessionDidReceiveSuggestions(const WritingTools::Session&, const Vector<WritingTools::TextSuggestion>&, const WritingTools::Context&, bool finished);

    void proofreadingSessionDidUpdateStateForSuggestion(const WritingTools::Session&, WritingTools::TextSuggestion::State, const WritingTools::TextSuggestion&, const WritingTools::Context&);

    void didEndWritingToolsSession(const WritingTools::Session&, bool accepted);

    void compositionSessionDidReceiveTextWithReplacementRange(const WritingTools::Session&, const AttributedString&, const CharacterRange&, const WritingTools::Context&, bool finished);

    void writingToolsSessionDidReceiveAction(const WritingTools::Session&, WritingTools::Action);

    void updateStateForSelectedSuggestionIfNeeded();

    // FIXME: Refactor `TextAnimationController` in such a way so as to not explicitly depend on `WritingToolsController`,
    // and then remove this method after doing so.
    std::optional<SimpleRange> contextRangeForSessionWithID(const WritingTools::Session::ID&) const;

private:
    struct CompositionState : CanMakeCheckedPtr<CompositionState> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(CompositionState);

        CompositionState(const Ref<Range>& contextRange, const Vector<Ref<ReplaceSelectionCommand>>& commands)
            : contextRange(contextRange)
            , commands(commands)
        {
        }

        Ref<Range> contextRange;
        Vector<Ref<ReplaceSelectionCommand>> commands;
    };

    struct ProofreadingState : CanMakeCheckedPtr<ProofreadingState> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProofreadingState);

        ProofreadingState(const Ref<Range>& contextRange, int replacementLocationOffset)
            : contextRange(contextRange)
            , replacementLocationOffset(replacementLocationOffset)
        {
        }

        Ref<Range> contextRange;
        int replacementLocationOffset { 0 };
    };

    template<WritingTools::Session::Type Type>
    struct StateFromSessionType { };

    template<>
    struct StateFromSessionType<WritingTools::Session::Type::Proofreading> {
        using Value = ProofreadingState;
    };

    template<>
    struct StateFromSessionType<WritingTools::Session::Type::Composition> {
        using Value = CompositionState;
    };

    enum MatchStyle : bool {
        No, Yes
    };

    static CharacterRange characterRange(const SimpleRange& scope, const SimpleRange&);
    static SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange);
    static uint64_t characterCount(const SimpleRange&);
    static String plainText(const SimpleRange&);

    template<WritingTools::Session::Type Type>
    StateFromSessionType<Type>::Value* stateForSession(const WritingTools::Session&);

    std::optional<std::tuple<Node&, DocumentMarker&>> findTextSuggestionMarkerContainingRange(const SimpleRange&) const;
    std::optional<std::tuple<Node&, DocumentMarker&>> findTextSuggestionMarkerByID(const SimpleRange& outerRange, const WritingTools::TextSuggestion::ID&) const;

    template<typename State>
    void replaceContentsOfRangeInSessionInternal(State&, const SimpleRange&, WTF::Function<void()>&&);
    void replaceContentsOfRangeInSession(ProofreadingState&, const SimpleRange&, const String&);
    void replaceContentsOfRangeInSession(CompositionState&, const SimpleRange&, RefPtr<DocumentFragment>&&, MatchStyle);

    template<WritingTools::Session::Type Type>
    void writingToolsSessionDidReceiveAction(const WritingTools::Session&, WritingTools::Action);

    template<WritingTools::Session::Type Type>
    void didEndWritingToolsSession(const WritingTools::Session&, bool accepted);

    RefPtr<Document> document() const;

    SingleThreadWeakPtr<Page> m_page;

    HashMap<WritingTools::Session::ID, std::variant<std::monostate, ProofreadingState, CompositionState>> m_states;
};

} // namespace WebKit

#endif
