/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

@class WebView;
@class WebEditorUndoTarget;

class WebEditorClient : public WebCore::EditorClient, public WebCore::TextCheckerClient {
public:
    WebEditorClient(WebView *);
    virtual ~WebEditorClient();
    virtual void pageDestroyed() OVERRIDE;

#if !PLATFORM(IOS)
    virtual bool isGrammarCheckingEnabled() OVERRIDE;
    virtual void toggleGrammarChecking() OVERRIDE;
    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE;
    virtual void toggleContinuousSpellChecking() OVERRIDE;
    virtual int spellCheckerDocumentTag() OVERRIDE;
#else
    virtual bool isGrammarCheckingEnabled() OVERRIDE { return false; }
    virtual void toggleGrammarChecking() OVERRIDE { }
    // Note: isContinuousSpellCheckingEnabled() is implemented.
    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE;
    virtual void toggleContinuousSpellChecking() OVERRIDE { }
    virtual int spellCheckerDocumentTag() OVERRIDE { return 0; }
#endif

    virtual bool smartInsertDeleteEnabled() OVERRIDE;
    virtual bool isSelectTrailingWhitespaceEnabled() OVERRIDE;

    virtual bool shouldDeleteRange(WebCore::Range*) OVERRIDE;

    virtual bool shouldBeginEditing(WebCore::Range*) OVERRIDE;
    virtual bool shouldEndEditing(WebCore::Range*) OVERRIDE;
    virtual bool shouldInsertNode(WebCore::Node*, WebCore::Range*, WebCore::EditorInsertAction) OVERRIDE;
    virtual bool shouldInsertText(const WTF::String&, WebCore::Range*, WebCore::EditorInsertAction) OVERRIDE;
    virtual bool shouldChangeSelectedRange(WebCore::Range* fromRange, WebCore::Range* toRange, WebCore::EAffinity, bool stillSelecting) OVERRIDE;

    virtual bool shouldApplyStyle(WebCore::StyleProperties*, WebCore::Range*) OVERRIDE;
    
    virtual bool shouldMoveRangeAfterDelete(WebCore::Range*, WebCore::Range* rangeToBeReplaced) OVERRIDE;

    virtual void didBeginEditing() OVERRIDE;
    virtual void didEndEditing() OVERRIDE;
    virtual void willWriteSelectionToPasteboard(WebCore::Range*) OVERRIDE;
    virtual void didWriteSelectionToPasteboard() OVERRIDE;
    virtual void getClientPasteboardDataForRange(WebCore::Range*, Vector<String>& pasteboardTypes, Vector<RefPtr<WebCore::SharedBuffer>>& pasteboardData) OVERRIDE;

    virtual NSString* userVisibleString(NSURL *) OVERRIDE;
    virtual WebCore::DocumentFragment* documentFragmentFromAttributedString(NSAttributedString *, Vector< RefPtr<WebCore::ArchiveResource>>&) OVERRIDE;
    virtual void setInsertionPasteboard(const String&) OVERRIDE;
    virtual NSURL* canonicalizeURL(NSURL*) OVERRIDE;
    virtual NSURL* canonicalizeURLString(NSString*) OVERRIDE;
    
#if USE(APPKIT)
    virtual void uppercaseWord() OVERRIDE;
    virtual void lowercaseWord() OVERRIDE;
    virtual void capitalizeWord() OVERRIDE;
#endif

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    virtual void showSubstitutionsPanel(bool show) OVERRIDE;
    virtual bool substitutionsPanelIsShowing() OVERRIDE;
    virtual void toggleSmartInsertDelete() OVERRIDE;
    virtual bool isAutomaticQuoteSubstitutionEnabled() OVERRIDE;
    virtual void toggleAutomaticQuoteSubstitution() OVERRIDE;
    virtual bool isAutomaticLinkDetectionEnabled() OVERRIDE;
    virtual void toggleAutomaticLinkDetection() OVERRIDE;
    virtual bool isAutomaticDashSubstitutionEnabled() OVERRIDE;
    virtual void toggleAutomaticDashSubstitution() OVERRIDE;
    virtual bool isAutomaticTextReplacementEnabled() OVERRIDE;
    virtual void toggleAutomaticTextReplacement() OVERRIDE;
    virtual bool isAutomaticSpellingCorrectionEnabled() OVERRIDE;
    virtual void toggleAutomaticSpellingCorrection() OVERRIDE;
#endif

#if ENABLE(DELETION_UI)
    virtual bool shouldShowDeleteInterface(WebCore::HTMLElement*) OVERRIDE;
#endif

    TextCheckerClient* textChecker() OVERRIDE { return this; }

    virtual void respondToChangedContents() OVERRIDE;
    virtual void respondToChangedSelection(WebCore::Frame*) OVERRIDE;

    virtual void registerUndoStep(PassRefPtr<WebCore::UndoStep>) OVERRIDE;
    virtual void registerRedoStep(PassRefPtr<WebCore::UndoStep>) OVERRIDE;
    virtual void clearUndoRedoOperations() OVERRIDE;

    virtual bool canCopyCut(WebCore::Frame*, bool defaultValue) const OVERRIDE;
    virtual bool canPaste(WebCore::Frame*, bool defaultValue) const OVERRIDE;
    virtual bool canUndo() const OVERRIDE;
    virtual bool canRedo() const OVERRIDE;
    
