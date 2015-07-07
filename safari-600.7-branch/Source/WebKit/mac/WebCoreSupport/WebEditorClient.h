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
#import <wtf/Forward.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/text/StringView.h>

@class WebView;
@class WebEditorUndoTarget;

class WebEditorClient : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    WebEditorClient(WebView *);
    virtual ~WebEditorClient();

    void didCheckSucceed(int sequence, NSArray *results);

private:
    virtual void pageDestroyed() override;

    virtual bool isGrammarCheckingEnabled() override;
    virtual void toggleGrammarChecking() override;
    virtual bool isContinuousSpellCheckingEnabled() override;
    virtual void toggleContinuousSpellChecking() override;
    virtual int spellCheckerDocumentTag() override;

    virtual bool smartInsertDeleteEnabled() override;
    virtual bool isSelectTrailingWhitespaceEnabled() override;

    virtual bool shouldDeleteRange(WebCore::Range*) override;

    virtual bool shouldBeginEditing(WebCore::Range*) override;
    virtual bool shouldEndEditing(WebCore::Range*) override;
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction) override;
    virtual bool shouldInsertText(const String&, WebCore::Range*, WebCore::EditorInsertAction) override;
    virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) override;

    virtual bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*) override;
    
    virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range* rangeToBeReplaced) override;

    virtual void didBeginEditing() override;
    virtual void didEndEditing() override;
    virtual void willWriteSelectionToPasteboard(WebCore::Range*) override;
    virtual void didWriteSelectionToPasteboard() override;
    virtual void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) override;

    virtual NSString *userVisibleString(NSURL *) override;
    virtual WebCore::DocumentFragment* documentFragmentFromAttributedString(NSAttributedString *, Vector< RefPtr<WebCore::ArchiveResource>>&) override;
    virtual void setInsertionPasteboard(const String&) override;
    virtual NSURL *canonicalizeURL(NSURL *) override;
    virtual NSURL *canonicalizeURLString(NSString *) override;
    
#if USE(APPKIT)
    virtual void uppercaseWord() override;
    virtual void lowercaseWord() override;
    virtual void capitalizeWord() override;
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    virtual void showSubstitutionsPanel(bool show) override;
    virtual bool substitutionsPanelIsShowing() override;
    virtual void toggleSmartInsertDelete() override;
    virtual bool isAutomaticQuoteSubstitutionEnabled() override;
    virtual void toggleAutomaticQuoteSubstitution() override;
    virtual bool isAutomaticLinkDetectionEnabled() override;
    virtual void toggleAutomaticLinkDetection() override;
    virtual bool isAutomaticDashSubstitutionEnabled() override;
    virtual void toggleAutomaticDashSubstitution() override;
    virtual bool isAutomaticTextReplacementEnabled() override;
    virtual void toggleAutomaticTextReplacement() override;
    virtual bool isAutomaticSpellingCorrectionEnabled() override;
    virtual void toggleAutomaticSpellingCorrection() override;
#endif

#if ENABLE(DELETION_UI)
    virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*) override;
#endif

    virtual TextCheckerClient* textChecker() override { return this; }

    virtual void respondToChangedContents() override;
    virtual void respondToChangedSelection(WebCore::Frame*) override;
    virtual void discardedComposition(WebCore::Frame*) override;

    virtual void registerUndoStep(PassRefPtr<WebCore::UndoStep>) override;
    virtual void registerRedoStep(PassRefPtr<WebCore::UndoStep>) override;
    virtual void clearUndoRedoOperations() override;

    virtual bool canCopyCut(WebCore::Frame*, bool defaultValue) const override;
    virtual bool canPaste(WebCore::Frame*, bool defaultValue) const override;
    virtual bool canUndo() const override;
    virtual bool canRedo() const override;
    
    virtual void undo() override;
    virtual void redo() override;
    
    virtual void handleKeyboardEvent(WebCore::KeyboardEvent*) override;
    virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*) override;

    virtual void textFieldDidBeginEditing(WebCore::Element*) override;
    virtual void textFieldDidEndEditing(WebCore::Element*) override;
    virtual void textDidChangeInTextField(WebCore::Element*) override;
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) override;
    virtual void textWillBeDeletedInTextField(WebCore::Element*) override;
    virtual void textDidChangeInTextArea(WebCore::Element*) override;
    virtual void overflowScrollPositionChanged() override { };

#if PLATFORM(IOS)
    virtual void startDelayingAndCoalescingContentChangeNotifications() override;
    virtual void stopDelayingAndCoalescingContentChangeNotifications() override;
    virtual void writeDataToPasteboard(NSDictionary*) override;
    virtual NSArray* supportedPasteboardTypesForCurrentSelection() override;
    virtual NSArray* readDataFromPasteboard(NSString* type, int index) override;
    virtual bool hasRichlyEditableSelection() override;
    virtual int getPasteboardItemsCount() override;
    virtual WebCore::DocumentFragment* documentFragmentFromDelegate(int index) override;
    virtual bool performsTwoStepPaste(WebCore::DocumentFragment*) override;
    virtual int pasteboardChangeCount() override;
#endif
    
    virtual bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const override;
    virtual void ignoreWordInSpellDocument(const String&) override;
    virtual void learnWord(const String&) override;
    virtual void checkSpellingOfString(StringView, int* misspellingLocation, int* misspellingLength) override;
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) override;
    virtual void checkGrammarOfString(StringView, Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) override;
    virtual Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, WebCore::TextCheckingTypeMask checkingTypes) override;
    virtual void updateSpellingUIWithGrammarString(const String&, const WebCore::GrammarDetail&) override;
    virtual void updateSpellingUIWithMisspelledWord(const String&) override;
    virtual void showSpellingUI(bool show) override;
    virtual bool spellingUIIsShowing() override;
    virtual void getGuessesForWord(const String& word, const String& context, Vector<String>& guesses) override;

    virtual void willSetInputMethodState() override;
    virtual void setInputMethodState(bool enabled) override;
    virtual void requestCheckingOfString(PassRefPtr<WebCore::TextCheckingRequest>) override;

    void registerUndoOrRedoStep(PassRefPtr<WebCore::UndoStep>, bool isRedo);

    WebView *m_webView;
    RetainPtr<WebEditorUndoTarget> m_undoTarget;
    bool m_haveUndoRedoOperations;
    RefPtr<WebCore::TextCheckingRequest> m_textCheckingRequest;
#if PLATFORM(IOS)
    bool m_delayingContentChangeNotifications;
    bool m_hasDelayedContentChangeNotification;
#endif
};

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

inline void WebEditorClient::getGuessesForWord(const String&, const String&, Vector<String>&)
{
}

#endif
