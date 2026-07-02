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
    bool NODELETE smartInsertDeleteEnabled() final;
    bool NODELETE isSelectTrailingWhitespaceEnabled() const final;
    bool isContinuousSpellCheckingEnabled() final;
    void NODELETE toggleContinuousSpellChecking() final;
    bool isGrammarCheckingEnabled() final;
    void NODELETE toggleGrammarChecking() final;
    int NODELETE spellCheckerDocumentTag() final;
    
    bool shouldBeginEditing(const WebCore::SimpleRange&) final;
    bool shouldEndEditing(const WebCore::SimpleRange&) final;
    bool shouldInsertNode(WebCore::Node&, const std::optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldInsertText(const String&, const std::optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldChangeSelectedRange(const std::optional<WebCore::SimpleRange>& fromRange, const std::optional<WebCore::SimpleRange>& toRange, WebCore::Affinity, bool stillSelecting) final;
    
    bool shouldApplyStyle(const WebCore::StyleProperties&, const std::optional<WebCore::SimpleRange>&) final;
    void didApplyStyle() final;
    bool NODELETE shouldMoveRangeAfterDelete(const WebCore::SimpleRange&, const WebCore::SimpleRange&) final;

#if ENABLE(ATTACHMENT_ELEMENT)
    void registerAttachmentIdentifier(const String&, const String& contentType, const String& preferredFileName, Ref<WebCore::FragmentedSharedBuffer>&&) final;
    void registerAttachmentIdentifier(const String&, const String& contentType, const String& filePath) final;
    void registerAttachmentIdentifier(const String&) final;
    void registerAttachments(Vector<WebCore::SerializedAttachmentData>&&) final;
    void cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier) final;
    void didInsertAttachmentWithIdentifier(const String& identifier, const String& source, WebCore::AttachmentAssociatedElementType) final;
    void didRemoveAttachmentWithIdentifier(const String& identifier) final;
    bool supportsClientSideAttachmentData() const final { return true; }
    Vector<WebCore::SerializedAttachmentData> serializedAttachmentDataForIdentifiers(const Vector<String>&) final;
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
    void getClientPasteboardData(const std::optional<WebCore::SimpleRange>&, Vector<std::pair<String, RefPtr<WebCore::SharedBuffer>>>& pasteboardTypesAndData) final;
    
    void registerUndoStep(WebCore::UndoStep&) final;
    void NODELETE registerRedoStep(WebCore::UndoStep&) final;
    void clearUndoRedoOperations() final;

    WebCore::DOMPasteAccessResponse requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::FrameIdentifier, const String& originIdentifier) final;

    bool NODELETE canCopyCut(WebCore::LocalFrame*, bool defaultValue) const final;
    bool NODELETE canPaste(WebCore::LocalFrame*, bool defaultValue) const final;
    bool canUndo() const final;
    bool canRedo() const final;
    
    void undo() final;
    void redo() final;

    void NODELETE handleKeyboardEvent(WebCore::KeyboardEvent&) final;
    void NODELETE handleInputMethodKeydown(WebCore::KeyboardEvent&) final;
    
    void textFieldDidBeginEditing(WebCore::Element&) final;
    void textFieldDidEndEditing(WebCore::Element&) final;
    void textDidChangeInTextField(WebCore::Element&) final;
    bool doTextFieldCommandFromEvent(WebCore::Element&, WebCore::KeyboardEvent*) final;
    void textWillBeDeletedInTextField(WebCore::Element&) final;
    void textDidChangeInTextArea(WebCore::Element&) final;
    void NODELETE overflowScrollPositionChanged() final;
    void NODELETE subFrameScrollPositionChanged() final;

#if PLATFORM(COCOA)
    void NODELETE setInsertionPasteboard(const String& pasteboardName) final;
#endif

#if USE(APPKIT)
    void NODELETE uppercaseWord() final;
    void NODELETE lowercaseWord() final;
    void NODELETE capitalizeWord() final;
    bool NODELETE canApplyCaseTransformations(const String&) final;
    bool NODELETE canConvertToTraditionalChinese(const String&) final;
    bool NODELETE canConvertToSimplifiedChinese(const String&) final;
    void NODELETE convertToTraditionalChinese() final;
    void NODELETE convertToSimplifiedChinese() final;
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void NODELETE showSubstitutionsPanel(bool show) final;
    bool NODELETE substitutionsPanelIsShowing() final;
    void NODELETE toggleSmartInsertDelete() final;
    bool NODELETE isAutomaticQuoteSubstitutionEnabled() final;
    void NODELETE toggleAutomaticQuoteSubstitution() final;
    bool NODELETE isAutomaticLinkDetectionEnabled() final;
    void NODELETE toggleAutomaticLinkDetection() final;
    bool NODELETE isAutomaticDashSubstitutionEnabled() final;
    bool NODELETE isSmartListsEnabled() final;
    void NODELETE toggleSmartLists() final;
    void NODELETE toggleAutomaticDashSubstitution() final;
    bool NODELETE isAutomaticTextReplacementEnabled() final;
    void NODELETE toggleAutomaticTextReplacement() final;
    bool NODELETE isAutomaticSpellingCorrectionEnabled() final;
    void NODELETE toggleAutomaticSpellingCorrection() final;
#endif

#if PLATFORM(GTK)
    bool executePendingEditorCommands(WebCore::LocalFrame&, const Vector<WTF::String>&, bool);
    bool handleGtkEditorCommand(WebCore::LocalFrame&, const String& command, bool);
    void getEditorCommandsForKeyEvent(const WebCore::KeyboardEvent*, Vector<WTF::String>&);
    void updateGlobalSelection(WebCore::LocalFrame*);
#endif

    TextCheckerClient* textChecker() final { return this; }

    bool NODELETE shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const final;
    void ignoreWordInSpellDocument(const String&) final;
    void learnWord(const String&) final;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) final;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) final;

#if USE(UNIFIED_TEXT_CHECKING)
    Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<WebCore::TextCheckingType> checkingTypes, const WebCore::VisibleSelection& currentSelection) final;
#endif

    void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) final;
    void updateSpellingUIWithMisspelledWord(const String&) final;
    void NODELETE showSpellingUI(bool show) final;
    bool spellingUIIsShowing() final;
    void getGuessesForWord(const String& word, const String& context, const WebCore::VisibleSelection& currentSelection, Vector<String>& guesses) final;
    void NODELETE setInputMethodState(WebCore::Element*) final;
    void requestCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection& currentSelection) final;
    void requestExtendedCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection& currentSelection) final;

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() final;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(MAC)
    void NODELETE didDispatchInputMethodKeydown(WebCore::KeyboardEvent&) final;
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
    void updateStringForFind(const String&) final;
    bool shouldRevealCurrentSelectionAfterInsertion() const final;
    bool shouldSuppressPasswordEcho() const final;
    bool shouldRemoveDictationAlternativesAfterEditing() const final;
#endif

    void NODELETE willChangeSelectionForAccessibility() final;
    void NODELETE didChangeSelectionForAccessibility() final;

    bool performTwoStepDrop(WebCore::DocumentFragment&, const WebCore::SimpleRange&, bool isMove) final;
    bool NODELETE supportsGlobalSelection() final;

#if PLATFORM(IOS_FAMILY)
    bool shouldDrawVisuallyContiguousBidiSelection() const final;
#endif

    const WeakPtr<WebPage> m_page;
};

} // namespace WebKit
