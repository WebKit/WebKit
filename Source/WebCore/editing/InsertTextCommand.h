/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "CompositeEditCommand.h"

namespace WebCore {

class DocumentMarkerController;
class Text;

class TextInsertionMarkerSupplier : public RefCounted<TextInsertionMarkerSupplier> {
public:
    virtual ~TextInsertionMarkerSupplier() = default;
    virtual void addMarkersToTextNode(Text&, unsigned offsetOfInsertion, StringImpl* textInserted) = 0;
protected:
    TextInsertionMarkerSupplier() = default;
};

struct SmartListUndoData {
    RefPtr<StringImpl> previousLineText;
    RefPtr<StringImpl> currentLineText;
    RefPtr<HTMLElement> listElement;
};

class InsertTextCommand : public CompositeEditCommand {
public:
    enum RebalanceType {
        RebalanceLeadingAndTrailingWhitespaces,
        RebalanceAllWhitespaces
    };

    static Ref<InsertTextCommand> create(Ref<Document>&& document, StringImpl* text, AllowPasswordEcho allowPasswordEcho
        , bool selectInsertedText = false, RebalanceType rebalanceType = RebalanceLeadingAndTrailingWhitespaces, EditAction editingAction = EditAction::Insert)
    {
        return adoptRef(*new InsertTextCommand(WTF::move(document), text, allowPasswordEcho, selectInsertedText, rebalanceType, editingAction));
    }

    static Ref<InsertTextCommand> createWithMarkerSupplier(Ref<Document>&& document, StringImpl* text, Ref<TextInsertionMarkerSupplier>&& markerSupplier, EditAction editingAction = EditAction::Insert)
    {
        return adoptRef(*new InsertTextCommand(WTF::move(document), text, WTF::move(markerSupplier), editingAction));
    }

protected:
    InsertTextCommand(Ref<Document>&&, StringImpl* text, Ref<TextInsertionMarkerSupplier>&&, EditAction);
    InsertTextCommand(Ref<Document>&&, StringImpl* text, AllowPasswordEcho, bool selectInsertedText, RebalanceType, EditAction);

private:

    void doApply() override;

    bool isInsertTextCommand() const override { return true; }

    Position positionInsideTextNode(const Position&);
    Position insertTab(const Position&);
    
    bool performTrivialReplace(StringImpl*, bool selectInsertedText);
    bool performOverwrite(StringImpl*, bool selectInsertedText);
    void setEndingSelectionWithoutValidation(const Position& startPosition, const Position& endPosition);

    bool applySmartListsIfNeeded();

    friend class TypingCommand;

    RefPtr<StringImpl> m_text;
    AllowPasswordEcho m_allowPasswordEcho { AllowPasswordEcho::Yes };
    bool m_selectInsertedText;
    RebalanceType m_rebalanceType;
    const RefPtr<TextInsertionMarkerSupplier> m_markerSupplier;
    RefPtr<EditingStyle> m_styleToPreserveForSmartList;
    std::optional<SmartListUndoData> m_smartListUndoData;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InsertTextCommand)
    static bool isType(const WebCore::EditCommand& command) { return command.isInsertTextCommand(); }
SPECIALIZE_TYPE_TRAITS_END()
