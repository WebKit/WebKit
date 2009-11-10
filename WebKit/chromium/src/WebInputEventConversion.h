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

#ifndef WebInputEventConversion_h
#define WebInputEventConversion_h

// FIXME: This relative path is a temporary hack to support using this
// header from webkit/glue.
#include "../public/WebInputEvent.h"

#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"

namespace WebCore {
class KeyboardEvent;
class MouseEvent;
class ScrollView;
class Widget;
}

namespace WebKit {

class WebMouseEvent;
class WebMouseWheelEvent;
class WebKeyboardEvent;

// These classes are used to convert from WebInputEvent subclasses to
// corresponding WebCore events.

class PlatformMouseEventBuilder : public WebCore::PlatformMouseEvent {
public:
    PlatformMouseEventBuilder(WebCore::Widget*, const WebMouseEvent&);
};

class PlatformWheelEventBuilder : public WebCore::PlatformWheelEvent {
public:
    PlatformWheelEventBuilder(WebCore::Widget*, const WebMouseWheelEvent&);
};

class PlatformKeyboardEventBuilder : public WebCore::PlatformKeyboardEvent {
public:
    PlatformKeyboardEventBuilder(const WebKeyboardEvent&);
    void setKeyType(Type);
    bool isCharacterKey() const;
};

// Converts a WebCore::MouseEvent to a corresponding WebMouseEvent. view is
// the ScrollView corresponding to the event.  Returns true if successful.
// NOTE: This is only implemented for mousemove, mouseover, mouseout,
// mousedown and mouseup.  If the event mapping fails, the event type will
// be set to Undefined.
class WebMouseEventBuilder : public WebMouseEvent {
public:
    WebMouseEventBuilder(const WebCore::ScrollView*, const WebCore::MouseEvent&);
};

// Converts a WebCore::KeyboardEvent to a corresponding WebKeyboardEvent.
// Returns true if successful.  NOTE: This is only implemented for keydown
// and keyup.  If the event mapping fails, the event type will be set to
// Undefined.
class WebKeyboardEventBuilder : public WebKeyboardEvent {
public:
    WebKeyboardEventBuilder(const WebCore::KeyboardEvent&);
};

} // namespace WebKit

#endif
