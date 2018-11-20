/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebEditorClient.h"

#include "EditorState.h"
#include "SharedBufferDataReference.h"
#include "UndoOrRedo.h"
#include "WKBundlePageEditorClient.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebUndoStep.h"
#include <WebCore/ArchiveResource.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/SerializedAttachmentData.h>
#include <WebCore/SpellChecker.h>
#include <WebCore/StyleProperties.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UndoStep.h>
#include <WebCore/UserTypingGestureIndicator.h>
#include <WebCore/VisibleUnits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringView.h>

#if PLATFORM(GTK)
#include <WebCore/PlatformDisplay.h>
#endif

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

static uint64_t generateTextCheckingRequestID()
{
    static uint64_t uniqueTextCheckingRequestID = 1;
    return uniqueTextCheckingRequestID++;
}

bool WebEditorClient::shouldDeleteRange(Range* range)
{
    bool result = m_page->injectedBundleEditorClient().shouldDeleteRange(*m_page, range);
    notImplemented();
    return result;
}

bool WebEditorClient::smartInsertDeleteEnabled()
{
    return m_page->isSmartInsertDeleteEnabled();
}
 
bool WebEditorClient::isSelectTrailingWhitespaceEnabled() const
{
    return m_page->isSelectTrailingWhitespaceEnabled();
}

bool WebEditorClient::isContinuousSpellCheckingEnabled()
{
    return WebProcess::singleton().textCheckerState().isContinuousSpellCheckingEnabled;
}

void WebEditorClient::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool WebEditorClient::isGrammarCheckingEnabled()
{
    return WebProcess::singleton().textCheckerState().isGrammarCheckingEnabled;
}

void WebEditorClient::toggleGrammarChecking()
{
    notImplemented();
}

int WebEditorClient::spellCheckerDocumentTag()
{
    notImplemented();
    return false;
}

bool WebEditorClient::shouldBeginEditing(Range* range)
{
    bool result = m_page->injectedBundleEditorClient().shouldBeginEditing(*m_page, range);
    notImplemented();
    return result;
}

bool WebEditorClient::shouldEndEditing(Range* range)
{
    bool result = m_page->injectedBundleEditorClient().shouldEndEditing(*m_page, range);
    notImplemented();
    return result;
}

bool WebEditorClient::shouldInsertNode(Node* node, Range* rangeToReplace, EditorInsertAction action)
{
    bool result = m_page->injectedBundleEditorClient().shouldInsertNode(*m_page, node, rangeToReplace, action);
    notImplemented();
    return result;
}

bool WebEditorClient::shouldInsertText(const String& text, Range* rangeToReplace, EditorInsertAction action)
{
    bool result = m_page->injectedBundleEditorClient().shouldInsertText(*m_page, text.impl(), rangeToReplace, action);
    notImplemented();
    return result;
}

bool WebEditorClient::shouldChangeSelectedRange(Range* fromRange, Range* toRange, EAffinity affinity, bool stillSelecting)
{
    bool result = m_page->injectedBundleEditorClient().shouldChangeSelectedRange(*m_page, fromRange, toRange, affinity, stillSelecting);
    notImplemented();
    return result;
}
    
bool WebEditorClient::shouldApplyStyle(StyleProperties* style, Range* range)
{
    Ref<MutableStyleProperties> mutableStyle(style->isMutable() ? Ref<MutableStyleProperties>(static_cast<MutableStyleProperties&>(*style)) : style->mutableCopy());
    bool result = m_page->injectedBundleEditorClient().shouldApplyStyle(*m_page, &mutableStyle->ensureCSSStyleDeclaration(), range);
    notImplemented();
    return result;
}

#if ENABLE(ATTACHMENT_ELEMENT)

void WebEditorClient::registerAttachmentIdentifier(const String& identifier, const String& contentType, const String& preferredFileName, Ref<SharedBuffer>&& data)
{
    m_page->send(Messages::WebPageProxy::RegisterAttachmentIdentifierFromData(identifier, contentType, preferredFileName, { data }));
}

void WebEditorClient::registerAttachments(Vector<WebCore::SerializedAttachmentData>&& data)
{
    m_page->send(Messages::WebPageProxy::registerAttachmentsFromSerializedData(WTFMove(data)));
}

