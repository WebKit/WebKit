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

#include "ProcessLauncher.h"

#include "Connection.h"
#include "CleanupHandler.h"
#include "NotImplemented.h"
#include "RunLoop.h"
#include "WebProcess.h"
#include <runtime/InitializeThreading.h>
#include <string>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/WTFString.h>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QLocalServer>
#include <QMetaType>
#include <QProcess>
#include <QString>

#include <QtCore/qglobal.h>

#include <sys/resource.h>
#include <unistd.h>

using namespace WebCore;

namespace WebKit {

class ProcessLauncherHelper : public QObject {
    Q_OBJECT
public:
    ~ProcessLauncherHelper();
    void launch(WebKit::ProcessLauncher*);
    QLocalSocket* takePendingConnection();
    static ProcessLauncherHelper* instance();

    const QString serverName() const { return m_server.serverName(); }

private:
    ProcessLauncherHelper();
    QLocalServer m_server;
    QList<WorkItem*> m_items;

    Q_SLOT void newConnection();
};

Q_GLOBAL_STATIC(WTF::HashSet<QProcess*>, processes);

static void cleanupAtExit()
{
    // Terminate our web process(es).
    WTF::HashSet<QProcess*>::const_iterator end = processes()->end();
    for (WTF::HashSet<QProcess*>::const_iterator it = processes()->begin(); it != end; ++it) {
        QProcess* process = *it;
        process->disconnect(process);
        process->terminate();
        if (!process->waitForFinished(200))
            process->kill();
    }

    // Do not leave the socket file behind.
    QLocalServer::removeServer(ProcessLauncherHelper::instance()->serverName());
}

class QtWebProcess : public QProcess
{
    Q_OBJECT
public:
    QtWebProcess(QObject* parent = 0)
        : QProcess(parent)
    {
        static bool isRegistered = false;
        if (!isRegistered) {
            qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");
            isRegistered = true;
        }

        connect(this, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(processStateChanged(QProcess::ProcessState)));
    }

private slots:
    void processStateChanged(QProcess::ProcessState state);
};

void QtWebProcess::processStateChanged(QProcess::ProcessState state)
{
    QProcess* process = qobject_cast<QProcess*>(sender());
    if (!process)
        return;

    if (state == QProcess::Running)
        processes()->add(process);
    else if (state == QProcess::NotRunning)
        processes()->remove(process);
}

void ProcessLauncherHelper::launch(WebKit::ProcessLauncher* launcher)
{
    QString applicationPath = "%1 %2";

    if (QFile::exists(QCoreApplication::applicationDirPath() + "/QtWebProcess")) {
        applicationPath = applicationPath.arg(QCoreApplication::applicationDirPath() + "/QtWebProcess");
    } else {
        applicationPath = applicationPath.arg("QtWebProcess");
    }

    QString program(applicationPath.arg(m_server.serverName()));

    QProcess* webProcess = new QtWebProcess();
    webProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    webProcess->start(program);

    if (!webProcess->waitForStarted()) {
        qDebug() << "Failed to start" << program;
        ASSERT_NOT_REACHED();
        delete webProcess;
        return;
    }

    setpriority(PRIO_PROCESS, webProcess->pid(), 10);

    m_items.append(WorkItem::create(launcher, &WebKit::ProcessLauncher::didFinishLaunchingProcess, webProcess, m_server.serverName()).leakPtr());
}

QLocalSocket* ProcessLauncherHelper::takePendingConnection()
{
    return m_server.nextPendingConnection();
}

ProcessLauncherHelper::~ProcessLauncherHelper()
{
    m_server.close();
}

ProcessLauncherHelper::ProcessLauncherHelper()
{
    srandom(time(0));
    if (!m_server.listen("QtWebKit" + QString::number(random()))) {
        qDebug() << "Failed to create server socket.";
        ASSERT_NOT_REACHED();
    }
    connect(&m_server, SIGNAL(newConnection()), this, SLOT(newConnection()));
}

ProcessLauncherHelper* ProcessLauncherHelper::instance()
{
    static ProcessLauncherHelper* result = 0;
    if (!result) {
        result = new ProcessLauncherHelper();

        // The purpose of the following line is to ensure that our static is initialized before the exit handler is installed.
        processes()->clear();

        atexit(cleanupAtExit);
    }
    return result;
}

void ProcessLauncherHelper::newConnection()
{
    ASSERT(!m_items.isEmpty());

    m_items[0]->execute();
    delete m_items[0];
    m_items.pop_front();
}

void ProcessLauncher::launchProcess()
{
    ProcessLauncherHelper::instance()->launch(this);
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processIdentifier)
        return;

    QObject::connect(m_processIdentifier, SIGNAL(finished(int)), m_processIdentifier, SLOT(deleteLater()), Qt::QueuedConnection);
    m_processIdentifier->terminate();
}

QLocalSocket* ProcessLauncher::takePendingConnection()
{
    return ProcessLauncherHelper::instance()->takePendingConnection();
}

void ProcessLauncher::platformInvalidate()
{
    notImplemented();
}

} // namespace WebKit

#include "ProcessLauncherQt.moc"
