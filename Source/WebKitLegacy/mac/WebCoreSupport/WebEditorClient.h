/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebCore/DOMPasteAccess.h>
#import <WebCore/EditorClient.h>
#import <WebCore/TextCheckerClient.h>
#import <WebCore/VisibleSelection.h>
#import <wtf/Forward.h>
#import <wtf/Ref.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/StringView.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKAppKitStubs.h>
#endif

@class WebView;
@class WebEditorUndoTarget;

class WebEditorClient final : public WebCore::EditorClient, public WebCore::TextCheckerClient, public CanMakeWeakPtr<WebEditorClient> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebEditorClient(WebView *);
    virtual ~WebEditorClient();

    void didCheckSucceed(WebCore::TextCheckingRequestIdentifier, NSArray *results);

private:
    bool isGrammarCheckingEnabled() final;
    void toggleGrammarChecking() final;
    bool isContinuousSpellCheckingEnabled() final;
    void toggleContinuousSpellChecking() final;
    int spellCheckerDocumentTag() final;

    bool smartInsertDeleteEnabled() final;
    bool isSelectTrailingWhitespaceEnabled() const final;

    bool shouldDeleteRange(const Optional<WebCore::SimpleRange>&) final;

    bool shouldBeginEditing(const WebCore::SimpleRange&) final;
    bool shouldEndEditing(const WebCore::SimpleRange&) final;
    bool shouldInsertNode(WebCore::Node&, const Optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldInsertText(const String&, const Optional<WebCore::SimpleRange>&, WebCore::EditorInsertAction) final;
    bool shouldChangeSelectedRange(const Optional<WebCore::SimpleRange>& fromRange, const Optional<WebCore::SimpleRange>& toRange, WebCore::EAffinity, bool stillSelecting) final;

    bool shouldApplyStyle(const WebCore::StyleProperties&, const Optional<WebCore::SimpleRange>&) final;
    void didApplyStyle() final;

    bool shouldMoveRangeAfterDelete(const WebCore::SimpleRange&, const WebCore::SimpleRange& rangeToBeReplaced) final;

    void didBeginEditing() final;
    void didEndEditing() final;
    void willWriteSelectionToPasteboard(const Optional<WebCore::SimpleRange>&) final;
    void didWriteSelectionToPasteboard() final;
    void getClientPasteboardData(const Optional<WebCore::SimpleRange>&, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) final;

    void setInsertionPasteboard(const String&) final;
    WebCore::DOMPasteAccessResponse requestDOMPasteAccess(const String&) final { return WebCore::DOMPasteAccessResponse::DeniedForGesture; }

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

    TextCheckerClient* textChecker() final { return this; }

    void respondToChangedContents() final;
    void respondToChangedSelection(WebCore::Frame*) final;
    void didEndUserTriggeredSelectionChanges() final { }
    void updateEditorStateAfterLayoutIfEditabilityChanged() final;
    void discardedComposition(WebCore::Frame*) final;
    void canceledComposition() final;
    void didUpdateComposition() final { }

    void registerUndoStep(WebCore::UndoStep&) final;
    void registerRedoStep(WebCore::UndoStep&) final;
    void clearUndoRedoOperations() final;

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
    void overflowScrollPositionChanged() final { };
    void subFrameScrollPositionChanged() final { };

#if PLATFORM(IOS_FAMILY)
    void startDelayingAndCoalescingContentChangeNotifications() final;
    void stopDelayingAndCoalescingContentChangeNotifications() final;
    bool hasRichlyEditableSelection() final;
    int getPasteboardItemsCount() final;
    RefPtr<WebCore::DocumentFragment> documentFragmentFromDelegate(int index) final;
    bool performsTwoStepPaste(WebCore::DocumentFragment*) final;
    void updateStringForFind(const String&) final { }
    bool shouldRevealCurrentSelectionAfterInsertion() const final;
    bool shouldSuppressPasswordEcho() const final;
#endif

    bool performTwoStepDrop(WebCore::DocumentFragment&, const WebCore::SimpleRange& destination, bool isMove) final;
    
    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const final;
    void ignoreWordInSpellDocument(const String&) final;
    void learnWord(const String&) final;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) final;
    String getAutoCorrectSuggestionForMisspelledWord(const String&) final;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) final;
    Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, OptionSet<WebCore::TextCheckingType> checkingTypes, const WebCore::VisibleSelection& currentSelection) final;
    void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) final;
    void updateSpellingUIWithMisspelledWord(const String&) final;
    void showSpellingUI(bool show) final;
    bool spellingUIIsShowing() final;
    void getGuessesForWord(const String& word, const String& context, const WebCore::VisibleSelection& currentSelection, Vector<String>& guesses) final;

    void willSetInputMethodState() final;
    void setInputMethodState(WebCore::Element*) final;
    void requestCheckingOfString(WebCore::TextCheckingRequest&, const WebCore::VisibleSelection& currentSelection) final;

