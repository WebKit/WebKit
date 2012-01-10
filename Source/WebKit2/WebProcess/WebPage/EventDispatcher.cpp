/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "EventDispatcher.h"

#include "RunLoop.h"
#include "WebEvent.h"
#include "WebEventConversion.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/Page.h>
#include <wtf/MainThread.h>

#if ENABLE(THREADED_SCROLLING)
#include <WebCore/ScrollingCoordinator.h>
#endif

using namespace WebCore;

namespace WebKit {

EventDispatcher::EventDispatcher()
{
}

EventDispatcher::~EventDispatcher()
{
}

#if ENABLE(THREADED_SCROLLING)
void EventDispatcher::addScrollingCoordinatorForPage(WebPage* webPage)
{
    MutexLocker locker(m_scrollingCoordinatorsMutex);

    ASSERT(webPage->corePage()->scrollingCoordinator());
    ASSERT(!m_scrollingCoordinators.contains(webPage->pageID()));
    m_scrollingCoordinators.set(webPage->pageID(), webPage->corePage()->scrollingCoordinator());
}

void EventDispatcher::removeScrollingCoordinatorForPage(WebPage* webPage)
{
    MutexLocker locker(m_scrollingCoordinatorsMutex);
    ASSERT(m_scrollingCoordinators.contains(webPage->pageID()));

    m_scrollingCoordinators.remove(webPage->pageID());
}
#endif

void EventDispatcher::didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments, bool& didHandleMessage)
{
    if (messageID.is<CoreIPC::MessageClassEventDispatcher>()) {
        didReceiveEventDispatcherMessageOnConnectionWorkQueue(connection, messageID, arguments, didHandleMessage);
        return;
    }
}

void EventDispatcher::wheelEvent(CoreIPC::Connection*, uint64_t pageID, const WebWheelEvent& wheelEvent)
{
#if ENABLE(THREADED_SCROLLING)
    MutexLocker locker(m_scrollingCoordinatorsMutex);
    if (ScrollingCoordinator* scrollingCoordinator = m_scrollingCoordinators.get(pageID).get()) {
        PlatformWheelEvent platformWheelEvent = platform(wheelEvent);

        if (scrollingCoordinator->handleWheelEvent(platformWheelEvent)) {
            sendDidHandleEvent(pageID, wheelEvent);
            return;
        }
    }
#endif

    RunLoop::main()->dispatch(bind(&EventDispatcher::dispatchWheelEvent, this, pageID, wheelEvent));
}

#if ENABLE(GESTURE_EVENTS)
void EventDispatcher::gestureEvent(CoreIPC::Connection*, uint64_t pageID, const WebGestureEvent& gestureEvent)
{
#if ENABLE(THREADED_SCROLLING)
    MutexLocker locker(m_scrollingCoordinatorsMutex);
    if (ScrollingCoordinator* scrollingCoordinator = m_scrollingCoordinators.get(pageID).get()) {
        PlatformGestureEvent platformGestureEvent = platform(gestureEvent);

        if (scrollingCoordinator->handleGestureEvent(platformGestureEvent)) {
            sendDidHandleEvent(pageID, gestureEvent);
            return;
        }
    }
#endif

    RunLoop::main()->dispatch(bind(&EventDispatcher::dispatchGestureEvent, this, pageID, gestureEvent));
}
#endif

void EventDispatcher::dispatchWheelEvent(uint64_t pageID, const WebWheelEvent& wheelEvent)
{
    ASSERT(isMainThread());

    WebPage* webPage = WebProcess::shared().webPage(pageID);
    if (!webPage)
        return;

    webPage->wheelEvent(wheelEvent);
}

#if ENABLE(GESTURE_EVENTS)
void EventDispatcher::dispatchGestureEvent(uint64_t pageID, const WebGestureEvent& gestureEvent)
{
    ASSERT(isMainThread());

    WebPage* webPage = WebProcess::shared().webPage(pageID);
    if (!webPage)
        return;

    webPage->gestureEvent(gestureEvent);
}
#endif

#if ENABLE(THREADED_SCROLLING)
void EventDispatcher::sendDidHandleEvent(uint64_t pageID, const WebEvent& event)
{
    WebProcess::shared().connection()->send(Messages::WebPageProxy::DidReceiveEvent(static_cast<uint32_t>(event.type()), true), pageID);
}
#endif

} // namespace WebKit
