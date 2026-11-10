/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebPage.h"
#include <WebCore/EditorClient.h>
#include <WebCore/TextCheckerClient.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
#if ENABLE(ATTACHMENT_ELEMENT)
enum class AttachmentAssociatedElementType : uint8_t;
#endif
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
}

namespace WebKit {

class WebPage;

class WebEditorClient final : public WebCore::EditorClient, public WebCore::TextCheckerClient {
    WTF_MAKE_TZONE_ALLOCATED(WebEditorClient);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(WebEditorClient);
public:
    WebEditorClient(WebPage& page)
        : m_page(page)
    {
    }

private:
    bool shouldDeleteRange(const std::optional<WebCore::SimpleRange>&) final;
    bool smartInsertDeleteEnabled() final;
    bool isSelectTrailingWhitespaceEnabled() const final;
    bool isContinuousSpellCheckingEnabled() final;
    void toggleContinuousSpellChecking() final;
    bool isGrammarCheckingEnabled() final;
    void toggleGrammarChecking() final;
    int spellCheckerDocumentTag() final;
    
    bool shouldBeginEditing(const WebCore::SimpleRange&) final;
    bool shouldEndEditing(const WebCore::SimpleRange&) final;
    bool shouldInsertNode(WebCore::Node&, const std::optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldInsertText(StringImpl*, const std::optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldChangeSelectedRange(const std::optional<WebCore::SimpleRange>& fromRange, const std::optional<WebCore::SimpleRange>& toRange, WebCore::Affinity, bool stillSelecting) final;
    
    bool shouldApplyStyle(const WebCore::StyleProperties&, const std::optional<WebCore::SimpleRange>&) final;
    void didApplyStyle() final;
    bool shouldMoveRangeAfterDelete(const WebCore::SimpleRange&, const WebCore::SimpleRange&) final;

#if ENABLE(ATTACHMENT_ELEMENT)
    void registerAttachmentIdentifier(StringImpl*, StringImpl* contentType, StringImpl* preferredFileName, Ref<WebCore::FragmentedSharedBuffer>&&) final;
    void registerAttachmentIdentifier(StringImpl*, StringImpl* contentType, StringImpl* filePath) final;
    void registerAttachmentIdentifier(StringImpl*) final;
    void registerAttachments(Vector<WebCore::SerializedAttachmentData>&&) final;
    void cloneAttachmentData(StringImpl* fromIdentifier, StringImpl* toIdentifier) final;
    void didInsertAttachmentWithIdentifier(StringImpl* identifier, StringImpl* source, WebCore::AttachmentAssociatedElementType) final;
    void didRemoveAttachmentWithIdentifier(StringImpl* identifier) final;
    bool supportsClientSideAttachmentData() const final { return true; }
    Vector<WebCore::SerializedAttachmentData> serializedAttachmentDataForIdentifiers(const Vector<RefPtr<StringImpl>>&) final;
#endif

    void didBeginEditing() final;
    void respondToChangedContents() final;
    void respondToChangedSelection(WebCore::LocalFrame*) final;
    void didEndUserTriggeredSelectionChanges() final;
    void updateEditorStateAfterLayoutIfEditabilityChanged() final;
    void discardedComposition(const WebCore::Document&) final;
    void canceledComposition() final;
    void didUpdateComposition() final;
    void didEndEditing() final;
    void willWriteSelectionToPasteboard(const std::optional<WebCore::SimpleRange>&) final;
    void didWriteSelectionToPasteboard() final;
    void getClientPasteboardData(const std::optional<WebCore::SimpleRange>&, Vector<std::pair<RefPtr<StringImpl>, RefPtr<WebCore::SharedBuffer>>>& pasteboardTypesAndData) final;
    
    void registerUndoStep(WebCore::UndoStep&) final;
    void registerRedoStep(WebCore::UndoStep&) final;
    void clearUndoRedoOperations() final;

    WebCore::DOMPasteAccessResponse requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::FrameIdentifier, StringImpl* originIdentifier) final;

    bool canCopyCut(WebCore::LocalFrame*, bool defaultValue) const final;
    bool canPaste(WebCore::LocalFrame*, bool defaultValue) const final;
    bool canUndo() const final;
    bool canRedo() const final;
    
    void undo() final;
    void redo() final;

    void handleKeyboardEvent(WebCore::KeyboardEvent&) final;
    void handleInputMethodKeydown(WebCore::KeyboardEvent&) final;
    
    void textFieldDidBeginEditing(WebCore::Element&) final;
    void textFieldDidEndEditing(WebCore::Element&) final;
    void textDidChangeInTextField(WebCore::Element&) final;
    bool doTextFieldCommandFromEvent(WebCore::Element&, WebCore::KeyboardEvent*) final;
    void textWillBeDeletedInTextField(WebCore::Element&) final;
    void textDidChangeInTextArea(WebCore::Element&) final;
    void overflowScrollPositionChanged() final;
    void subFrameScrollPositionChanged() final;

#if PLATFORM(COCOA)
    void setInsertionPasteboard(StringImpl* pasteboardName) final;
    HashMap<WebCore::FrameIdentifier, WebCore::AttributedString> collectAttributedStringsForRemoteFrames(WebCore::FrameIdentifier rootFrameIdentifier, const Vector<WebCore::FrameIdentifier>&) final;
#endif

#if USE(APPKIT)
    void uppercaseWord() final;
    void lowercaseWord() final;
    void capitalizeWord() final;
    bool canApplyCaseTransformations(StringImpl*) final;
    bool canConvertToTraditionalChinese(StringImpl*) final;
    bool canConvertToSimplifiedChinese(StringImpl*) final;
    void convertToTraditionalChinese() final;
    void convertToSimplifiedChinese() final;
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel(bool show) final;
    bool substitutionsPanelIsShowing() final;
    void toggleSmartInsertDelete() final;
    bool isAutomaticQuoteSubstitutionEnabled() final;
    void toggleAutomaticQuoteSubstitution() final;
    bool isAutomaticLinkDetectionEnabled() final;
    void toggleAutomaticLinkDetection() final;
    bool isAutomaticDashSubstitutionEnabled() final;
    bool isSmartListsEnabled() final;
    void toggleSmartLists() final;
    void toggleAutomaticDashSubstitution() final;
    bool isAutomaticTextReplacementEnabled() final;
    void toggleAutomaticTextReplacement() final;
    bool isAutomaticSpellingCorrectionEnabled() final;
    void toggleAutomaticSpellingCorrection() final;
#endif

#if PLATFORM(GTK)
    bool executePendingEditorCommands(WebCore::LocalFrame&, const Vector<RefPtr<StringImpl>>&, bool);
    bool handleGtkEditorCommand(WebCore::LocalFrame&, StringImpl* command, bool);
    void getEditorCommandsForKeyEvent(const WebCore::KeyboardEvent*, Vector<RefPtr<StringImpl>>&);
    void updateGlobalSelection(WebCore::LocalFrame*);
#endif

    TextCheckerClient* textChecker() final { return this; }

    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const final;
    void ignoreWordInSpellDocument(StringImpl*) final;
    void learnWord(StringImpl*) final;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) final;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) final;

#if USE(UNIFIED_TEXT_CHECKING)
    Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<WebCore::TextCheckingType> checkingTypes, const WebCore::VisibleSelection& currentSelection) final;
