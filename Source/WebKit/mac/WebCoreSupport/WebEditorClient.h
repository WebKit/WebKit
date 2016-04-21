/*
 * Copyright (C) 2006, 2014 Apple Inc. All rights reserved.
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

#import <WebCore/EditorClient.h>
#import <WebCore/TextCheckerClient.h>
#import <WebCore/VisibleSelection.h>
#import <wtf/Forward.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/WeakPtr.h>
#import <wtf/text/StringView.h>

#if PLATFORM(IOS)
#import <WebCore/WAKAppKitStubs.h>
#endif

@class WebView;
@class WebEditorUndoTarget;

class WebEditorClient : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    WebEditorClient(WebView *);
    virtual ~WebEditorClient();

    void didCheckSucceed(int sequence, NSArray *results);

private:
    void pageDestroyed() override;

    bool isGrammarCheckingEnabled() override;
    void toggleGrammarChecking() override;
    bool isContinuousSpellCheckingEnabled() override;
    void toggleContinuousSpellChecking() override;
    int spellCheckerDocumentTag() override;

    bool smartInsertDeleteEnabled() override;
    bool isSelectTrailingWhitespaceEnabled() override;

    bool shouldDeleteRange(WebCore::Range*) override;

    bool shouldBeginEditing(WebCore::Range*) override;
    bool shouldEndEditing(WebCore::Range*) override;
    bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction) override;
    bool shouldInsertText(const String&, WebCore::Range*, WebCore::EditorInsertAction) override;
    bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) override;

    bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*) override;
    void didApplyStyle() override;

    bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range* rangeToBeReplaced) override;

    void didBeginEditing() override;
    void didEndEditing() override;
    void willWriteSelectionToPasteboard(WebCore::Range*) override;
    void didWriteSelectionToPasteboard() override;
    void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) override;

    NSString *userVisibleString(NSURL *) override;
    WebCore::DocumentFragment* documentFragmentFromAttributedString(NSAttributedString *, Vector< RefPtr<WebCore::ArchiveResource>>&) override;
    void setInsertionPasteboard(const String&) override;
    NSURL *canonicalizeURL(NSURL *) override;
    NSURL *canonicalizeURLString(NSString *) override;
    
#if USE(APPKIT)
    void uppercaseWord() override;
    void lowercaseWord() override;
    void capitalizeWord() override;
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void showSubstitutionsPanel(bool show) override;
    bool substitutionsPanelIsShowing() override;
    void toggleSmartInsertDelete() override;
    bool isAutomaticQuoteSubstitutionEnabled() override;
    void toggleAutomaticQuoteSubstitution() override;
    bool isAutomaticLinkDetectionEnabled() override;
    void toggleAutomaticLinkDetection() override;
    bool isAutomaticDashSubstitutionEnabled() override;
    void toggleAutomaticDashSubstitution() override;
    bool isAutomaticTextReplacementEnabled() override;
    void toggleAutomaticTextReplacement() override;
    bool isAutomaticSpellingCorrectionEnabled() override;
    void toggleAutomaticSpellingCorrection() override;
#endif

    TextCheckerClient* textChecker() override { return this; }

    void respondToChangedContents() override;
    void respondToChangedSelection(WebCore::Frame*) override;
    void didChangeSelectionAndUpdateLayout() override { }
    void discardedComposition(WebCore::Frame*) override;

    void registerUndoStep(PassRefPtr<WebCore::UndoStep>) override;
    void registerRedoStep(PassRefPtr<WebCore::UndoStep>) override;
    void clearUndoRedoOperations() override;

    bool canCopyCut(WebCore::Frame*, bool defaultValue) const override;
    bool canPaste(WebCore::Frame*, bool defaultValue) const override;
    bool canUndo() const override;
    bool canRedo() const override;
    
    void undo() override;
    void redo() override;
    
    void handleKeyboardEvent(WebCore::KeyboardEvent*) override;
    void handleInputMethodKeydown(WebCore::KeyboardEvent*) override;

    void textFieldDidBeginEditing(WebCore::Element*) override;
    void textFieldDidEndEditing(WebCore::Element*) override;
    void textDidChangeInTextField(WebCore::Element*) override;
    bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) override;
    void textWillBeDeletedInTextField(WebCore::Element*) override;
    void textDidChangeInTextArea(WebCore::Element*) override;
    void overflowScrollPositionChanged() override { };

#if PLATFORM(IOS)
    void startDelayingAndCoalescingContentChangeNotifications() override;
    void stopDelayingAndCoalescingContentChangeNotifications() override;
    void writeDataToPasteboard(NSDictionary*) override;
    NSArray* supportedPasteboardTypesForCurrentSelection() override;
    NSArray* readDataFromPasteboard(NSString* type, int index) override;
    bool hasRichlyEditableSelection() override;
    int getPasteboardItemsCount() override;
    WebCore::DocumentFragment* documentFragmentFromDelegate(int index) override;
    bool performsTwoStepPaste(WebCore::DocumentFragment*) override;
    int pasteboardChangeCount() override;
#endif
    
    bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const override;
    void ignoreWordInSpellDocument(const String&) override;
    void learnWord(const String&) override;
    void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) override;
    String getAutoCorrectSuggestionForMisspelledWord(const String&) override;
    void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) override;
    Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, WebCore::TextCheckingTypeMask checkingTypes, const WebCore::VisibleSelection& currentSelection) override;
    void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) override;
    void updateSpellingUIWithMisspelledWord(const String&) override;
    void showSpellingUI(bool show) override;
    bool spellingUIIsShowing() override;
    void getGuessesForWord(const String& word, const String& context, const WebCore::VisibleSelection& currentSelection, Vector<String>& guesses) override;

    void willSetInputMethodState() override;
    void setInputMethodState(bool enabled) override;
    void requestCheckingOfString(PassRefPtr<WebCore::TextCheckingRequest>, const WebCore::VisibleSelection& currentSelection) override;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    void requestCandidatesForSelection(const WebCore::VisibleSelection&) override;
    void handleRequestedCandidates(NSInteger, NSArray<NSTextCheckingResult *> *);
    void handleAcceptedCandidate(NSTextCheckingResult *);
#endif

    void registerUndoOrRedoStep(PassRefPtr<WebCore::UndoStep>, bool isRedo);

    WebView *m_webView;
    RetainPtr<WebEditorUndoTarget> m_undoTarget;
    bool m_haveUndoRedoOperations;
    RefPtr<WebCore::TextCheckingRequest> m_textCheckingRequest;
#if PLATFORM(IOS)
    bool m_delayingContentChangeNotifications;
    bool m_hasDelayedContentChangeNotification;
#endif

    WebCore::VisibleSelection m_lastSelectionForRequestedCandidates;
#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200
    RetainPtr<NSString> m_paragraphContextForCandidateRequest;
    NSRange m_rangeForCandidates;
#endif

    WeakPtrFactory<WebEditorClient> m_weakPtrFactory;
};

#if PLATFORM(COCOA)
inline NSSelectionAffinity kit(WebCore::EAffinity affinity)
{
    switch (affinity) {
        case WebCore::EAffinity::UPSTREAM:
            return NSSelectionAffinityUpstream;
        case WebCore::EAffinity::DOWNSTREAM:
            return NSSelectionAffinityDownstream;
    }
    ASSERT_NOT_REACHED();
    return NSSelectionAffinityUpstream;
}

inline WebCore::EAffinity core(NSSelectionAffinity affinity)
{
    switch (affinity) {
    case NSSelectionAffinityUpstream:
        return WebCore::EAffinity::UPSTREAM;
    case NSSelectionAffinityDownstream:
        return WebCore::EAffinity::DOWNSTREAM;
    }
    ASSERT_NOT_REACHED();
    return WebCore::EAffinity::UPSTREAM;
}
#endif

#if PLATFORM(IOS)

inline bool WebEditorClient::isGrammarCheckingEnabled()
{
    return false;
}

inline void WebEditorClient::toggleGrammarChecking()
{
}

inline void WebEditorClient::toggleContinuousSpellChecking()
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