void WebEditorClient::registerAttachmentIdentifier(const String& identifier, const String& contentType, const String& filePath)
{
    m_page->send(Messages::WebPageProxy::RegisterAttachmentIdentifierFromFilePath(identifier, contentType, filePath));
}

void WebEditorClient::registerAttachmentIdentifier(const String& identifier)
{
    m_page->send(Messages::WebPageProxy::RegisterAttachmentIdentifier(identifier));
}

void WebEditorClient::cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier)
{
    m_page->send(Messages::WebPageProxy::CloneAttachmentData(fromIdentifier, toIdentifier));
}

void WebEditorClient::didInsertAttachmentWithIdentifier(const String& identifier, const String& source, bool hasEnclosingImage)
{
    m_page->send(Messages::WebPageProxy::DidInsertAttachmentWithIdentifier(identifier, source, hasEnclosingImage));
}

void WebEditorClient::didRemoveAttachmentWithIdentifier(const String& identifier)
{
    m_page->send(Messages::WebPageProxy::DidRemoveAttachmentWithIdentifier(identifier));
}

Vector<SerializedAttachmentData> WebEditorClient::serializedAttachmentDataForIdentifiers(const Vector<String>& identifiers)
{
    Vector<WebCore::SerializedAttachmentData> serializedData;
    m_page->sendSync(Messages::WebPageProxy::SerializedAttachmentDataForIdentifiers(identifiers), Messages::WebPageProxy::SerializedAttachmentDataForIdentifiers::Reply(serializedData));
    return serializedData;
}

#endif

void WebEditorClient::didApplyStyle()
{
    m_page->didApplyStyle();
}

bool WebEditorClient::shouldMoveRangeAfterDelete(Range*, Range*)
{
    notImplemented();
    return true;
}

void WebEditorClient::didBeginEditing()
{
    // FIXME: What good is a notification name, if it's always the same?
    static NeverDestroyed<String> WebViewDidBeginEditingNotification(MAKE_STATIC_STRING_IMPL("WebViewDidBeginEditingNotification"));
    m_page->injectedBundleEditorClient().didBeginEditing(*m_page, WebViewDidBeginEditingNotification.get().impl());
    notImplemented();
}

void WebEditorClient::respondToChangedContents()
{
    static NeverDestroyed<String> WebViewDidChangeNotification(MAKE_STATIC_STRING_IMPL("WebViewDidChangeNotification"));
    m_page->injectedBundleEditorClient().didChange(*m_page, WebViewDidChangeNotification.get().impl());
    m_page->didChangeContents();
}

void WebEditorClient::respondToChangedSelection(Frame* frame)
{
    static NeverDestroyed<String> WebViewDidChangeSelectionNotification(MAKE_STATIC_STRING_IMPL("WebViewDidChangeSelectionNotification"));
    m_page->injectedBundleEditorClient().didChangeSelection(*m_page, WebViewDidChangeSelectionNotification.get().impl());
    if (!frame)
        return;

    m_page->didChangeSelection();

#if PLATFORM(GTK)
    updateGlobalSelection(frame);
#endif
}

void WebEditorClient::didEndUserTriggeredSelectionChanges()
{
    m_page->didEndUserTriggeredSelectionChanges();
}

void WebEditorClient::updateEditorStateAfterLayoutIfEditabilityChanged()
{
    m_page->updateEditorStateAfterLayoutIfEditabilityChanged();
}

void WebEditorClient::didUpdateComposition()
{
    m_page->didUpdateComposition();
}

void WebEditorClient::discardedComposition(Frame*)
{
    m_page->discardedComposition();
}

void WebEditorClient::canceledComposition()
{
    m_page->canceledComposition();
}

void WebEditorClient::didEndEditing()
{
    static NeverDestroyed<String> WebViewDidEndEditingNotification(MAKE_STATIC_STRING_IMPL("WebViewDidEndEditingNotification"));
    m_page->injectedBundleEditorClient().didEndEditing(*m_page, WebViewDidEndEditingNotification.get().impl());
    notImplemented();
}

