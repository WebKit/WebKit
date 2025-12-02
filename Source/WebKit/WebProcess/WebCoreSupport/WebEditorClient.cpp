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

#include "APIInjectedBundleEditorClient.h"
#include "APIInjectedBundleFormClient.h"
#include "EditorState.h"
#include "MessageSenderInlines.h"
#include "SharedBufferReference.h"
#include "UndoOrRedo.h"
#include "WKBundlePageEditorClient.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageProxy.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include "WebUndoStep.h"
#include <WebCore/ArchiveResource.h>
#include <WebCore/DOMPasteAccess.h>
#include <WebCore/DocumentFragment.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NodeDocument.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/Range.h>
#include <WebCore/SerializedAttachmentData.h>
#include <WebCore/SpellChecker.h>
#include <WebCore/StyleProperties.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UndoStep.h>
#include <WebCore/UserTypingGestureIndicator.h>
#include <WebCore/VisibleUnits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/StringView.h>

namespace WebKit {
using namespace WebCore;
using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebEditorClient);

bool WebEditorClient::shouldDeleteRange(const std::optional<SimpleRange>& range)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldDeleteRange(*page, range) : false;
}

bool WebEditorClient::smartInsertDeleteEnabled()
{
    RefPtr page = m_page.get();
    return page ? page->isSmartInsertDeleteEnabled() : false;
}

bool WebEditorClient::isSelectTrailingWhitespaceEnabled() const
{
    RefPtr page = m_page.get();
    return page ? page->isSelectTrailingWhitespaceEnabled() : false;
}

bool WebEditorClient::isContinuousSpellCheckingEnabled()
{
    return WebProcess::singleton().textCheckerState().contains(TextCheckerState::ContinuousSpellCheckingEnabled);
}

void WebEditorClient::toggleContinuousSpellChecking()
{
    notImplemented();
}

bool WebEditorClient::isGrammarCheckingEnabled()
{
    return WebProcess::singleton().textCheckerState().contains(TextCheckerState::GrammarCheckingEnabled);
}

void WebEditorClient::toggleGrammarChecking()
{
    notImplemented();
}

int WebEditorClient::spellCheckerDocumentTag()
{
    notImplemented();
    return 0;
}

bool WebEditorClient::shouldBeginEditing(const SimpleRange& range)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldBeginEditing(*page, range) : false;
}

bool WebEditorClient::shouldEndEditing(const SimpleRange& range)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldEndEditing(*page, range) : false;
}

bool WebEditorClient::shouldInsertNode(Node& node, const std::optional<SimpleRange>& rangeToReplace, EditorInsertAction action)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldInsertNode(*page, node, rangeToReplace, action) : false;
}

bool WebEditorClient::shouldInsertText(const String& text, const std::optional<SimpleRange>& rangeToReplace, EditorInsertAction action)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldInsertText(*page, text, rangeToReplace, action) : false;
}

bool WebEditorClient::shouldChangeSelectedRange(const std::optional<SimpleRange>& fromRange, const std::optional<SimpleRange>& toRange, Affinity affinity, bool stillSelecting)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldChangeSelectedRange(*page, fromRange, toRange, affinity, stillSelecting) : false;
}

bool WebEditorClient::shouldApplyStyle(const StyleProperties& style, const std::optional<SimpleRange>& range)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().shouldApplyStyle(*page, style, range) : false;
}

#if ENABLE(ATTACHMENT_ELEMENT)

void WebEditorClient::registerAttachmentIdentifier(const String& identifier, const String& contentType, const String& preferredFileName, Ref<FragmentedSharedBuffer>&& data)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RegisterAttachmentIdentifierFromData(identifier, contentType, preferredFileName, IPC::SharedBufferReference(WTFMove(data))));
}

void WebEditorClient::registerAttachments(Vector<WebCore::SerializedAttachmentData>&& data)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RegisterAttachmentsFromSerializedData(WTFMove(data)));
}

void WebEditorClient::registerAttachmentIdentifier(const String& identifier, const String& contentType, const String& filePath)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RegisterAttachmentIdentifierFromFilePath(identifier, contentType, filePath));
}

void WebEditorClient::registerAttachmentIdentifier(const String& identifier)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::RegisterAttachmentIdentifier(identifier));
}

void WebEditorClient::cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::CloneAttachmentData(fromIdentifier, toIdentifier));
}

