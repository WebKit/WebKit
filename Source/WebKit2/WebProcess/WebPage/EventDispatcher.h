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

#ifndef EventDispatcher_h
#define EventDispatcher_h

#include "Connection.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadingPrimitives.h>

namespace WebCore {
    class ScrollingCoordinator;
}

namespace WebKit {

class WebEvent;
class WebPage;
class WebWheelEvent;

class EventDispatcher : public CoreIPC::Connection::QueueClient {
    WTF_MAKE_NONCOPYABLE(EventDispatcher);

public:
    EventDispatcher();
    ~EventDispatcher();

#if ENABLE(THREADED_SCROLLING)
    void addScrollingCoordinatorForPage(WebPage*);
    void removeScrollingCoordinatorForPage(WebPage*);
#endif

private:
    // CoreIPC::Connection::QueueClient
    virtual void didReceiveMessageOnConnectionWorkQueue(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, bool& didHandleMessage) OVERRIDE;

    // Implemented in generated EventDispatcherMessageReceiver.cpp
    void didReceiveEventDispatcherMessageOnConnectionWorkQueue(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder* arguments, bool& didHandleMessage);

    // Message handlers
    void wheelEvent(uint64_t pageID, const WebWheelEvent&);

    // This is called on the main thread.
    void dispatchWheelEvent(uint64_t pageID, const WebWheelEvent&);

#if ENABLE(THREADED_SCROLLING)
    void sendDidHandleEvent(uint64_t pageID, const WebEvent&);

    Mutex m_scrollingCoordinatorsMutex;
    HashMap<uint64_t, RefPtr<WebCore::ScrollingCoordinator> > m_scrollingCoordinators;
#endif
};

} // namespace WebKit

#endif // EventDispatcher_h