#if PLATFORM(MAC)
    void requestCandidatesForSelection(const WebCore::VisibleSelection&) final;
    void handleRequestedCandidates(NSInteger, NSArray<NSTextCheckingResult *> *);
    void handleAcceptedCandidateWithSoftSpaces(WebCore::TextCheckingResult) final;
#endif

    void registerUndoOrRedoStep(WebCore::UndoStep&, bool isRedo);

#if PLATFORM(IOS_FAMILY)
    bool shouldAllowSingleClickToChangeSelection(WebCore::Node& targetNode, const WebCore::VisibleSelection& newSelection) const;
#endif

    bool canShowFontPanel() const final
    {
#if PLATFORM(MAC)
        return true;
#else
        return false;
#endif
    }

    WebView *m_webView;
    RetainPtr<WebEditorUndoTarget> m_undoTarget;
    bool m_haveUndoRedoOperations { false };
    
    HashMap<WebCore::TextCheckingRequestIdentifier, Ref<WebCore::TextCheckingRequest>> m_requestsInFlight;

#if PLATFORM(IOS_FAMILY)
    bool m_delayingContentChangeNotifications { false };
    bool m_hasDelayedContentChangeNotification { false };
#endif

    WebCore::VisibleSelection m_lastSelectionForRequestedCandidates;

#if PLATFORM(MAC)
    RetainPtr<NSString> m_paragraphContextForCandidateRequest;
    NSRange m_rangeForCandidates;
    NSInteger m_lastCandidateRequestSequenceNumber;
#endif

    enum class EditorStateIsContentEditable { No, Yes, Unset };
    EditorStateIsContentEditable m_lastEditorStateWasContentEditable { EditorStateIsContentEditable::Unset };
};

inline NSSelectionAffinity kit(WebCore::EAffinity affinity)
{
    switch (affinity) {
    case WebCore::Affinity::Upstream:
        return NSSelectionAffinityUpstream;
    case WebCore::Affinity::Downstream:
        return NSSelectionAffinityDownstream;
    }
    ASSERT_NOT_REACHED();
    return NSSelectionAffinityDownstream;
}

inline WebCore::EAffinity core(NSSelectionAffinity affinity)
{
    switch (affinity) {
    case NSSelectionAffinityUpstream:
        return WebCore::Affinity::Upstream;
    case NSSelectionAffinityDownstream:
        return WebCore::Affinity::Downstream;
    }
    ASSERT_NOT_REACHED();
    return WebCore::Affinity::Downstream;
}

#if PLATFORM(IOS_FAMILY)

inline bool WebEditorClient::isGrammarCheckingEnabled()
{
    return false;
}

inline void WebEditorClient::toggleGrammarChecking()
{
}

inline int WebEditorClient::spellCheckerDocumentTag()
{
    return 0;
}

inline bool WebEditorClient::shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const
{
    return true;
}

inline void WebEditorClient::ignoreWordInSpellDocument(const String&)
{
}

inline void WebEditorClient::learnWord(const String&)
{
}

inline void WebEditorClient::checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength)
{
}

inline String WebEditorClient::getAutoCorrectSuggestionForMisspelledWord(const String&)
{
    return "";
}

inline void WebEditorClient::checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength)
{
}

inline void WebEditorClient::updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&)
{
}

inline void WebEditorClient::updateSpellingUIWithMisspelledWord(const String&)
{
}

inline void WebEditorClient::showSpellingUI(bool show)
{
}

inline bool WebEditorClient::spellingUIIsShowing()
{
    return false;
}

inline void WebEditorClient::getGuessesForWord(const String&, const String&, const WebCore::VisibleSelection&, Vector<String>&)
{
}

#endif
