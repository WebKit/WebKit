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

#include <WebCore/EditorClient.h>
#include <WebCore/TextCheckerClient.h>

namespace WebCore {
enum class DOMPasteAccessResponse : uint8_t;
}

namespace WebKit {

class WebPage;

class WebEditorClient final : public WebCore::EditorClient, public WebCore::TextCheckerClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebEditorClient(WebPage* page)
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
    bool shouldInsertText(const String&, const std::optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldChangeSelectedRange(const std::optional<WebCore::SimpleRange>& fromRange, const std::optional<WebCore::SimpleRange>& toRange, WebCore::Affinity, bool stillSelecting) final;
    
    bool shouldApplyStyle(const WebCore::StyleProperties&, const std::optional<WebCore::SimpleRange>&) final;
    void didApplyStyle() final;
    bool shouldMoveRangeAfterDelete(const WebCore::SimpleRange&, const WebCore::SimpleRange&) final;

#if ENABLE(ATTACHMENT_ELEMENT)
    void registerAttachmentIdentifier(const String&, const String& contentType, const String& preferredFileName, Ref<WebCore::SharedBuffer>&&) final;
    void registerAttachmentIdentifier(const String&, const String& contentType, const String& filePath) final;
    void registerAttachmentIdentifier(const String&) final;
    void registerAttachments(Vector<WebCore::SerializedAttachmentData>&&) final;
    void cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier) final;
    void didInsertAttachmentWithIdentifier(const String& identifier, const String& source, bool hasEnclosingImage) final;
    void didRemoveAttachmentWithIdentifier(const String& identifier) final;
    bool supportsClientSideAttachmentData() const final { return true; }
    Vector<WebCore::SerializedAttachmentData> serializedAttachmentDataForIdentifiers(const Vector<String>&) final;
#endif

    void didBeginEditing() final;
    void respondToChangedContents() final;
    void respondToChangedSelection(WebCore::Frame*) final;
    void didEndUserTriggeredSelectionChanges() final;
    void updateEditorStateAfterLayoutIfEditabilityChanged() final;
    void discardedComposition(WebCore::Frame*) final;
    void canceledComposition() final;
    void didUpdateComposition() final;
    void didEndEditing() final;
    void willWriteSelectionToPasteboard(const std::optional<WebCore::SimpleRange>&) final;
    void didWriteSelectionToPasteboard() final;
    void getClientPasteboardData(const std::optional<WebCore::SimpleRange>&, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) final;
    
    void registerUndoStep(WebCore::UndoStep&) final;
    void registerRedoStep(WebCore::UndoStep&) final;
    void clearUndoRedoOperations() final;

    WebCore::DOMPasteAccessResponse requestDOMPasteAccess(const String& originIdentifier) final;

    bool canCopyCut(WebCore::Frame*, bool defaultValue) const final;
    bool canPaste(WebCore::Frame*, bool defaultValue) const final;
    bool canUndo() const final;
    bool canRedo() const final;
    
    void undo() final;
    void redo() final;

    void handleKeyboardEvent(WebCore::KeyboardEvent&) final;
    void handleInputMethodKeydown(WebCore::KeyboardEvent&) final;
    
    void textFieldDidBeginEditing(WebCore::Element*) final;
    void textFieldDidEndEditing(WebCore::Element*) final;
    void textDidChangeInTextField(WebCore::Element*) final;
    bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) final;
    void textWillBeDeletedInTextField(WebCore::Element*) final;
    void textDidChangeInTextArea(WebCore::Element*) final;
    void overflowScrollPositionChanged() final;
    void subFrameScrollPositionChanged() final;

#if PLATFORM(COCOA)
    void setInsertionPasteboard(const String& pasteboardName) final;
#endif

#if USE(APPKIT)
    void uppercaseWord() final;
    void lowercaseWord() final;
    void capitalizeWord() final;
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
    void toggleAutomaticDashSubstitution() final;
    bool isAutomaticTextReplacementEnabled() final;
    void toggleAutomaticTextReplacement() final;
    bool isAutomaticSpellingCorrectionEnabled() final;
    void toggleAutomaticSpellingCorrection() final;
#endif

#if PLATFORM(GTK)
    bool executePendingEditorCommands(WebCore::Frame&, const Vector<WTF::String>&, bool);
    bool handleGtkEditorCommand(WebCore::Frame&, const String& command, bool);
    void getEditorCommandsForKeyEvent(const WebCore::KeyboardEvent*, Vector<WTF::String>&);
    void updateGlobalSelection(WebCore::Frame*);
#endif

    TextCheckerClient* textChecker() final { return this; }

    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const final;
    void ignoreWordInSpellDocument(const String&) final;
    void learnWord(const String&) final;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) final;
    String getAutoCorrectSuggestionForMisspelledWord(const String& misspelledWord) final;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) final;

#if USE(UNIFIED_TEXT_CHECKING)
    Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<WebCore::TextCheckingType> checkingTypes, const WebCore::VisibleSelection& currentSelection) final;
#endif

    void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) final;
    void updateSpellingUIWithMisspelledWord(const String&) final;
    void showSpellingUI(bool show) final;
    bool spellingUIIsShowing() final;
    void getGuessesForWord(const String& word, const String& context, const WebCore::VisibleSelection& currentSelection, Vector<String>& guesses) final;
    void willSetInputMethodState() final;
    void setInputMethodState(WebCore::Element*) final;
    void requestCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection& currentSelection) final;

#if PLATFORM(GTK)
    bool shouldShowUnicodeMenu() final;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    void didDispatchInputMethodKeydown(WebCore::KeyboardEvent&) final;
#endif

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final;
    void stopDelayingAndCoalescingContentChangeNotifications() final;
    bool hasRichlyEditableSelection() final;
    int getPasteboardItemsCount() final;
    RefPtr<WebCore::DocumentFragment> documentFragmentFromDelegate(int index) final;
    bool performsTwoStepPaste(WebCore::DocumentFragment*) final;
    void updateStringForFind(const String&) final;
    bool shouldAllowSingleClickToChangeSelection(WebCore::Node&, const WebCore::VisibleSelection&) const final;
    bool shouldRevealCurrentSelectionAfterInsertion() const final;
    bool shouldSuppressPasswordEcho() const final;
#endif

    void willChangeSelectionForAccessibility() final;
    void didChangeSelectionForAccessibility() final;

    bool performTwoStepDrop(WebCore::DocumentFragment&, const WebCore::SimpleRange&, bool isMove) final;
    bool supportsGlobalSelection() final;

    bool canShowFontPanel() const final
    {
        // FIXME: Support for showing the system font panel (as well as other font styling controls) is
        // tracked in <rdar://problem/21577518>.
        return false;
    }

    WebPage* m_page;
};

} // namespace WebKit
