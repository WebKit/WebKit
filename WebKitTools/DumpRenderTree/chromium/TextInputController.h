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

// TextInputController is bound to window.textInputController in Javascript
// when DRT is running. Layout tests use it to exercise various corners of
// text input.

#ifndef TextInputController_h
#define TextInputController_h

#include "CppBoundClass.h"

class TestShell;

namespace WebKit {
class WebFrame;
}

class TextInputController : public CppBoundClass {
public:
    TextInputController(TestShell*);

    void insertText(const CppArgumentList&, CppVariant*);
    void doCommand(const CppArgumentList&, CppVariant*);
    void setMarkedText(const CppArgumentList&, CppVariant*);
    void unmarkText(const CppArgumentList&, CppVariant*);
    void hasMarkedText(const CppArgumentList&, CppVariant*);
    void conversationIdentifier(const CppArgumentList&, CppVariant*);
    void substringFromRange(const CppArgumentList&, CppVariant*);
    void attributedSubstringFromRange(const CppArgumentList&, CppVariant*);
    void markedRange(const CppArgumentList&, CppVariant*);
    void selectedRange(const CppArgumentList&, CppVariant*);
    void firstRectForCharacterRange(const CppArgumentList&, CppVariant*);
    void characterIndexForPoint(const CppArgumentList&, CppVariant*);
    void validAttributesForMarkedText(const CppArgumentList&, CppVariant*);
    void makeAttributedString(const CppArgumentList&, CppVariant*);
    void hasSpellingMarker(const CppArgumentList&, CppVariant*);

private:
    // Returns the test shell's main WebFrame.
    static WebKit::WebFrame* getMainFrame();

    // Non-owning pointer. The TextInputController is owned by the TestShell.
    static TestShell* testShell;
};

#endif // TextInputController_h
