/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "WebPage.h"

#include "EditorState.h"
#include "InputMethodState.h"
#include "UserMessage.h"
#include "WebKitExtensionManager.h"
#include "WebKitUserMessage.h"
#include "WebKitWebExtension.h"
#include "WebKitWebPagePrivate.h"
#include "WebPageProxyMessages.h"
#include <WebCore/Editor.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/TextIterator.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/VisibleUnits.h>

namespace WebKit {
using namespace WebCore;

void WebPage::sendMessageToWebExtensionWithReply(UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    auto* extension = WebKitExtensionManager::singleton().extension();
    if (!extension) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    auto* page = webkit_web_extension_get_page(extension, m_identifier.toUInt64());
    if (!page) {
        completionHandler(UserMessage(message.name, WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE));
        return;
    }

    webkitWebPageDidReceiveUserMessage(page, WTFMove(message), WTFMove(completionHandler));
}

void WebPage::sendMessageToWebExtension(UserMessage&& message)
{
    sendMessageToWebExtensionWithReply(WTFMove(message), [](UserMessage&&) { });
}

void WebPage::getPlatformEditorState(Frame& frame, EditorState& result) const
{
    if (result.isMissingPostLayoutData || !frame.view() || frame.view()->needsLayout())
        return;

    auto& postLayoutData = result.postLayoutData();
    postLayoutData.caretRectAtStart = frame.selection().absoluteCaretBounds();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone())
        return;

#if PLATFORM(GTK)
    const Editor& editor = frame.editor();
    if (selection.isRange()) {
        if (editor.selectionHasStyle(CSSPropertyFontWeight, "bold") == TriState::True)
            postLayoutData.typingAttributes |= AttributeBold;
        if (editor.selectionHasStyle(CSSPropertyFontStyle, "italic") == TriState::True)
            postLayoutData.typingAttributes |= AttributeItalics;
        if (editor.selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline") == TriState::True)
            postLayoutData.typingAttributes |= AttributeUnderline;
        if (editor.selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "line-through") == TriState::True)
            postLayoutData.typingAttributes |= AttributeStrikeThrough;
    } else if (selection.isCaret()) {
        if (editor.selectionStartHasStyle(CSSPropertyFontWeight, "bold"))
            postLayoutData.typingAttributes |= AttributeBold;
        if (editor.selectionStartHasStyle(CSSPropertyFontStyle, "italic"))
            postLayoutData.typingAttributes |= AttributeItalics;
        if (editor.selectionStartHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"))
            postLayoutData.typingAttributes |= AttributeUnderline;
        if (editor.selectionStartHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "line-through"))
            postLayoutData.typingAttributes |= AttributeStrikeThrough;
    }
#endif

    if (selection.isContentEditable()) {
        auto selectionStart = selection.visibleStart();
        auto surroundingStart = startOfEditableContent(selectionStart);
        auto surroundingEnd = endOfEditableContent(selectionStart);
        auto surroundingRange = makeRange(surroundingStart, surroundingEnd);
        auto compositionRange = frame.editor().compositionRange();
        if (compositionRange && surroundingRange && surroundingRange->contains(createLiveRange(*compositionRange).get())) {
            auto clonedRange = surroundingRange->cloneRange();
            surroundingRange->setEnd(createLegacyEditingPosition(compositionRange->start));
            clonedRange->setStart(createLegacyEditingPosition(compositionRange->end));
            postLayoutData.surroundingContext = plainText(*surroundingRange) + plainText(clonedRange);
            postLayoutData.surroundingContextCursorPosition = characterCount(*surroundingRange);
            postLayoutData.surroundingContextSelectionPosition = postLayoutData.surroundingContextCursorPosition;
        } else {
            postLayoutData.surroundingContext = surroundingRange ? plainText(*surroundingRange) : emptyString();
            if (surroundingStart.isNull() || selectionStart.isNull())
                postLayoutData.surroundingContextCursorPosition = 0;
            else
                postLayoutData.surroundingContextCursorPosition = characterCount(*makeRange(surroundingStart, selectionStart));
            if (surroundingStart.isNull() || selection.visibleEnd().isNull())
                postLayoutData.surroundingContextSelectionPosition = 0;
            else
                postLayoutData.surroundingContextSelectionPosition = characterCount(*makeRange(surroundingStart, selection.visibleEnd()));
        }
    }
}

static Optional<InputMethodState> inputMethodSateForElement(Element* element)
{
    if (!element || !element->shouldUseInputMethod())
        return WTF::nullopt;

    InputMethodState state;
    if (is<HTMLInputElement>(*element)) {
        auto& inputElement = downcast<HTMLInputElement>(*element);
        state.setPurposeForInputElement(inputElement);
        state.addHintsForAutocapitalizeType(inputElement.autocapitalizeType());
    } else if (is<HTMLTextAreaElement>(*element) || (element->hasEditableStyle() && is<HTMLElement>(*element))) {
        auto& htmlElement = downcast<HTMLElement>(*element);
        state.setPurposeOrHintForInputMode(htmlElement.canonicalInputMode());
        state.addHintsForAutocapitalizeType(htmlElement.autocapitalizeType());
    }

    if (element->isSpellCheckingEnabled())
        state.hints.add(InputMethodState::Hint::Spellcheck);

    return state;
}

void WebPage::setInputMethodState(Element* element)
{
    auto state = inputMethodSateForElement(element);
    if (m_inputMethodState == state)
        return;

    m_inputMethodState = state;
    send(Messages::WebPageProxy::SetInputMethodState(state));
}

} // namespace WebKit
