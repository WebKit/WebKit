/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef GamepadController_h
#define GamepadController_h

#include "CppBoundClass.h"
#include "platform/WebGamepads.h"

namespace WebKit {
class WebGamepads;
class WebFrame;
}

class TestShell;

class GamepadController : public CppBoundClass {
public:
    explicit GamepadController(TestShell*);

    void bindToJavascript(WebKit::WebFrame*, const WebKit::WebString& classname);
    void reset();

private:
    // Bound methods and properties
    void connect(const CppArgumentList&, CppVariant*);
    void disconnect(const CppArgumentList&, CppVariant*);
    void setId(const CppArgumentList&, CppVariant*);
    void setButtonCount(const CppArgumentList&, CppVariant*);
    void setButtonData(const CppArgumentList&, CppVariant*);
    void setAxisCount(const CppArgumentList&, CppVariant*);
    void setAxisData(const CppArgumentList&, CppVariant*);
    void fallbackCallback(const CppArgumentList&, CppVariant*);

    TestShell* m_shell;

    WebKit::WebGamepads internalData;
};

#endif // GamepadController_h