    virtual void undo() OVERRIDE;
    virtual void redo() OVERRIDE;
    
    virtual void handleKeyboardEvent(WebCore::KeyboardEvent*) OVERRIDE;
    virtual void handleInputMethodKeydown(WebCore::KeyboardEvent*) OVERRIDE;

    virtual void textFieldDidBeginEditing(WebCore::Element*) OVERRIDE;
    virtual void textFieldDidEndEditing(WebCore::Element*) OVERRIDE;
    virtual void textDidChangeInTextField(WebCore::Element*) OVERRIDE;
    virtual bool doTextFieldCommandFromEvent(WebCore::Element*, WebCore::KeyboardEvent*) OVERRIDE;
    virtual void textWillBeDeletedInTextField(WebCore::Element*) OVERRIDE;
    virtual void textDidChangeInTextArea(WebCore::Element*) OVERRIDE;

#if PLATFORM(IOS)
    virtual void suppressSelectionNotifications() OVERRIDE;
    virtual void restoreSelectionNotifications() OVERRIDE;
    virtual void startDelayingAndCoalescingContentChangeNotifications() OVERRIDE;
    virtual void stopDelayingAndCoalescingContentChangeNotifications() OVERRIDE;
    virtual void writeDataToPasteboard(NSDictionary*) OVERRIDE;
    virtual NSArray* supportedPasteboardTypesForCurrentSelection() OVERRIDE;
    virtual NSArray* readDataFromPasteboard(NSString* type, int index) OVERRIDE;
    virtual bool hasRichlyEditableSelection() OVERRIDE;
    virtual int getPasteboardItemsCount() OVERRIDE;
    virtual WebCore::DocumentFragment* documentFragmentFromDelegate(int index) OVERRIDE;
    virtual bool performsTwoStepPaste(WebCore::DocumentFragment*) OVERRIDE;
    virtual int pasteboardChangeCount() OVERRIDE;
#endif
    
#if !PLATFORM(IOS)
    virtual bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const OVERRIDE;
    virtual void ignoreWordInSpellDocument(const WTF::String&) OVERRIDE;
    virtual void learnWord(const WTF::String&) OVERRIDE;
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength) OVERRIDE;
    virtual WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&) OVERRIDE;
    virtual void checkGrammarOfString(const UChar*, int length, WTF::Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) OVERRIDE;
    virtual Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, WebCore::TextCheckingTypeMask checkingTypes) OVERRIDE;
    virtual void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&) OVERRIDE;
    virtual void updateSpellingUIWithMisspelledWord(const WTF::String&) OVERRIDE;
    virtual void showSpellingUI(bool show) OVERRIDE;
    virtual bool spellingUIIsShowing() OVERRIDE;
    virtual void getGuessesForWord(const WTF::String& word, const WTF::String& context, WTF::Vector<WTF::String>& guesses) OVERRIDE;
#else
    virtual bool shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType) const OVERRIDE { return true; }
    virtual void ignoreWordInSpellDocument(const WTF::String&) OVERRIDE { }
    virtual void learnWord(const WTF::String&) OVERRIDE { }
    virtual void checkSpellingOfString(const UChar*, int length, int* misspellingLocation, int* misspellingLength) OVERRIDE { }
    virtual WTF::String getAutoCorrectSuggestionForMisspelledWord(const WTF::String&) OVERRIDE { return ""; }
    virtual void checkGrammarOfString(const UChar*, int length, WTF::Vector<WebCore::GrammarDetail>&, int* badGrammarLocation, int* badGrammarLength) OVERRIDE { }
    virtual Vector<WebCore::TextCheckingResult> checkTextOfParagraph(StringView, WebCore::TextCheckingTypeMask checkingTypes) OVERRIDE;
    virtual void updateSpellingUIWithGrammarString(const WTF::String&, const WebCore::GrammarDetail&) OVERRIDE { }
    virtual void updateSpellingUIWithMisspelledWord(const WTF::String&) OVERRIDE { }
    virtual void showSpellingUI(bool show) OVERRIDE { }
    virtual bool spellingUIIsShowing() OVERRIDE { return false; }
    virtual void getGuessesForWord(const WTF::String& word, const WTF::String& context, WTF::Vector<WTF::String>& guesses) OVERRIDE { }
#endif // PLATFORM(IOS)
    virtual void willSetInputMethodState() OVERRIDE;
    virtual void setInputMethodState(bool enabled) OVERRIDE;
    virtual void requestCheckingOfString(PassRefPtr<WebCore::TextCheckingRequest>) OVERRIDE;

    void didCheckSucceed(int sequence, NSArray* results);

private:
    void registerUndoOrRedoStep(PassRefPtr<WebCore::UndoStep>, bool isRedo);
    WebEditorClient();

    WebView *m_webView;
    RetainPtr<WebEditorUndoTarget> m_undoTarget;
    bool m_haveUndoRedoOperations;
    RefPtr<WebCore::TextCheckingRequest> m_textCheckingRequest;
#if PLATFORM(IOS)
    int m_selectionNotificationSuppressions;
    bool m_delayingContentChangeNotifications;
    bool m_hasDelayedContentChangeNotification;
#endif
};
