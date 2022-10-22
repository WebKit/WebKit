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
#include <WebCore/Range.h>
#include <WebCore/TextIterator.h>
#include <WebCore/UserAgent.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/VisibleUnits.h>

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitialize(const WebPageCreationParameters&)
{
#if USE(ATSPI)
    // Create the accessible object (the plug) that will serve as the
    // entry point to the Web process, and send a message to the UI
    // process to connect the two worlds through the accessibility
    // object there specifically placed for that purpose (the socket).
#if PLATFORM(GTK) && USE(GTK4)
    // FIXME: we need a way to connect DOM and app a11y tree in GTK4.
#else
    if (auto* page = corePage()) {
        m_accessibilityRootObject = AccessibilityRootAtspi::create(*page);
        m_accessibilityRootObject->registerObject([&](const String& plugID) {
            if (!plugID.isEmpty())
                send(Messages::WebPageProxy::BindAccessibilityTree(plugID));
        });
    }
#endif
#endif
}

void WebPage::platformDetach()
{
#if USE(ATSPI)
    if (m_accessibilityRootObject)
        m_accessibilityRootObject->unregisterObject();
#endif
}

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
    if (!result.hasPostLayoutAndVisualData() || !frame.view() || frame.view()->needsLayout())
        return;

    auto& postLayoutData = *result.postLayoutData;
    auto& visualData = *result.visualData;
    visualData.caretRectAtStart = frame.selection().absoluteCaretBounds();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone())
        return;

#if PLATFORM(GTK)
    const Editor& editor = frame.editor();
    if (selection.isRange()) {
        if (editor.selectionHasStyle(CSSPropertyFontWeight, "bold"_s) == TriState::True)
            postLayoutData.typingAttributes |= AttributeBold;
        if (editor.selectionHasStyle(CSSPropertyFontStyle, "italic"_s) == TriState::True)
            postLayoutData.typingAttributes |= AttributeItalics;
        if (editor.selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s) == TriState::True)
            postLayoutData.typingAttributes |= AttributeUnderline;
        if (editor.selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "line-through"_s) == TriState::True)
            postLayoutData.typingAttributes |= AttributeStrikeThrough;
    } else if (selection.isCaret()) {
        if (editor.selectionStartHasStyle(CSSPropertyFontWeight, "bold"_s))
            postLayoutData.typingAttributes |= AttributeBold;
        if (editor.selectionStartHasStyle(CSSPropertyFontStyle, "italic"_s))
            postLayoutData.typingAttributes |= AttributeItalics;
        if (editor.selectionStartHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s))
            postLayoutData.typingAttributes |= AttributeUnderline;
        if (editor.selectionStartHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "line-through"_s))
            postLayoutData.typingAttributes |= AttributeStrikeThrough;
    }
#endif

    if (selection.isContentEditable()) {
        auto selectionStart = selection.visibleStart();
        auto surroundingStart = startOfEditableContent(selectionStart);
        auto surroundingRange = makeSimpleRange(surroundingStart, endOfEditableContent(selectionStart));
        auto compositionRange = frame.editor().compositionRange();
        if (surroundingRange && compositionRange && contains<ComposedTree>(*surroundingRange, *compositionRange)) {
            auto beforeText = plainText({ surroundingRange->start, compositionRange->start });
            postLayoutData.surroundingContext = beforeText + plainText({ compositionRange->end, surroundingRange->end });
            postLayoutData.surroundingContextCursorPosition = beforeText.length();
            postLayoutData.surroundingContextSelectionPosition = postLayoutData.surroundingContextCursorPosition;
        } else {
            auto cursorPositionRange = makeSimpleRange(surroundingStart, selectionStart);
            auto selectionPositionRange = makeSimpleRange(surroundingStart, selection.visibleEnd());
            postLayoutData.surroundingContext = surroundingRange ? plainText(*surroundingRange) : emptyString();
            postLayoutData.surroundingContextCursorPosition = cursorPositionRange ? characterCount(*cursorPositionRange) : 0;
            postLayoutData.surroundingContextSelectionPosition = selectionPositionRange ? characterCount(*selectionPositionRange): 0;
        }
    }
}

static std::optional<InputMethodState> inputMethodSateForElement(Element* element)
{
    if (!element || !element->shouldUseInputMethod())
        return std::nullopt;

    InputMethodState state;
    if (is<HTMLInputElement>(*element)) {
        auto& inputElement = downcast<HTMLInputElement>(*element);
        state.setPurposeForInputElement(inputElement);
#if ENABLE(AUTOCAPITALIZE)
        state.addHintsForAutocapitalizeType(inputElement.autocapitalizeType());
#endif
    } else if (is<HTMLTextAreaElement>(*element) || (element->hasEditableStyle() && is<HTMLElement>(*element))) {
        auto& htmlElement = downcast<HTMLElement>(*element);
        state.setPurposeOrHintForInputMode(htmlElement.canonicalInputMode());
#if ENABLE(AUTOCAPITALIZE)
        state.addHintsForAutocapitalizeType(htmlElement.autocapitalizeType());
#endif
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

String WebPage::platformUserAgent(const URL& url) const
{
    if (url.isNull() || !m_page->settings().needsSiteSpecificQuirks())
        return String();

    return WebCore::standardUserAgentForURL(url);
}

} // namespace WebKit