void WebEditorClient::didInsertAttachmentWithIdentifier(const String& identifier, const String& source, WebCore::AttachmentAssociatedElementType associatedElementType)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::DidInsertAttachmentWithIdentifier(identifier, source, associatedElementType));
}

void WebEditorClient::didRemoveAttachmentWithIdentifier(const String& identifier)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::DidRemoveAttachmentWithIdentifier(identifier));
}

Vector<SerializedAttachmentData> WebEditorClient::serializedAttachmentDataForIdentifiers(const Vector<String>& identifiers)
{
    RefPtr page = m_page.get();
    if (!page)
        return { };
    auto sendResult = page->sendSync(Messages::WebPageProxy::SerializedAttachmentDataForIdentifiers(identifiers));
    auto [serializedData] = sendResult.takeReplyOr(Vector<WebCore::SerializedAttachmentData> { });
    return serializedData;
}

#endif

void WebEditorClient::didApplyStyle()
{
    if (RefPtr page = m_page.get())
        page->didApplyStyle();
}

bool WebEditorClient::shouldMoveRangeAfterDelete(const SimpleRange&, const SimpleRange&)
{
    return true;
}

void WebEditorClient::didBeginEditing()
{
    // FIXME: What good is a notification name, if it's always the same?
    static NeverDestroyed<String> WebViewDidBeginEditingNotification(MAKE_STATIC_STRING_IMPL("WebViewDidBeginEditingNotification"));
    if (RefPtr page = m_page.get())
        page->injectedBundleEditorClient().didBeginEditing(*page, WebViewDidBeginEditingNotification.get().impl());
}

void WebEditorClient::respondToChangedContents()
{
    static NeverDestroyed<String> WebViewDidChangeNotification(MAKE_STATIC_STRING_IMPL("WebViewDidChangeNotification"));
    RefPtr page = m_page.get();
    if (!page)
        return;
    page->injectedBundleEditorClient().didChange(*page, WebViewDidChangeNotification.get().impl());
    page->didChangeContents();
}

void WebEditorClient::respondToChangedSelection(LocalFrame* frame)
{
    static NeverDestroyed<String> WebViewDidChangeSelectionNotification(MAKE_STATIC_STRING_IMPL("WebViewDidChangeSelectionNotification"));
    RefPtr page = m_page.get();
    if (!page)
        return;
    page->injectedBundleEditorClient().didChangeSelection(*page, WebViewDidChangeSelectionNotification.get().impl());
    if (!frame)
        return;

    page->didChangeSelection(*frame);

#if PLATFORM(GTK)
    updateGlobalSelection(frame);
#endif
}

void WebEditorClient::didEndUserTriggeredSelectionChanges()
{
    if (RefPtr page = m_page.get())
        page->didEndUserTriggeredSelectionChanges();
}

void WebEditorClient::updateEditorStateAfterLayoutIfEditabilityChanged()
{
    if (RefPtr page = m_page.get())
        page->updateEditorStateAfterLayoutIfEditabilityChanged();
}

void WebEditorClient::didUpdateComposition()
{
    if (RefPtr page = m_page.get())
        page->didUpdateComposition();
}

void WebEditorClient::discardedComposition(const Document& document)
{
    if (RefPtr page = m_page.get())
        page->discardedComposition(document);
}

void WebEditorClient::canceledComposition()
{
    if (RefPtr page = m_page.get())
        page->canceledComposition();
}

void WebEditorClient::didEndEditing()
{
    static NeverDestroyed<String> WebViewDidEndEditingNotification(MAKE_STATIC_STRING_IMPL("WebViewDidEndEditingNotification"));
    if (RefPtr page = m_page.get())
        page->injectedBundleEditorClient().didEndEditing(*page, WebViewDidEndEditingNotification.get().impl());
}

void WebEditorClient::didWriteSelectionToPasteboard()
{
    if (RefPtr page = m_page.get())
        page->injectedBundleEditorClient().didWriteToPasteboard(*page);
}

void WebEditorClient::willWriteSelectionToPasteboard(const std::optional<SimpleRange>& range)
{
    if (RefPtr page = m_page.get())
        page->injectedBundleEditorClient().willWriteToPasteboard(*page, range);
}

