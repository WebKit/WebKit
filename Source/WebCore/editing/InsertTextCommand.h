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
    virtual void addMarkersToTextNode(Text&, unsigned offsetOfInsertion, const String& textInserted) = 0;
protected:
    TextInsertionMarkerSupplier() = default;
};

class InsertTextCommand : public CompositeEditCommand {
public:
    enum RebalanceType {
        RebalanceLeadingAndTrailingWhitespaces,
        RebalanceAllWhitespaces
    };

    static Ref<InsertTextCommand> create(Document& document, const String& text, bool selectInsertedText = false,
        RebalanceType rebalanceType = RebalanceLeadingAndTrailingWhitespaces, EditAction editingAction = EditAction::Insert)
    {
        return adoptRef(*new InsertTextCommand(document, text, selectInsertedText, rebalanceType, editingAction));
    }

    static Ref<InsertTextCommand> createWithMarkerSupplier(Document& document, const String& text, Ref<TextInsertionMarkerSupplier>&& markerSupplier, EditAction editingAction = EditAction::Insert)
    {
        return adoptRef(*new InsertTextCommand(document, text, WTFMove(markerSupplier), editingAction));
    }

protected:
    InsertTextCommand(Document&, const String& text, Ref<TextInsertionMarkerSupplier>&&, EditAction);
    InsertTextCommand(Document&, const String& text, bool selectInsertedText, RebalanceType, EditAction);

private:

    void deleteCharacter();

    void doApply() override;

    bool isInsertTextCommand() const override { return true; }

    Position positionInsideTextNode(const Position&);
    Position insertTab(const Position&);
    
    bool performTrivialReplace(const String&, bool selectInsertedText);
    bool performOverwrite(const String&, bool selectInsertedText);
    void setEndingSelectionWithoutValidation(const Position& startPosition, const Position& endPosition);

    friend class TypingCommand;

    String m_text;
    bool m_selectInsertedText;
    RebalanceType m_rebalanceType;
    RefPtr<TextInsertionMarkerSupplier> m_markerSupplier;
};

} // namespace WebCore
