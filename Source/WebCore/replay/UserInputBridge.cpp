/*
 * Copyright (C) 2013 University of Washington. All rights reserved.
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

#include "config.h"
#include "UserInputBridge.h"

#include "EventHandler.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "MainFrame.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"

#if ENABLE(WEB_REPLAY)
#include "ReplayController.h"
#include "SerializationMethods.h"
#include "WebReplayInputs.h"
#include <replay/InputCursor.h>
#endif

#define EARLY_RETURN_IF_SHOULD_IGNORE_INPUT \
    do { \
        if (inputSource == InputSource::User && m_state == UserInputBridge::State::Replaying) \
            return true; \
    } while (false)

namespace WebCore {

UserInputBridge::UserInputBridge(Page& page)
    : m_page(page)
#if ENABLE(WEB_REPLAY)
    , m_state(UserInputBridge::State::Open)
#endif
{
}

#if ENABLE(WEB_REPLAY)
InputCursor& UserInputBridge::activeCursor() const
{
    return m_page.replayController().activeInputCursor();
}
#endif

#if ENABLE(CONTEXT_MENUS)
bool UserInputBridge::handleContextMenuEvent(const PlatformMouseEvent& mouseEvent, const Frame* frame, InputSource)
{
    return frame->eventHandler().sendContextMenuEvent(mouseEvent);
}
#endif

bool UserInputBridge::handleMousePressEvent(const PlatformMouseEvent& mouseEvent, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing()) {
        std::unique_ptr<PlatformMouseEvent> ownedEvent = std::make_unique<PlatformMouseEvent>(mouseEvent);
        cursor.appendInput<HandleMousePress>(std::move(ownedEvent));
    }
    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.mainFrame().eventHandler().handleMousePressEvent(mouseEvent);
}

bool UserInputBridge::handleMouseReleaseEvent(const PlatformMouseEvent& mouseEvent, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing()) {
        std::unique_ptr<PlatformMouseEvent> ownedEvent = std::make_unique<PlatformMouseEvent>(mouseEvent);
        cursor.appendInput<HandleMouseRelease>(std::move(ownedEvent));
    }
    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.mainFrame().eventHandler().handleMouseReleaseEvent(mouseEvent);
}

bool UserInputBridge::handleMouseMoveEvent(const PlatformMouseEvent& mouseEvent, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing()) {
        std::unique_ptr<PlatformMouseEvent> ownedEvent = std::make_unique<PlatformMouseEvent>(mouseEvent);
        cursor.appendInput<HandleMouseMove>(std::move(ownedEvent), false);
    }
    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.mainFrame().eventHandler().mouseMoved(mouseEvent);
}

bool UserInputBridge::handleMouseMoveOnScrollbarEvent(const PlatformMouseEvent& mouseEvent, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing()) {
        std::unique_ptr<PlatformMouseEvent> ownedEvent = std::make_unique<PlatformMouseEvent>(mouseEvent);
        cursor.appendInput<HandleMouseMove>(std::move(ownedEvent), true);
    }
    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.mainFrame().eventHandler().passMouseMovedEventToScrollbars(mouseEvent);
}

bool UserInputBridge::handleKeyEvent(const PlatformKeyboardEvent& keyEvent, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing()) {
        std::unique_ptr<PlatformKeyboardEvent> ownedEvent = std::make_unique<PlatformKeyboardEvent>(keyEvent);
        cursor.appendInput<HandleKeyPress>(std::move(ownedEvent));
    }
    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.focusController().focusedOrMainFrame().eventHandler().keyEvent(keyEvent);
}

bool UserInputBridge::handleAccessKeyEvent(const PlatformKeyboardEvent& keyEvent, InputSource)
{
    return m_page.focusController().focusedOrMainFrame().eventHandler().handleAccessKey(keyEvent);
}

bool UserInputBridge::handleWheelEvent(const PlatformWheelEvent& wheelEvent, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing()) {
        std::unique_ptr<PlatformWheelEvent> ownedEvent = std::make_unique<PlatformWheelEvent>(wheelEvent);
        cursor.appendInput<HandleWheelEvent>(std::move(ownedEvent));
    }
    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.mainFrame().eventHandler().handleWheelEvent(wheelEvent);
}

void UserInputBridge::focusSetActive(bool active, InputSource)
{
    m_page.focusController().setActive(active);
}

void UserInputBridge::focusSetFocused(bool focused, InputSource)
{
    m_page.focusController().setFocused(focused);
}

bool UserInputBridge::scrollRecursively(ScrollDirection direction, ScrollGranularity granularity, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing())
        cursor.appendInput<ScrollPage>(direction, granularity);

    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.focusController().focusedOrMainFrame().eventHandler().scrollRecursively(direction, granularity, nullptr);
}

bool UserInputBridge::logicalScrollRecursively(ScrollLogicalDirection direction, ScrollGranularity granularity, InputSource inputSource)
{
#if ENABLE(WEB_REPLAY)
    EARLY_RETURN_IF_SHOULD_IGNORE_INPUT;

    InputCursor& cursor = activeCursor();
    if (cursor.isCapturing())
        cursor.appendInput<LogicalScrollPage>(direction, granularity);

    EventLoopInputExtent extent(cursor);
#else
    UNUSED_PARAM(inputSource);
#endif

    return m_page.focusController().focusedOrMainFrame().eventHandler().logicalScrollRecursively(direction, granularity, nullptr);
}

void UserInputBridge::loadRequest(const FrameLoadRequest& request, InputSource)
{
    m_page.mainFrame().loader().load(request);
}

void UserInputBridge::reloadFrame(Frame* frame, bool endToEndReload, InputSource)
{
    frame->loader().reload(endToEndReload);
}

void UserInputBridge::stopLoadingFrame(Frame* frame, InputSource)
{
    frame->loader().stopForUserCancel();
}

bool UserInputBridge::tryClosePage(InputSource)
{
    return m_page.mainFrame().loader().shouldClose();
}

} // namespace WebCore
