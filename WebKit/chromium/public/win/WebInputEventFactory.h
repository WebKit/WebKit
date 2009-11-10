/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebInputEventFactory_h
#define WebInputEventFactory_h

#include "../WebCommon.h"

#include <windows.h>

namespace WebKit {

class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;

class WebInputEventFactory {
public:
    WEBKIT_API static WebKeyboardEvent keyboardEvent(HWND, UINT, WPARAM, LPARAM);
    WEBKIT_API static WebMouseEvent mouseEvent(HWND, UINT, WPARAM, LPARAM);
    WEBKIT_API static WebMouseWheelEvent mouseWheelEvent(HWND, UINT, WPARAM, LPARAM);

    // Windows only provides information on whether a click was a single or
    // double click, while we need to know the click count past two. The
    // WebInputEventFactory keeps internal state to allow it to synthesize
    // that information. In some cases, like fast-running tests, that
    // information is known to be stale and needs to be reset; that is the
    // function of resetLastClickState().
    WEBKIT_API static void resetLastClickState();
};

} // namespace WebKit

#endif