void WebEditorClient::didWriteSelectionToPasteboard()
{
    m_page->injectedBundleEditorClient().didWriteToPasteboard(*m_page);
}

void WebEditorClient::willWriteSelectionToPasteboard(Range* range)
{
    m_page->injectedBundleEditorClient().willWriteToPasteboard(*m_page, range);
}

void WebEditorClient::getClientPasteboardDataForRange(Range* range, Vector<String>& pasteboardTypes, Vector<RefPtr<SharedBuffer>>& pasteboardData)
{
    m_page->injectedBundleEditorClient().getPasteboardDataForRange(*m_page, range, pasteboardTypes, pasteboardData);
}

bool WebEditorClient::performTwoStepDrop(DocumentFragment& fragment, Range& destination, bool isMove)
{
    return m_page->injectedBundleEditorClient().performTwoStepDrop(*m_page, fragment, destination, isMove);
}

String WebEditorClient::replacementURLForResource(Ref<WebCore::SharedBuffer>&& resourceData, const String& mimeType)
{
    return m_page->injectedBundleEditorClient().replacementURLForResource(*m_page, WTFMove(resourceData), mimeType);
}

void WebEditorClient::registerUndoStep(UndoStep& step)
{
    // FIXME: Add assertion that the command being reapplied is the same command that is
    // being passed to us.
    if (m_page->isInRedo())
        return;

    auto webStep = WebUndoStep::create(step);
    auto editAction = static_cast<uint32_t>(webStep->step().editingAction());

    m_page->addWebUndoStep(webStep->stepID(), webStep.ptr());
    m_page->send(Messages::WebPageProxy::RegisterEditCommandForUndo(webStep->stepID(), editAction), m_page->pageID(), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void WebEditorClient::registerRedoStep(UndoStep&)
{
}

void WebEditorClient::clearUndoRedoOperations()
{
    m_page->send(Messages::WebPageProxy::ClearAllEditCommands());
}

bool WebEditorClient::canCopyCut(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool WebEditorClient::canPaste(Frame*, bool defaultValue) const
{
    return defaultValue;
}

bool WebEditorClient::canUndo() const
{
    bool result = false;
    m_page->sendSync(Messages::WebPageProxy::CanUndoRedo(UndoOrRedo::Undo), Messages::WebPageProxy::CanUndoRedo::Reply(result));
    return result;
}

bool WebEditorClient::canRedo() const
{
    bool result = false;
    m_page->sendSync(Messages::WebPageProxy::CanUndoRedo(UndoOrRedo::Redo), Messages::WebPageProxy::CanUndoRedo::Reply(result));
    return result;
}

void WebEditorClient::undo()
{
    m_page->sendSync(Messages::WebPageProxy::ExecuteUndoRedo(UndoOrRedo::Undo), Messages::WebPageProxy::ExecuteUndoRedo::Reply());
}

void WebEditorClient::redo()
{
    m_page->sendSync(Messages::WebPageProxy::ExecuteUndoRedo(UndoOrRedo::Redo), Messages::WebPageProxy::ExecuteUndoRedo::Reply());
}

#if PLATFORM(WIN)
void WebEditorClient::handleKeyboardEvent(KeyboardEvent* event)
{
    if (m_page->handleEditingKeyboardEvent(event))
        event->setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent*)
{
    notImplemented();
}
#endif // PLATFORM(WIN)

void WebEditorClient::textFieldDidBeginEditing(Element* element)
{
    if (!is<HTMLInputElement>(*element))
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);

    m_page->injectedBundleFormClient().textFieldDidBeginEditing(m_page, downcast<HTMLInputElement>(element), webFrame);
}

void WebEditorClient::textFieldDidEndEditing(Element* element)
{
    if (!is<HTMLInputElement>(*element))
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);

    m_page->injectedBundleFormClient().textFieldDidEndEditing(m_page, downcast<HTMLInputElement>(element), webFrame);
}

void WebEditorClient::textDidChangeInTextField(Element* element)
{
    if (!is<HTMLInputElement>(*element))
        return;

    bool initiatedByUserTyping = UserTypingGestureIndicator::processingUserTypingGesture() && UserTypingGestureIndicator::focusedElementAtGestureStart() == element;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);

    m_page->injectedBundleFormClient().textDidChangeInTextField(m_page, downcast<HTMLInputElement>(element), webFrame, initiatedByUserTyping);
}