void WebEditorClient::getClientPasteboardData(const std::optional<SimpleRange>& range, Vector<std::pair<String, RefPtr<WebCore::SharedBuffer>>>& pasteboardTypesAndData)
{
    Vector<String> pasteboardTypes;
    Vector<RefPtr<WebCore::SharedBuffer>> pasteboardData;
    for (size_t i = 0; i < pasteboardTypesAndData.size(); ++i) {
        pasteboardTypes.append(pasteboardTypesAndData[i].first);
        pasteboardData.append(pasteboardTypesAndData[i].second);
    }

    if (RefPtr page = m_page.get())
        page->injectedBundleEditorClient().getPasteboardDataForRange(*page, range, pasteboardTypes, pasteboardData);

    ASSERT(pasteboardTypes.size() == pasteboardData.size());
    pasteboardTypesAndData.clear();
    for (size_t i = 0; i < pasteboardTypes.size(); ++i)
        pasteboardTypesAndData.append(std::make_pair(pasteboardTypes[i], pasteboardData[i]));
}

bool WebEditorClient::performTwoStepDrop(DocumentFragment& fragment, const SimpleRange& destination, bool isMove)
{
    RefPtr page = m_page.get();
    return page ? page->injectedBundleEditorClient().performTwoStepDrop(*page, fragment, destination, isMove) : false;
}

void WebEditorClient::registerUndoStep(UndoStep& step)
{
    // FIXME: Add assertion that the command being reapplied is the same command that is
    // being passed to us.
    RefPtr page = m_page.get();
    if (!page || page->isInRedo())
        return;

    auto webStep = WebUndoStep::create(step);
    auto stepID = webStep->stepID();

    page->addWebUndoStep(stepID, WTFMove(webStep));
    page->send(Messages::WebPageProxy::RegisterEditCommandForUndo(stepID, step.label()), IPC::SendOption::DispatchMessageEvenWhenWaitingForSyncReply);
}

void WebEditorClient::registerRedoStep(UndoStep&)
{
}

void WebEditorClient::clearUndoRedoOperations()
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::ClearAllEditCommands());
}

bool WebEditorClient::canCopyCut(LocalFrame*, bool defaultValue) const
{
    return defaultValue;
}

bool WebEditorClient::canPaste(LocalFrame*, bool defaultValue) const
{
    return defaultValue;
}

bool WebEditorClient::canUndo() const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    auto sendResult = page->sendSync(Messages::WebPageProxy::CanUndoRedo(UndoOrRedo::Undo));
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

bool WebEditorClient::canRedo() const
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    auto sendResult = page->sendSync(Messages::WebPageProxy::CanUndoRedo(UndoOrRedo::Redo));
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

void WebEditorClient::undo()
{
    if (RefPtr page = m_page.get())
        page->sendSync(Messages::WebPageProxy::ExecuteUndoRedo(UndoOrRedo::Undo));
}

void WebEditorClient::redo()
{
    if (RefPtr page = m_page.get())
        page->sendSync(Messages::WebPageProxy::ExecuteUndoRedo(UndoOrRedo::Redo));
}

WebCore::DOMPasteAccessResponse WebEditorClient::requestDOMPasteAccess(WebCore::DOMPasteAccessCategory pasteAccessCategory, WebCore::FrameIdentifier frameID, const String& originIdentifier)
{
    RefPtr page = m_page.get();
    return page ? page->requestDOMPasteAccess(pasteAccessCategory, frameID, originIdentifier) : WebCore::DOMPasteAccessResponse::DeniedForGesture;
}

#if !PLATFORM(COCOA) && !USE(GLIB)

void WebEditorClient::handleKeyboardEvent(KeyboardEvent& event)
{
    RefPtr page = m_page.get();
    if (page && page->handleEditingKeyboardEvent(event))
        event.setDefaultHandled();
}

void WebEditorClient::handleInputMethodKeydown(KeyboardEvent&)
{
    notImplemented();
}

#endif // !PLATFORM(COCOA) && !USE(GLIB)

void WebEditorClient::textFieldDidBeginEditing(Element& element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement)
        return;

    RefPtr frame = element.document().frame();
    RefPtr webFrame = WebFrame::fromCoreFrame(*frame);
    ASSERT(webFrame);

    if (RefPtr page = m_page.get())
        page->injectedBundleFormClient().textFieldDidBeginEditing(page.get(), *inputElement, webFrame.get());
}

