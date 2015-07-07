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

#if ENABLE(REMOTE_INSPECTOR)

#ifndef RemoteInspectorDebuggableConnection_h
#define RemoteInspectorDebuggableConnection_h

#import "InspectorFrontendChannel.h"
#import "RemoteInspectorDebuggable.h"
#import <dispatch/dispatch.h>
#import <mutex>
#import <wtf/RetainPtr.h>
#import <wtf/ThreadSafeRefCounted.h>

OBJC_CLASS NSString;

namespace Inspector {
    
class RemoteInspectorBlock {
public:
    RemoteInspectorBlock(void (^task)())
        : m_task(Block_copy(task))
    {
    }

    RemoteInspectorBlock(const RemoteInspectorBlock& other)
        : m_task(Block_copy(other.m_task))
    {
    }

    ~RemoteInspectorBlock()
    {
        Block_release(m_task);
    }

    RemoteInspectorBlock& operator=(const RemoteInspectorBlock& other)
    {
        void (^oldTask)() = m_task;
        m_task = Block_copy(other.m_task);
        Block_release(oldTask);
        return *this;
    }

    void operator()() const
    {
        m_task();
    }

private:
    void (^m_task)();
};

typedef Vector<RemoteInspectorBlock> RemoteInspectorQueue;

class RemoteInspectorDebuggableConnection final : public ThreadSafeRefCounted<RemoteInspectorDebuggableConnection>, public InspectorFrontendChannel {
public:
    RemoteInspectorDebuggableConnection(RemoteInspectorDebuggable*, NSString *connectionIdentifier, NSString *destination, RemoteInspectorDebuggable::DebuggableType);
    virtual ~RemoteInspectorDebuggableConnection();

    NSString *destination() const;
    NSString *connectionIdentifier() const;
    unsigned identifier() const { return m_identifier; }

    bool setup();

    void close();
    void closeFromDebuggable();

    void sendMessageToBackend(NSString *);
    virtual bool sendMessageToFrontend(const String&) override;

    std::mutex& queueMutex() { return m_queueMutex; }
    RemoteInspectorQueue queue() const { return m_queue; }
    void clearQueue() { m_queue.clear(); }

private:
    void dispatchAsyncOnDebuggable(void (^block)());

    void setupRunLoop();
    void teardownRunLoop();
    void queueTaskOnPrivateRunLoop(void (^block)());

    // This connection from the RemoteInspector singleton to the Debuggable
    // can be used on multiple threads. So any access to the debuggable
    // itself must take this mutex to ensure m_debuggable is valid.
    std::mutex m_debuggableMutex;

    // If a debuggable has a specific run loop it wants to evaluate on
    // we setup our run loop sources on that specific run loop.
    RetainPtr<CFRunLoopRef> m_runLoop;
    RetainPtr<CFRunLoopSourceRef> m_runLoopSource;
    RemoteInspectorQueue m_queue;
    std::mutex m_queueMutex;

    RemoteInspectorDebuggable* m_debuggable;
    RetainPtr<NSString> m_connectionIdentifier;
    RetainPtr<NSString> m_destination;
    unsigned m_identifier;
    bool m_connected;
};

} // namespace Inspector

#endif // RemoteInspectorDebuggableConnection_h

#endif // ENABLE(REMOTE_INSPECTOR)