void WebEditorClient::textDidChangeInTextArea(Element* element)
{
    if (!is<HTMLTextAreaElement>(*element))
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);

    m_page->injectedBundleFormClient().textDidChangeInTextArea(m_page, downcast<HTMLTextAreaElement>(element), webFrame);
}

#if !PLATFORM(IOS_FAMILY)
void WebEditorClient::overflowScrollPositionChanged()
{
    notImplemented();
}
#endif

static bool getActionTypeForKeyEvent(KeyboardEvent* event, WKInputFieldActionType& type)
{
    String key = event->keyIdentifier();
    if (key == "Up")
        type = WKInputFieldActionTypeMoveUp;
    else if (key == "Down")
        type = WKInputFieldActionTypeMoveDown;
    else if (key == "U+001B")
        type = WKInputFieldActionTypeCancel;
    else if (key == "U+0009") {
        if (event->shiftKey())
            type = WKInputFieldActionTypeInsertBacktab;
        else
            type = WKInputFieldActionTypeInsertTab;
    } else if (key == "Enter")
        type = WKInputFieldActionTypeInsertNewline;
    else
        return false;

    return true;
}

static API::InjectedBundle::FormClient::InputFieldAction toInputFieldAction(WKInputFieldActionType action)
{
    switch (action) {
    case WKInputFieldActionTypeMoveUp:
        return API::InjectedBundle::FormClient::InputFieldAction::MoveUp;
    case WKInputFieldActionTypeMoveDown:
        return API::InjectedBundle::FormClient::InputFieldAction::MoveDown;
    case WKInputFieldActionTypeCancel:
        return API::InjectedBundle::FormClient::InputFieldAction::Cancel;
    case WKInputFieldActionTypeInsertTab:
        return API::InjectedBundle::FormClient::InputFieldAction::InsertTab;
    case WKInputFieldActionTypeInsertNewline:
        return API::InjectedBundle::FormClient::InputFieldAction::InsertNewline;
    case WKInputFieldActionTypeInsertDelete:
        return API::InjectedBundle::FormClient::InputFieldAction::InsertDelete;
    case WKInputFieldActionTypeInsertBacktab:
        return API::InjectedBundle::FormClient::InputFieldAction::InsertBacktab;
    }

    ASSERT_NOT_REACHED();
    return API::InjectedBundle::FormClient::InputFieldAction::Cancel;
}

bool WebEditorClient::doTextFieldCommandFromEvent(Element* element, KeyboardEvent* event)
{
    if (!is<HTMLInputElement>(*element))
        return false;

    WKInputFieldActionType actionType = static_cast<WKInputFieldActionType>(0);
    if (!getActionTypeForKeyEvent(event, actionType))
        return false;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);

    return m_page->injectedBundleFormClient().shouldPerformActionInTextField(m_page, downcast<HTMLInputElement>(element), toInputFieldAction(actionType), webFrame);
}

void WebEditorClient::textWillBeDeletedInTextField(Element* element)
{
    if (!is<HTMLInputElement>(*element))
        return;

    WebFrame* webFrame = WebFrame::fromCoreFrame(*element->document().frame());
    ASSERT(webFrame);

    m_page->injectedBundleFormClient().shouldPerformActionInTextField(m_page, downcast<HTMLInputElement>(element), toInputFieldAction(WKInputFieldActionTypeInsertDelete), webFrame);
}

bool WebEditorClient::shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType type) const
{
    // This prevents erasing spelling markers on OS X Lion or later to match AppKit on these Mac OS X versions.
#if PLATFORM(COCOA)
    return type != TextCheckingType::Spelling;
#else
    UNUSED_PARAM(type);
    return true;
#endif
}

void WebEditorClient::ignoreWordInSpellDocument(const String& word)
{
    m_page->send(Messages::WebPageProxy::IgnoreWord(word));
}

void WebEditorClient::learnWord(const String& word)
{
    m_page->send(Messages::WebPageProxy::LearnWord(word));
}

