/*
 * Copyright (C) 2012, 2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef UserInputBridge_h
#define UserInputBridge_h

#include "ScrollTypes.h"
#include <wtf/Noncopyable.h>

namespace JSC {
class InputCursor;
}

namespace WebCore {

struct FrameLoadRequest;

class Frame;
class Page;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;

// Real user inputs come from WebKit or WebKit2.
// Synthetic inputs come from within WebCore (i.e., from web replay or fake mouse moves).
enum class InputSource {
    User,
    Synthetic
};

class UserInputBridge {
    WTF_MAKE_NONCOPYABLE(UserInputBridge);
public:
    UserInputBridge(Page&);

#if ENABLE(WEB_REPLAY)
    enum class State {
        Capturing,
        Open,
        Replaying,
    };

    void setState(State bridgeState) { m_state = bridgeState; }
    State state() const { return m_state; }

    JSC::InputCursor& activeCursor() const;
#endif

    // User input APIs.
#if ENABLE(CONTEXT_MENUS)
    bool handleContextMenuEvent(const PlatformMouseEvent&, const Frame*, InputSource source = InputSource::User);
#endif
    bool handleMousePressEvent(const PlatformMouseEvent&, InputSource source = InputSource::User);
    bool handleMouseReleaseEvent(const PlatformMouseEvent&, InputSource source = InputSource::User);
    bool handleMouseMoveEvent(const PlatformMouseEvent&, InputSource source = InputSource::User);
    bool handleMouseMoveOnScrollbarEvent(const PlatformMouseEvent&, InputSource source = InputSource::User);
    bool handleWheelEvent(const PlatformWheelEvent&, InputSource source = InputSource::User);
    bool handleKeyEvent(const PlatformKeyboardEvent&, InputSource source = InputSource::User);
    bool handleAccessKeyEvent(const PlatformKeyboardEvent&, InputSource source = InputSource::User);
    void focusSetActive(bool active, InputSource source = InputSource::User);
    void focusSetFocused(bool focused, InputSource source = InputSource::User);
    bool scrollRecursively(ScrollDirection, ScrollGranularity, InputSource source = InputSource::User);
    bool logicalScrollRecursively(ScrollLogicalDirection, ScrollGranularity, InputSource source = InputSource::User);

    // Navigation APIs.
    void loadRequest(const FrameLoadRequest&, InputSource source = InputSource::User);
    void reloadFrame(Frame*, bool endToEndReload, InputSource source = InputSource::User);
    void stopLoadingFrame(Frame*, InputSource source = InputSource::User);
    bool tryClosePage(InputSource source = InputSource::User);

private:
    Page& m_page;
#if ENABLE(WEB_REPLAY)
    State m_state;
#endif
};

} // namespace WebCore

#endif // UserInputBridge_h
