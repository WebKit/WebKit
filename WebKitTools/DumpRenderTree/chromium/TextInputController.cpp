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

#include "TestShell.h"
#include "public/WebBindings.h"
#include "public/WebFrame.h"
#include "public/WebRange.h"
#include "public/WebString.h"
#include "public/WebView.h"
#include <wtf/StringExtras.h>
#include <string>

using namespace WebKit;
using namespace std;

TestShell* TextInputController::testShell = 0;

TextInputController::TextInputController(TestShell* shell)
{
    // Set static testShell variable. Be careful not to assign testShell to new
    // windows which are temporary.
    if (!testShell)
        testShell = shell;

    bindMethod("attributedSubstringFromRange", &TextInputController::attributedSubstringFromRange);
    bindMethod("characterIndexForPoint", &TextInputController::characterIndexForPoint);
    bindMethod("conversationIdentifier", &TextInputController::conversationIdentifier);
    bindMethod("doCommand", &TextInputController::doCommand);
    bindMethod("firstRectForCharacterRange", &TextInputController::firstRectForCharacterRange);
    bindMethod("hasMarkedText", &TextInputController::hasMarkedText);
    bindMethod("hasSpellingMarker", &TextInputController::hasSpellingMarker);
    bindMethod("insertText", &TextInputController::insertText);
    bindMethod("makeAttributedString", &TextInputController::makeAttributedString);
    bindMethod("markedRange", &TextInputController::markedRange);
    bindMethod("selectedRange", &TextInputController::selectedRange);
    bindMethod("setMarkedText", &TextInputController::setMarkedText);
    bindMethod("substringFromRange", &TextInputController::substringFromRange);
    bindMethod("unmarkText", &TextInputController::unmarkText);
    bindMethod("validAttributesForMarkedText", &TextInputController::validAttributesForMarkedText);
}

WebFrame* TextInputController::getMainFrame()
{
    return testShell->webView()->mainFrame();
}

void TextInputController::insertText(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;
    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    if (mainFrame->hasMarkedText()) {
        mainFrame->unmarkText();
        mainFrame->replaceSelection(WebString());
    }
    mainFrame->insertText(WebString::fromUTF8(arguments[0].toString()));
}

void TextInputController::doCommand(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;

    if (arguments.size() >= 1 && arguments[0].isString())
        mainFrame->executeCommand(WebString::fromUTF8(arguments[0].toString()));
}

void TextInputController::setMarkedText(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;

    if (arguments.size() >= 3 && arguments[0].isString()
        && arguments[1].isNumber() && arguments[2].isNumber()) {
        mainFrame->setMarkedText(WebString::fromUTF8(arguments[0].toString()),
                                 arguments[1].toInt32(),
                                 arguments[2].toInt32());
    }
}

void TextInputController::unmarkText(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;

    mainFrame->unmarkText();
}

void TextInputController::hasMarkedText(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
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

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;

    WebRange range = mainFrame->markedRange();
    char buffer[30];
    snprintf(buffer, 30, "%d,%d", range.startOffset(), range.endOffset());
    result->set(string(buffer));
}

void TextInputController::selectedRange(const CppArgumentList&, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;

    WebRange range = mainFrame->selectionRange();
    char buffer[30];
    snprintf(buffer, 30, "%d,%d", range.startOffset(), range.endOffset());
    result->set(string(buffer));
}

void TextInputController::firstRectForCharacterRange(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;

    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;

    WebRect rect;
    if (!mainFrame->firstRectForCharacterRange(arguments[0].toInt32(), arguments[1].toInt32(), rect))
        return;

    Vector<int> intArray(4);
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

    WebFrame* mainFrame = getMainFrame();
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

void TextInputController::hasSpellingMarker(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 2 || !arguments[0].isNumber() || !arguments[1].isNumber())
        return;
    WebFrame* mainFrame = getMainFrame();
    if (!mainFrame)
        return;
    // Returns as a number for a compatibility reason.
    result->set(mainFrame->selectionStartHasSpellingMarkerFor(arguments[0].toInt32(), arguments[1].toInt32()) ? 1 : 0);
}