void WebEditorClient::checkSpellingOfString(StringView text, int* misspellingLocation, int* misspellingLength)
{
    int32_t resultLocation = -1;
    int32_t resultLength = 0;
    m_page->sendSync(Messages::WebPageProxy::CheckSpellingOfString(text.toStringWithoutCopying()),
        Messages::WebPageProxy::CheckSpellingOfString::Reply(resultLocation, resultLength));
    *misspellingLocation = resultLocation;
    *misspellingLength = resultLength;
}

String WebEditorClient::getAutoCorrectSuggestionForMisspelledWord(const String&)
{
    notImplemented();
    return String();
}

void WebEditorClient::checkGrammarOfString(StringView text, Vector<WebCore::GrammarDetail>& grammarDetails, int* badGrammarLocation, int* badGrammarLength)
{
    int32_t resultLocation = -1;
    int32_t resultLength = 0;
    m_page->sendSync(Messages::WebPageProxy::CheckGrammarOfString(text.toStringWithoutCopying()),
        Messages::WebPageProxy::CheckGrammarOfString::Reply(grammarDetails, resultLocation, resultLength));
    *badGrammarLocation = resultLocation;
    *badGrammarLength = resultLength;
}

static int32_t insertionPointFromCurrentSelection(const VisibleSelection& currentSelection)
{
    VisiblePosition selectionStart = currentSelection.visibleStart();
    VisiblePosition paragraphStart = startOfParagraph(selectionStart);
    return TextIterator::rangeLength(makeRange(paragraphStart, selectionStart).get());
}

#if USE(UNIFIED_TEXT_CHECKING)
Vector<TextCheckingResult> WebEditorClient::checkTextOfParagraph(StringView stringView, OptionSet<WebCore::TextCheckingType> checkingTypes, const VisibleSelection& currentSelection)
{
    Vector<TextCheckingResult> results;
    m_page->sendSync(Messages::WebPageProxy::CheckTextOfParagraph(stringView.toStringWithoutCopying(), checkingTypes, insertionPointFromCurrentSelection(currentSelection)), Messages::WebPageProxy::CheckTextOfParagraph::Reply(results));
    return results;
}
#endif

void WebEditorClient::updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const GrammarDetail& grammarDetail)
{
    m_page->send(Messages::WebPageProxy::UpdateSpellingUIWithGrammarString(badGrammarPhrase, grammarDetail));
}

void WebEditorClient::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    m_page->send(Messages::WebPageProxy::UpdateSpellingUIWithMisspelledWord(misspelledWord));
}

void WebEditorClient::showSpellingUI(bool)
{
    notImplemented();
}

bool WebEditorClient::spellingUIIsShowing()
{
    bool isShowing = false;
    m_page->sendSync(Messages::WebPageProxy::SpellingUIIsShowing(), Messages::WebPageProxy::SpellingUIIsShowing::Reply(isShowing));
    return isShowing;
}

void WebEditorClient::getGuessesForWord(const String& word, const String& context, const VisibleSelection& currentSelection, Vector<String>& guesses)
{
    m_page->sendSync(Messages::WebPageProxy::GetGuessesForWord(word, context, insertionPointFromCurrentSelection(currentSelection)), Messages::WebPageProxy::GetGuessesForWord::Reply(guesses));
}

void WebEditorClient::requestCheckingOfString(TextCheckingRequest& request, const WebCore::VisibleSelection& currentSelection)
{
    uint64_t requestID = generateTextCheckingRequestID();
    m_page->addTextCheckingRequest(requestID, request);

    m_page->send(Messages::WebPageProxy::RequestCheckingOfString(requestID, request.data(), insertionPointFromCurrentSelection(currentSelection)));
}

void WebEditorClient::willSetInputMethodState()
{
}

void WebEditorClient::setInputMethodState(bool enabled)
{
#if PLATFORM(GTK)
    m_page->setInputMethodState(enabled);
#else
    notImplemented();
    UNUSED_PARAM(enabled);
#endif
}

bool WebEditorClient::supportsGlobalSelection()
{
#if PLATFORM(GTK)
#if PLATFORM(X11)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
        return true;
#endif
#if PLATFORM(WAYLAND)
    if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland)
        return true;
#endif
#endif
    return false;
}

} // namespace WebKit
