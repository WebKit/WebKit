/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "RemoteInspectorDebuggableConnection.h"

#if ENABLE(REMOTE_INSPECTOR)

#import "EventLoop.h"
#import "RemoteInspector.h"
#import <wtf/Vector.h>

#if PLATFORM(IOS)
#import <wtf/ios/WebCoreThread.h>
#endif

namespace Inspector {

static std::mutex* rwiQueueMutex;
static CFRunLoopSourceRef rwiRunLoopSource;
static RemoteInspectorQueue* rwiQueue;

static void RemoteInspectorHandleRunSourceGlobal(void*)
{
    ASSERT(CFRunLoopGetCurrent() == CFRunLoopGetMain());
    ASSERT(rwiQueueMutex);
    ASSERT(rwiRunLoopSource);
    ASSERT(rwiQueue);

    RemoteInspectorQueue queueCopy;
    {
        std::lock_guard<std::mutex> lock(*rwiQueueMutex);
        queueCopy = *rwiQueue;
        rwiQueue->clear();
    }

    for (const auto& block : queueCopy)
        block();
}

static void RemoteInspectorQueueTaskOnGlobalQueue(void (^task)())
{
    ASSERT(rwiQueueMutex);
    ASSERT(rwiRunLoopSource);
    ASSERT(rwiQueue);

    {
        std::lock_guard<std::mutex> lock(*rwiQueueMutex);
        rwiQueue->append(RemoteInspectorBlock(task));
    }

    CFRunLoopSourceSignal(rwiRunLoopSource);
    CFRunLoopWakeUp(CFRunLoopGetMain());
}

static void RemoteInspectorInitializeGlobalQueue()
{
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        rwiQueue = new RemoteInspectorQueue;
        rwiQueueMutex = std::make_unique<std::mutex>().release();

        CFRunLoopSourceContext runLoopSourceContext = {0, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RemoteInspectorHandleRunSourceGlobal};
        rwiRunLoopSource = CFRunLoopSourceCreate(nullptr, 1, &runLoopSourceContext);

        // Add to the default run loop mode for default handling, and the JSContext remote inspector run loop mode when paused.
        CFRunLoopAddSource(CFRunLoopGetMain(), rwiRunLoopSource, kCFRunLoopDefaultMode);
        CFRunLoopAddSource(CFRunLoopGetMain(), rwiRunLoopSource, EventLoop::remoteInspectorRunLoopMode());
    });
}

static void RemoteInspectorHandleRunSourceWithInfo(void* info)
{
    RemoteInspectorDebuggableConnection *debuggableConnection = static_cast<RemoteInspectorDebuggableConnection*>(info);

    RemoteInspectorQueue queueCopy;
    {
        std::lock_guard<std::mutex> lock(debuggableConnection->queueMutex());
        queueCopy = debuggableConnection->queue();
        debuggableConnection->clearQueue();
    }

    for (const auto& block : queueCopy)
        block();
}


RemoteInspectorDebuggableConnection::RemoteInspectorDebuggableConnection(RemoteInspectorDebuggable* debuggable, NSString *connectionIdentifier, NSString *destination, RemoteInspectorDebuggable::DebuggableType)
    : m_debuggable(debuggable)
    , m_connectionIdentifier(connectionIdentifier)
    , m_destination(destination)
    , m_identifier(debuggable->identifier())
    , m_connected(false)
{
    setupRunLoop();
}

RemoteInspectorDebuggableConnection::~RemoteInspectorDebuggableConnection()
{
    teardownRunLoop();
}

NSString *RemoteInspectorDebuggableConnection::destination() const
{
    return [[m_destination copy] autorelease];
}

NSString *RemoteInspectorDebuggableConnection::connectionIdentifier() const
{
    return [[m_connectionIdentifier copy] autorelease];
}

void RemoteInspectorDebuggableConnection::dispatchAsyncOnDebuggable(void (^block)())
{
    if (m_runLoop) {
        queueTaskOnPrivateRunLoop(block);
        return;
    }

#if PLATFORM(IOS)
    if (WebCoreWebThreadIsEnabled && WebCoreWebThreadIsEnabled()) {
        WebCoreWebThreadRun(block);
        return;
    }
#endif

    RemoteInspectorQueueTaskOnGlobalQueue(block);
}

bool RemoteInspectorDebuggableConnection::setup()
{
    std::lock_guard<std::mutex> lock(m_debuggableMutex);

    if (!m_debuggable)
        return false;

    ref();
    dispatchAsyncOnDebuggable(^{
        {
            std::lock_guard<std::mutex> lock(m_debuggableMutex);
            if (!m_debuggable || !m_debuggable->remoteDebuggingAllowed() || m_debuggable->hasLocalDebugger()) {
                RemoteInspector::shared().setupFailed(identifier());
                m_debuggable = nullptr;
            } else {
                m_debuggable->connect(this);
                m_connected = true;
            }
        }
        deref();
    });

    return true;
}

void RemoteInspectorDebuggableConnection::closeFromDebuggable()
{
    std::lock_guard<std::mutex> lock(m_debuggableMutex);

    m_debuggable = nullptr;
}

void RemoteInspectorDebuggableConnection::close()
{
    ref();
    dispatchAsyncOnDebuggable(^{
        {
            std::lock_guard<std::mutex> lock(m_debuggableMutex);

            if (m_debuggable) {
                if (m_connected)
                    m_debuggable->disconnect();

                m_debuggable = nullptr;
            }
        }
        deref();
    });
}

void RemoteInspectorDebuggableConnection::sendMessageToBackend(NSString *message)
{
    ref();
    dispatchAsyncOnDebuggable(^{
        {
            RemoteInspectorDebuggable* debuggable = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_debuggableMutex);
                if (!m_debuggable)
                    return;
                debuggable = m_debuggable;
            }

            debuggable->dispatchMessageFromRemoteFrontend(message);
        }
        deref();
    });
}

bool RemoteInspectorDebuggableConnection::sendMessageToFrontend(const String& message)
{
    RemoteInspector::shared().sendMessageToRemoteFrontend(identifier(), message);

    return true;
}

void RemoteInspectorDebuggableConnection::setupRunLoop()
{
    CFRunLoopRef debuggerRunLoop = m_debuggable->debuggerRunLoop();
    if (!debuggerRunLoop) {
        RemoteInspectorInitializeGlobalQueue();
        return;
    }

    m_runLoop = debuggerRunLoop;

    CFRunLoopSourceContext runLoopSourceContext = {0, this, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, RemoteInspectorHandleRunSourceWithInfo};
    m_runLoopSource = CFRunLoopSourceCreate(nullptr, 1, &runLoopSourceContext);

    CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopDefaultMode);
    CFRunLoopAddSource(m_runLoop.get(), m_runLoopSource.get(), EventLoop::remoteInspectorRunLoopMode());
}

void RemoteInspectorDebuggableConnection::teardownRunLoop()
{
    if (!m_runLoop)
        return;

    CFRunLoopRemoveSource(m_runLoop.get(), m_runLoopSource.get(), kCFRunLoopDefaultMode);
    CFRunLoopRemoveSource(m_runLoop.get(), m_runLoopSource.get(), EventLoop::remoteInspectorRunLoopMode());

    m_runLoop = nullptr;
    m_runLoopSource = nullptr;
}

void RemoteInspectorDebuggableConnection::queueTaskOnPrivateRunLoop(void (^block)())
{
    ASSERT(m_runLoop);

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.append(RemoteInspectorBlock(block));
    }

    CFRunLoopSourceSignal(m_runLoopSource.get());
    CFRunLoopWakeUp(m_runLoop.get());
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