#endif

    void updateSpellingUIWithGrammarString(StringImpl*, const WebCore::GrammarDetail&) final;
    void updateSpellingUIWithMisspelledWord(StringImpl*) final;
    void showSpellingUI(bool show) final;
    bool spellingUIIsShowing() final;
    void getGuessesForWord(StringImpl* word, StringImpl* context, const WebCore::VisibleSelection& currentSelection, Vector<RefPtr<StringImpl>>& guesses) final;
    void setInputMethodState(WebCore::Element*) final;
    void requestCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection& currentSelection) final;
    void requestExtendedCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection& currentSelection) final;

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() final;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(MAC)
    void didDispatchInputMethodKeydown(WebCore::KeyboardEvent&) final;
#endif

#if PLATFORM(COCOA)
    bool shouldAllowSingleClickToChangeSelection(WebCore::Node&, const WebCore::VisibleSelection&, WebCore::MouseEventInputSource) const final;
#endif

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final;
    void stopDelayingAndCoalescingContentChangeNotifications() final;
    bool hasRichlyEditableSelection() final;
    int getPasteboardItemsCount() final;
    RefPtr<WebCore::DocumentFragment> documentFragmentFromDelegate(int index) final;
    bool performsTwoStepPaste(WebCore::DocumentFragment*) final;
    void updateStringForFind(StringImpl*) final;
    bool shouldRevealCurrentSelectionAfterInsertion() const final;
    bool shouldSuppressPasswordEcho() const final;
    bool shouldRemoveDictationAlternativesAfterEditing() const final;
#endif

    void willChangeSelectionForAccessibility() final;
    void didChangeSelectionForAccessibility() final;

    bool performTwoStepDrop(WebCore::DocumentFragment&, const WebCore::SimpleRange&, bool isMove) final;
    bool supportsGlobalSelection() final;

#if PLATFORM(IOS_FAMILY)
    bool shouldDrawVisuallyContiguousBidiSelection() const final;
#endif

    const WeakPtr<WebPage> m_page;
};

} // namespace WebKit