void WebEditorClient::textFieldDidEndEditing(Element& element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement)
        return;

    auto webFrame = WebFrame::fromCoreFrame(*element.protectedDocument()->protectedFrame());
    ASSERT(webFrame);

    if (RefPtr page = m_page.get())
        page->injectedBundleFormClient().textFieldDidEndEditing(page.get(), *inputElement, webFrame.get());
}

void WebEditorClient::textDidChangeInTextField(Element& element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement)
        return;

    bool initiatedByUserTyping = UserTypingGestureIndicator::processingUserTypingGesture() && UserTypingGestureIndicator::focusedElementAtGestureStart() == inputElement;

    auto webFrame = WebFrame::fromCoreFrame(*element.protectedDocument()->protectedFrame());
    ASSERT(webFrame);

    if (RefPtr page = m_page.get())
        page->injectedBundleFormClient().textDidChangeInTextField(page.get(), *inputElement, webFrame.get(), initiatedByUserTyping);

    if (initiatedByUserTyping)
        inputElement->dispatchUserTextInputEvent();
}

void WebEditorClient::textDidChangeInTextArea(Element& element)
{
    RefPtr textAreaElement = dynamicDowncast<HTMLTextAreaElement>(element);
    if (!textAreaElement)
        return;

    auto webFrame = WebFrame::fromCoreFrame(*element.protectedDocument()->protectedFrame());
    ASSERT(webFrame);

    if (RefPtr page = m_page.get())
        page->injectedBundleFormClient().textDidChangeInTextArea(page.get(), *textAreaElement, webFrame.get());

    if (UserTypingGestureIndicator::processingUserTypingGesture())
        textAreaElement->dispatchUserTextInputEvent();
}

#if !PLATFORM(IOS_FAMILY)

void WebEditorClient::overflowScrollPositionChanged()
{
}

void WebEditorClient::subFrameScrollPositionChanged()
{
}

#endif

static bool getActionTypeForKeyEvent(KeyboardEvent* event, WKInputFieldActionType& type)
{
    String key = event->keyIdentifier();
    if (key == "Up"_s)
        type = WKInputFieldActionTypeMoveUp;
    else if (key == "Down"_s)
        type = WKInputFieldActionTypeMoveDown;
    else if (key == "U+001B"_s)
        type = WKInputFieldActionTypeCancel;
    else if (key == "U+0009"_s) {
        if (event->shiftKey())
            type = WKInputFieldActionTypeInsertBacktab;
        else
            type = WKInputFieldActionTypeInsertTab;
    } else if (key == "Enter"_s)
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

bool WebEditorClient::doTextFieldCommandFromEvent(Element& element, KeyboardEvent* event)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement)
        return false;

    WKInputFieldActionType actionType = static_cast<WKInputFieldActionType>(0);
    if (!getActionTypeForKeyEvent(event, actionType))
        return false;

    auto webFrame = WebFrame::fromCoreFrame(*element.protectedDocument()->protectedFrame());
    ASSERT(webFrame);

    RefPtr page = m_page.get();
    return page ? page->injectedBundleFormClient().shouldPerformActionInTextField(page.get(), *inputElement, toInputFieldAction(actionType), webFrame.get()) : false;
}

void WebEditorClient::textWillBeDeletedInTextField(Element& element)
{
    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(element);
    if (!inputElement)
        return;

    auto webFrame = WebFrame::fromCoreFrame(*element.protectedDocument()->protectedFrame());
    ASSERT(webFrame);

    if (RefPtr page = m_page.get())
        page->injectedBundleFormClient().shouldPerformActionInTextField(page.get(), *inputElement, toInputFieldAction(WKInputFieldActionTypeInsertDelete), webFrame.get());
}

bool WebEditorClient::shouldEraseMarkersAfterChangeSelection(WebCore::TextCheckingType type) const
{
    // This prevents erasing spelling and grammar markers to match AppKit.
#if PLATFORM(COCOA)
    return !(type == TextCheckingType::Spelling || type == TextCheckingType::Grammar);
#else
    UNUSED_PARAM(type);
    return true;
#endif
}

void WebEditorClient::ignoreWordInSpellDocument(const String& word)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::IgnoreWord(word));
}

void WebEditorClient::learnWord(const String& word)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::LearnWord(word));
}

