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

#include "config.h"
#include "WorkQueue.h"

#include <QLocalSocket>
#include <QObject>
#include <QThread>
#include <QProcess>
#include <WebCore/NotImplemented.h>
#include <wtf/Threading.h>

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

QSocketNotifier* WorkQueue::registerSocketEventHandler(int socketDescriptor, QSocketNotifier::Type type, PassOwnPtr<WorkItem> workItem)
{
    ASSERT(m_workThread);

    QSocketNotifier* notifier = new QSocketNotifier(socketDescriptor, type, 0);
    notifier->setEnabled(false);
    notifier->moveToThread(m_workThread);
    WorkQueue::WorkItemQt* itemQt = new WorkQueue::WorkItemQt(this, notifier, SIGNAL(activated(int)), workItem.leakPtr());
    itemQt->moveToThread(m_workThread);
    notifier->setEnabled(true);
    return notifier;
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

void WorkQueue::scheduleWorkAfterDelay(PassOwnPtr<WorkItem> item, double delayInSecond)
{
    WorkQueue::WorkItemQt* itemQt = new WorkQueue::WorkItemQt(this, item.leakPtr());
    itemQt->startTimer(static_cast<int>(delayInSecond * 1000));
    itemQt->moveToThread(m_workThread);
}

void WorkQueue::scheduleWorkOnTermination(WebKit::PlatformProcessIdentifier process, PassOwnPtr<WorkItem> workItem)
{
    WorkQueue::WorkItemQt* itemQt = new WorkQueue::WorkItemQt(this, process, SIGNAL(finished(int, QProcess::ExitStatus)), workItem.leakPtr());
    itemQt->moveToThread(m_workThread);
}

#include "WorkQueueQt.moc"
