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
#import "WritingToolsCompositionCommand.h"
#import "WritingToolsTypes.h"
#import <wtf/CheckedPtr.h>
#import <wtf/FastMalloc.h>
#import <wtf/WeakPtr.h>

namespace WebCore {

class CompositeEditCommand;
class EditCommandComposition;
class Document;
class DocumentFragment;
class DocumentMarker;
class Node;
class Page;

struct SimpleRange;

enum class TextAnimationRunMode : uint8_t;

class WritingToolsController final : public CanMakeWeakPtr<WritingToolsController>, public CanMakeCheckedPtr<WritingToolsController> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(WritingToolsController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WritingToolsController);

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

    void respondToUnappliedEditing(EditCommandComposition*);
    void respondToReappliedEditing(EditCommandComposition*);

    // FIXME: Refactor `TextAnimationController` in such a way so as to not explicitly depend on `WritingToolsController`,
    // and then remove these methods after doing so.
    std::optional<SimpleRange> activeSessionRange() const;
    void showSelection() const;

private:
    struct CompositionState : CanMakeCheckedPtr<CompositionState> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(CompositionState);

        CompositionState(const Vector<Ref<WritingToolsCompositionCommand>>& unappliedCommands, const Vector<Ref<WritingToolsCompositionCommand>>& reappliedCommands, const WritingTools::Session& session)
            : unappliedCommands(unappliedCommands)
            , reappliedCommands(reappliedCommands)
            , session(session)
        {
        }

        // These two vectors should never have the same command in both of them.
        Vector<Ref<WritingToolsCompositionCommand>> unappliedCommands;
        Vector<Ref<WritingToolsCompositionCommand>> reappliedCommands;
        WritingTools::Session session;
        std::optional<SimpleRange> currentRange;
    };

    struct ProofreadingState : CanMakeCheckedPtr<ProofreadingState> {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        WTF_STRUCT_OVERRIDE_DELETE_FOR_CHECKED_PTR(ProofreadingState);

        ProofreadingState(const Ref<Range>& contextRange, const WritingTools::Session& session, int replacementLocationOffset)
            : contextRange(contextRange)
            , session(session)
            , replacementLocationOffset(replacementLocationOffset)
        {
        }

        Ref<Range> contextRange;
        WritingTools::Session session;
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

    class EditingScope {
        WTF_MAKE_NONCOPYABLE(EditingScope); WTF_MAKE_FAST_ALLOCATED;
    public:
        EditingScope(Document&);
        ~EditingScope();

    private:
        RefPtr<Document> m_document;
        bool m_editingWasSuppressed;
    };

    static CharacterRange characterRange(const SimpleRange& scope, const SimpleRange&);
    static SimpleRange resolveCharacterRange(const SimpleRange& scope, CharacterRange);
    static uint64_t characterCount(const SimpleRange&);
    static String plainText(const SimpleRange&);

    template<WritingTools::Session::Type Type>
    StateFromSessionType<Type>::Value* currentState();

    std::optional<std::tuple<Node&, DocumentMarker&>> findTextSuggestionMarkerContainingRange(const SimpleRange&) const;
    std::optional<std::tuple<Node&, DocumentMarker&>> findTextSuggestionMarkerByID(const SimpleRange& outerRange, const WritingTools::TextSuggestion::ID&) const;

    void replaceContentsOfRangeInSession(ProofreadingState&, const SimpleRange&, const String&);
    void replaceContentsOfRangeInSession(CompositionState&, const SimpleRange&, const AttributedString&, WritingToolsCompositionCommand::State);

    void compositionSessionDidFinishReplacement();
    void compositionSessionDidFinishReplacement(const CharacterRange&, const String&);

    void compositionSessionDidReceiveTextWithReplacementRangeAsync(const AttributedString&, const CharacterRange&, const WritingTools::Context&, bool finished, TextAnimationRunMode);

    void showOriginalCompositionForSession();
    void showRewrittenCompositionForSession();
    void restartCompositionForSession();

    template<WritingTools::Session::Type Type>
    void writingToolsSessionDidReceiveAction(WritingTools::Action);

    template<WritingTools::Session::Type Type>
    void didEndWritingToolsSession(bool accepted);

    RefPtr<Document> document() const;

    WeakPtr<Page> m_page;

    std::variant<std::monostate, ProofreadingState, CompositionState> m_state;
};

} // namespace WebKit

#endif
