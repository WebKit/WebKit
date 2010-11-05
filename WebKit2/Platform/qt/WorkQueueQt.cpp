/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "WorkQueue.h"

#include <QLocalSocket>
#include <QObject>
#include <QThread>
#include <wtf/Threading.h>
#include "NotImplemented.h"

class WorkQueue::WorkItemQt : public QObject {
    Q_OBJECT
public:
    WorkItemQt(WorkQueue* workQueue, WorkItem* workItem)
        : m_queue(workQueue)
        , m_source(0)
        , m_signal(0)
        , m_workItem(workItem)
    {
    }

    WorkItemQt(WorkQueue* workQueue, QObject* source, const char* signal, WorkItem* workItem)
        : m_queue(workQueue)
        , m_source(source)
        , m_signal(signal)
        , m_workItem(workItem)
    {
        connect(m_source, m_signal, SLOT(execute()), Qt::QueuedConnection);
    }

    ~WorkItemQt()
    {
        delete m_workItem;
    }

    Q_SLOT void execute() 
    { 
        if (m_queue->m_isValid)
            m_workItem->execute();
    }

    virtual void timerEvent(QTimerEvent*)
    {
        execute(); 
        delete this;
    }

    WorkQueue* m_queue;
    QObject* m_source;
    const char* m_signal;
    WorkItem* m_workItem;
};

void WorkQueue::connectSignal(QObject* o, const char* signal, PassOwnPtr<WorkItem> workItem)
{
    WorkQueue::WorkItemQt* itemQt = new WorkQueue::WorkItemQt(this, o, signal, workItem.leakPtr());
    itemQt->moveToThread(m_workThread);
    m_signalListeners.add(o, itemQt);
}

void WorkQueue::disconnectSignal(QObject* o, const char* name)
{
    HashMap<QObject*, WorkItemQt*>::iterator it = m_signalListeners.find(o);
    for (; it != m_signalListeners.end(); ++it) {
        if (strcmp(it->second->m_signal, name))
            continue;
        delete it->second;
        m_signalListeners.remove(it);
        return;
    }
}

void WorkQueue::moveSocketToWorkThread(QLocalSocket* socket)
{
    ASSERT(m_workThread);
    ASSERT(socket);

    socket->setParent(0);
    socket->moveToThread(m_workThread);
}

void WorkQueue::platformInitialize(const char*)
{
    m_workThread = new QThread();
    m_workThread->start();
}

void WorkQueue::platformInvalidate()
{
    m_workThread->exit();
    m_workThread->wait();
    delete m_workThread;
    deleteAllValues(m_signalListeners);
}

void WorkQueue::scheduleWork(PassOwnPtr<WorkItem> item)
{
    WorkQueue::WorkItemQt* itemQt = new WorkQueue::WorkItemQt(this, item.leakPtr());
    itemQt->startTimer(0);
    itemQt->moveToThread(m_workThread);
}

void WorkQueue::scheduleWorkAfterDelay(PassOwnPtr<WorkItem>, double)
{
    notImplemented();
}

#include "WorkQueueQt.moc"
