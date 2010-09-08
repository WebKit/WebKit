/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WorkQueue_h
#define WorkQueue_h

#if PLATFORM(MAC)
#if HAVE(DISPATCH_H)
#include <dispatch/dispatch.h>
#endif
#endif

#include "WorkItem.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>

#if PLATFORM(QT)
class QLocalSocket;
class QObject;
class QThread;
#endif

class WorkQueue {
public:
    explicit WorkQueue(const char* name);
    ~WorkQueue();

    void scheduleWork(PassOwnPtr<WorkItem>);
    void invalidate();

#if PLATFORM(MAC)
    enum MachPortEventType {
        // Fired when there is data on the given receive right.
        MachPortDataAvailable,
        
        // Fired when the receive right for this send right has been destroyed.
        MachPortDeadNameNotification
    };
    
    // Will execute the given work item whenever the given mach port event fires.
    // Note that this will adopt the mach port and destroy it when the work queue is invalidated.
    void registerMachPortEventHandler(mach_port_t, MachPortEventType, PassOwnPtr<WorkItem>);
    void unregisterMachPortEventHandler(mach_port_t);
#elif PLATFORM(WIN)
    void registerHandle(HANDLE, PassOwnPtr<WorkItem>);
    void unregisterAndCloseHandle(HANDLE);
#elif PLATFORM(QT)
    void connectSignal(QObject*, const char* signal, PassOwnPtr<WorkItem>);
    void disconnectSignal(QObject*, const char* signal);

    void moveSocketToWorkThread(QLocalSocket*);
#endif

private:
    // FIXME: Use an atomic boolean here instead.
    Mutex m_isValidMutex;
    bool m_isValid;

    void platformInitialize(const char* name);
    void platformInvalidate();

#if PLATFORM(MAC)
#if HAVE(DISPATCH_H)
    static void executeWorkItem(void*);
    Mutex m_eventSourcesMutex;
    class EventSource;
    HashMap<mach_port_t, EventSource*> m_eventSources;
    dispatch_queue_t m_dispatchQueue;
#endif
#elif PLATFORM(WIN)
    class WorkItemWin : public RefCounted<WorkItemWin> {
    public:
        static PassRefPtr<WorkItemWin> create(PassOwnPtr<WorkItem>, WorkQueue*);
        virtual ~WorkItemWin();

        WorkItem* item() const { return m_item.get(); }
        WorkQueue* queue() const { return m_queue; }

    protected:
        WorkItemWin(PassOwnPtr<WorkItem>, WorkQueue*);

    private:
        OwnPtr<WorkItem> m_item;
        WorkQueue* m_queue;
    };

    class HandleWorkItem : public WorkItemWin {
    public:
        static PassRefPtr<HandleWorkItem> createByAdoptingHandle(HANDLE, PassOwnPtr<WorkItem>, WorkQueue*);
        virtual ~HandleWorkItem();

        void setWaitHandle(HANDLE waitHandle) { m_waitHandle = waitHandle; }
        HANDLE waitHandle() const { return m_waitHandle; }

    private:
        HandleWorkItem(HANDLE, PassOwnPtr<WorkItem>, WorkQueue*);

        HANDLE m_handle;
        HANDLE m_waitHandle;
    };

    static void CALLBACK handleCallback(void* context, BOOLEAN timerOrWaitFired);
    static DWORD WINAPI workThreadCallback(void* context);

    bool tryRegisterAsWorkThread();
    void unregisterAsWorkThread();
    void performWorkOnRegisteredWorkThread();

    static void unregisterWaitAndDestroyItemSoon(PassRefPtr<HandleWorkItem>);
    static DWORD WINAPI unregisterWaitAndDestroyItemCallback(void* context);

    volatile LONG m_isWorkThreadRegistered;

    Mutex m_workItemQueueLock;
    Vector<RefPtr<WorkItemWin> > m_workItemQueue;

    Mutex m_handlesLock;
    HashMap<HANDLE, RefPtr<HandleWorkItem> > m_handles;
#elif PLATFORM(QT)
    class WorkItemQt;
    HashMap<QObject*, WorkItemQt*> m_signalListeners;
    QThread* m_workThread;
    friend class WorkItemQt;
#endif
};

#endif // WorkQueue_h
