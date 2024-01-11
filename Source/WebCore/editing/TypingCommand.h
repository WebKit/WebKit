/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "TextInsertionBaseCommand.h"
#include <wtf/CheckedRef.h>

namespace WebCore {

class TypingCommand final : public TextInsertionBaseCommand {
public:
    enum class Type : uint8_t {
        DeleteSelection,
        DeleteKey, 
        ForwardDeleteKey, 
        InsertText, 
        InsertLineBreak, 
        InsertParagraphSeparator,
        InsertParagraphSeparatorInQuotedContent
    };

    enum class TextCompositionType : uint8_t {
        None,
        Pending,
        Final,
    };

    enum class Option : uint8_t {
        SelectInsertedText = 1 << 0,
        AddsToKillRing = 1 << 1,
        RetainAutocorrectionIndicator = 1 << 2,
        PreventSpellChecking = 1 << 3,
        SmartDelete = 1 << 4,
        IsAutocompletion = 1 << 5,
    };

    static void deleteSelection(Ref<Document>&&, OptionSet<Option> = { }, TextCompositionType = TextCompositionType::None);
    static void deleteKeyPressed(Ref<Document>&&, OptionSet<Option> = { }, TextGranularity = TextGranularity::CharacterGranularity);
    static void forwardDeleteKeyPressed(Ref<Document>&&, OptionSet<Option> = { }, TextGranularity = TextGranularity::CharacterGranularity);
    static void insertText(Ref<Document>&&, const String&, OptionSet<Option>, TextCompositionType = TextCompositionType::None);
    static void insertText(Ref<Document>&&, const String&, const VisibleSelection&, OptionSet<Option>, TextCompositionType = TextCompositionType::None);
    static void insertLineBreak(Ref<Document>&&, OptionSet<Option>);
    static void insertParagraphSeparator(Ref<Document>&&, OptionSet<Option>);
    static void insertParagraphSeparatorInQuotedContent(Ref<Document>&&);
    static void closeTyping(Document&);
#if PLATFORM(IOS_FAMILY)
    static void ensureLastEditCommandHasCurrentSelectionIfOpenForMoreTyping(Document&, const VisibleSelection&);
#endif

    void insertText(const String &text, bool selectInsertedText);
    void insertTextRunWithoutNewlines(const String &text, bool selectInsertedText);
    void insertLineBreak();
    void insertParagraphSeparatorInQuotedContent();
    void insertParagraphSeparator();
    void deleteKeyPressed(TextGranularity, bool shouldAddToKillRing);
    void forwardDeleteKeyPressed(TextGranularity, bool shouldAddToKillRing);
    void deleteSelection(bool smartDelete);
    void setCompositionType(TextCompositionType type) { m_compositionType = type; }
    void setIsAutocompletion(bool isAutocompletion) { m_isAutocompletion = isAutocompletion; }

#if PLATFORM(IOS_FAMILY)
    void setEndingSelectionOnLastInsertCommand(const VisibleSelection& selection);
#endif

private:
    static Ref<TypingCommand> create(Ref<Document>&& document, Type command, const String& text = emptyString(), OptionSet<Option> options = { }, TextGranularity granularity = TextGranularity::CharacterGranularity, TextCompositionType compositionType = TextCompositionType::None)
    {
        return adoptRef(*new TypingCommand(WTFMove(document), command, text, options, granularity, compositionType));
    }

    static Ref<TypingCommand> create(Ref<Document>&& document, Type command, const String& text, OptionSet<Option> options, TextCompositionType compositionType)
    {
        return adoptRef(*new TypingCommand(WTFMove(document), command, text, options, TextGranularity::CharacterGranularity, compositionType));
    }

    TypingCommand(Ref<Document>&&, Type, const String& text, OptionSet<Option>, TextGranularity, TextCompositionType);

    void setSmartDelete(bool smartDelete) { m_smartDelete = smartDelete; }
    bool isOpenForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    static RefPtr<TypingCommand> lastTypingCommandIfStillOpenForTyping(Document&);

    void doApply() final;
    bool isTypingCommand() const final;
    bool preservesTypingStyle() const final { return m_preservesTypingStyle; }
    bool shouldRetainAutocorrectionIndicator() const final
    {
        ASSERT(isTopLevelCommand());
        return m_shouldRetainAutocorrectionIndicator;
    }
    void setShouldRetainAutocorrectionIndicator(bool retain) final { m_shouldRetainAutocorrectionIndicator = retain; }
    bool shouldStopCaretBlinking() const final { return true; }
    void setShouldPreventSpellChecking(bool prevent) { m_shouldPreventSpellChecking = prevent; }

    AtomString inputEventTypeName() const final;
    bool isInputMethodComposing() const final;
    String inputEventData() const final;
    RefPtr<DataTransfer> inputEventDataTransfer() const final;
    bool isBeforeInputEventCancelable() const final;

    static void updateSelectionIfDifferentFromCurrentSelection(TypingCommand*, Document&);

    void updatePreservesTypingStyle(Type);
    bool willAddTypingToOpenCommand(Type, TextGranularity, const String& = emptyString(), const std::optional<SimpleRange>& = { });
    void markMisspellingsAfterTyping(Type);
    void typingAddedToOpenCommand(Type);
    bool makeEditableRootEmpty();

    void postTextStateChangeNotificationForDeletion(const VisibleSelection&);
    void insertTextAndNotifyAccessibility(const String &text, bool selectInsertedText);
    void insertLineBreakAndNotifyAccessibility();
    void insertParagraphSeparatorInQuotedContentAndNotifyAccessibility();
    void insertParagraphSeparatorAndNotifyAccessibility();

    bool willApplyCommand() final;
    void didApplyCommand() final;

    bool shouldDeferWillApplyCommandUntilAddingTypingCommand() const;

    Type m_commandType;
    EditAction m_currentTypingEditAction;
    String m_textToInsert;
    String m_currentTextToInsert;
    bool m_openForMoreTyping;
    bool m_selectInsertedText;
    bool m_smartDelete;
    bool m_isHandlingInitialTypingCommand { true };
    TextGranularity m_granularity;
    TextCompositionType m_compositionType;
    bool m_shouldAddToKillRing;
    bool m_preservesTypingStyle;
    bool m_isAutocompletion;
    
    // Undoing a series of backward deletes will restore a selection around all of the
    // characters that were deleted, but only if the typing command being undone
    // was opened with a backward delete.
    bool m_openedByBackwardDelete;

    bool m_shouldRetainAutocorrectionIndicator;
    bool m_shouldPreventSpellChecking;
};

} // namespace WebCore
