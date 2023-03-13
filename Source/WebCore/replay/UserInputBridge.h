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

#pragma once

#include "FrameLoaderTypes.h"
#include "ScrollTypes.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class FrameLoadRequest;
class LocalFrame;
class Page;
class PlatformKeyboardEvent;
class PlatformMouseEvent;
class PlatformWheelEvent;

enum class WheelEventProcessingSteps : uint8_t;

// Real user inputs come from WebKit or WebKit2.
// Synthetic inputs come from within WebCore (i.e., from web replay or fake mouse moves).
enum class InputSource {
    User,
    Synthetic
};

class UserInputBridge {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(UserInputBridge);
public:
    UserInputBridge(Page&);

    // User input APIs.
#if ENABLE(CONTEXT_MENU_EVENT)
    WEBCORE_EXPORT bool handleContextMenuEvent(const PlatformMouseEvent&, LocalFrame&, InputSource = InputSource::User);
#endif
    WEBCORE_EXPORT bool handleMousePressEvent(const PlatformMouseEvent&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleMouseReleaseEvent(const PlatformMouseEvent&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleMouseMoveEvent(const PlatformMouseEvent&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleMouseMoveOnScrollbarEvent(const PlatformMouseEvent&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleMouseForceEvent(const PlatformMouseEvent&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleWheelEvent(const PlatformWheelEvent&, OptionSet<WheelEventProcessingSteps>, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleKeyEvent(const PlatformKeyboardEvent&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool handleAccessKeyEvent(const PlatformKeyboardEvent&, InputSource = InputSource::User);
    void focusSetActive(bool active, InputSource = InputSource::User);
    void focusSetFocused(bool focused, InputSource = InputSource::User);
    WEBCORE_EXPORT bool scrollRecursively(ScrollDirection, ScrollGranularity, InputSource = InputSource::User);
    WEBCORE_EXPORT bool logicalScrollRecursively(ScrollLogicalDirection, ScrollGranularity, InputSource = InputSource::User);

    // Navigation APIs.
    WEBCORE_EXPORT void loadRequest(FrameLoadRequest&&, InputSource = InputSource::User);
    WEBCORE_EXPORT void reloadFrame(LocalFrame&, OptionSet<ReloadOption>, InputSource = InputSource::User);
    WEBCORE_EXPORT void stopLoadingFrame(LocalFrame&, InputSource = InputSource::User);
    WEBCORE_EXPORT bool tryClosePage(InputSource = InputSource::User);

private:
    Page& m_page;
};

} // namespace WebCore