void WebEditorClient::checkSpellingOfString(StringView text, int* misspellingLocation, int* misspellingLength)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    auto sendResult = page->sendSync(Messages::WebPageProxy::CheckSpellingOfString(text.toStringWithoutCopying()));
    auto [resultLocation, resultLength] = sendResult.takeReplyOr(-1, 0);
    *misspellingLocation = resultLocation;
    *misspellingLength = resultLength;
}

void WebEditorClient::checkGrammarOfString(StringView text, Vector<WebCore::GrammarDetail>& grammarDetails, int* badGrammarLocation, int* badGrammarLength)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    auto sendResult = page->sendSync(Messages::WebPageProxy::CheckGrammarOfString(text.toStringWithoutCopying()));
    int32_t resultLocation = -1;
    int32_t resultLength = 0;
    if (sendResult.succeeded())
        std::tie(grammarDetails, resultLocation, resultLength) = sendResult.takeReply();
    *badGrammarLocation = resultLocation;
    *badGrammarLength = resultLength;
}

static uint64_t insertionPointFromCurrentSelection(const VisibleSelection& currentSelection)
{
    auto selectionStart = currentSelection.visibleStart();
    auto range = makeSimpleRange(selectionStart, startOfParagraph(selectionStart));
    return range ? characterCount(*range) : 0;
}

#if USE(UNIFIED_TEXT_CHECKING)

Vector<TextCheckingResult> WebEditorClient::checkTextOfParagraph(StringView stringView, OptionSet<WebCore::TextCheckingType> checkingTypes, const VisibleSelection& currentSelection)
{
    RefPtr page = m_page.get();
    if (!page)
        return { };
    auto sendResult = page->sendSync(Messages::WebPageProxy::CheckTextOfParagraph(stringView.toStringWithoutCopying(), checkingTypes, insertionPointFromCurrentSelection(currentSelection)));
    auto [results] = sendResult.takeReplyOr(Vector<TextCheckingResult> { });
    return results;
}

#endif

void WebEditorClient::updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const GrammarDetail& grammarDetail)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::UpdateSpellingUIWithGrammarString(badGrammarPhrase, grammarDetail));
}

void WebEditorClient::updateSpellingUIWithMisspelledWord(const String& misspelledWord)
{
    if (RefPtr page = m_page.get())
        page->send(Messages::WebPageProxy::UpdateSpellingUIWithMisspelledWord(misspelledWord));
}

void WebEditorClient::showSpellingUI(bool)
{
    notImplemented();
}

bool WebEditorClient::spellingUIIsShowing()
{
    RefPtr page = m_page.get();
    if (!page)
        return false;
    auto sendResult = page->sendSync(Messages::WebPageProxy::SpellingUIIsShowing());
    auto [isShowing] = sendResult.takeReplyOr(false);
    return isShowing;
}

void WebEditorClient::getGuessesForWord(const String& word, const String& context, const VisibleSelection& currentSelection, Vector<String>& guesses)
{
    RefPtr page = m_page.get();
    if (!page)
        return;
    auto sendResult = page->sendSync(Messages::WebPageProxy::GetGuessesForWord(word, context, insertionPointFromCurrentSelection(currentSelection)));
    if (sendResult.succeeded())
        std::tie(guesses) = sendResult.takeReply();
}

void WebEditorClient::requestCheckingOfString(TextCheckingRequest& request, const WebCore::VisibleSelection& currentSelection)
{
    auto requestID = TextCheckerRequestID::generate();
    RefPtr page = m_page.get();
    if (!page)
        return;
    page->addTextCheckingRequest(requestID, request);

    page->send(Messages::WebPageProxy::RequestCheckingOfString(requestID, request.data(), insertionPointFromCurrentSelection(currentSelection)));
}

void WebEditorClient::willChangeSelectionForAccessibility()
{
    if (RefPtr page = m_page.get())
        page->willChangeSelectionForAccessibility();
}

void WebEditorClient::didChangeSelectionForAccessibility()
{
    if (RefPtr page = m_page.get())
        page->didChangeSelectionForAccessibility();
}

void WebEditorClient::setInputMethodState(Element* element)
{
#if PLATFORM(GTK) || PLATFORM(WPE)
    if (RefPtr page = m_page.get())
        page->setInputMethodState(element);
#else
    UNUSED_PARAM(element);
#endif
}

bool WebEditorClient::supportsGlobalSelection()
{
#if PLATFORM(GTK)
    return true;
#else
    return false;
#endif
}

} // namespace WebKit
