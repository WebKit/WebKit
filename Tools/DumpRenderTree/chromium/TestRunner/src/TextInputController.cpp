/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextInputController.h"

#include "TestCommon.h"
#include "WebBindings.h"
#include "WebCompositionUnderline.h"
#include "WebFrame.h"
#include "WebInputEvent.h"
#include "WebRange.h"
#include "WebView.h"
#include <public/WebString.h>
#include <public/WebVector.h>
#include <string>

using namespace WebKit;
using namespace std;

namespace WebTestRunner {

TextInputController::TextInputController()
{
    bindMethod("attributedSubstringFromRange", &TextInputController::attributedSubstringFromRange);
    bindMethod("characterIndexForPoint", &TextInputController::characterIndexForPoint);
    bindMethod("conversationIdentifier", &TextInputController::conversationIdentifier);
    bindMethod("doCommand", &TextInputController::doCommand);
    bindMethod("firstRectForCharacterRange", &TextInputController::firstRectForCharacterRange);
    bindMethod("hasMarkedText", &TextInputController::hasMarkedText);
    bindMethod("insertText", &TextInputController::insertText);
    bindMethod("makeAttributedString", &TextInputController::makeAttributedString);
    bindMethod("markedRange", &TextInputController::markedRange);
    bindMethod("selectedRange", &TextInputController::selectedRange);
    bindMethod("setMarkedText", &TextInputController::setMarkedText);
    bindMethod("substringFromRange", &TextInputController::substringFromRange);
    bindMethod("unmarkText", &TextInputController::unmarkText);
    bindMethod("validAttributesForMarkedText", &TextInputController::validAttributesForMarkedText);
    bindMethod("setComposition", &TextInputController::setComposition);
}

void TextInputController::insertText(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    m_webView->confirmComposition(WebString::fromUTF8(arguments[0].toString()));
}

void TextInputController::doCommand(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = m_webView->mainFrame();
    if (!mainFrame)
        return;

    if (arguments.size() >= 1 && arguments[0].isString())
        mainFrame->executeCommand(WebString::fromUTF8(arguments[0].toString()));
}

void TextInputController::setMarkedText(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() >= 3 && arguments[0].isString()
        && arguments[1].isNumber() && arguments[2].isNumber()) {
        WebVector<WebCompositionUnderline> underlines;
        m_webView->setComposition(WebString::fromUTF8(arguments[0].toString()),
                                  underlines,
                                  arguments[1].toInt32(),
                                  arguments[1].toInt32() + arguments[2].toInt32());
    }
}

void TextInputController::unmarkText(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    m_webView->confirmComposition();
}

void TextInputController::hasMarkedText(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = m_webView->mainFrame();
    if (!mainFrame)
        return;

    result->set(mainFrame->hasMarkedText());
}

void TextInputController::conversationIdentifier(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void TextInputController::substringFromRange(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void TextInputController::attributedSubstringFromRange(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void TextInputController::markedRange(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = m_webView->mainFrame();
    if (!mainFrame)
        return;

    WebRange range = mainFrame->markedRange();
    vector<int> intArray(2);
    intArray[0] = range.startOffset();
    intArray[1] = range.endOffset();
    result->set(WebBindings::makeIntArray(intArray));
}

void TextInputController::selectedRange(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = m_webView->mainFrame();
    if (!mainFrame)
        return;

    WebRange range = mainFrame->selectionRange();
    vector<int> intArray(2);
    intArray[0] = range.startOffset();
    intArray[1] = range.endOffset();
    result->set(WebBindings::makeIntArray(intArray));
}

void TextInputController::firstRectForCharacterRange(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    WebFrame* frame = m_webView->focusedFrame();
    if (!frame)
        return;

    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    WebRect rect;
    if (!frame->firstRectForCharacterRange(arguments[0].toInt32(), arguments[1].toInt32(), rect))
        return;

    vector<int> intArray(4);
    intArray[0] = rect.x;
    intArray[1] = rect.y;
    intArray[2] = rect.width;
    intArray[3] = rect.height;
    result->set(WebBindings::makeIntArray(intArray));
}

void TextInputController::characterIndexForPoint(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void TextInputController::validAttributesForMarkedText(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = m_webView->mainFrame();
    if (!mainFrame)
        return;

    result->set("NSUnderline,NSUnderlineColor,NSMarkedClauseSegment,"
                "NSTextInputReplacementRangeAttributeName");
}

void TextInputController::makeAttributedString(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
    result->setNull();
}

void TextInputController::setComposition(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() < 1)
        return;

    // Sends a keydown event with key code = 0xE5 to emulate input method behavior.
    WebKeyboardEvent keyDown;
    keyDown.type = WebInputEvent::RawKeyDown;
    keyDown.modifiers = 0;
    keyDown.windowsKeyCode = 0xE5; // VKEY_PROCESSKEY
    keyDown.setKeyIdentifierFromWindowsKeyCode();
    m_webView->handleInputEvent(keyDown);

    WebVector<WebCompositionUnderline> underlines;
    WebString text(WebString::fromUTF8(arguments[0].toString()));
    m_webView->setComposition(text, underlines, 0, text.length());
}

}
