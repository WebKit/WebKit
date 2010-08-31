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
#include "RunLoop.h"
#include "WebProcess.h"
#include <runtime/InitializeThreading.h>
#include <string>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/WTFString.h>

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QLocalServer>
#include <QProcess>

#include <QtCore/qglobal.h>

#include <sys/resource.h>
#include <unistd.h>

using namespace WebCore;

namespace WebKit {

class ProcessLauncherHelper : public QObject {
    Q_OBJECT
public:
    void launch(WebKit::ProcessLauncher*);
    QLocalSocket* takePendingConnection();
    static ProcessLauncherHelper* instance();
private:
    ProcessLauncherHelper();
    QLocalServer m_server;
    QList<WorkItem*> m_items;

    Q_SLOT void newConnection();
};

void ProcessLauncherHelper::launch(WebKit::ProcessLauncher* launcher)
{
    QString applicationPath = "%1 %2";

    if (QFile::exists(QCoreApplication::applicationDirPath() + "/QtWebProcess")) {
        applicationPath = applicationPath.arg(QCoreApplication::applicationDirPath() + "/QtWebProcess");
    } else {
        applicationPath = applicationPath.arg("QtWebProcess");
    }

    QString program(applicationPath.arg(m_server.serverName()));

    QProcess* webProcess = new QProcess();
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
    static ProcessLauncherHelper* result = new ProcessLauncherHelper();
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
    m_processIdentifier->kill();
}

QLocalSocket* ProcessLauncher::takePendingConnection()
{
    return ProcessLauncherHelper::instance()->takePendingConnection();
}

static void* webThreadBody(void* /* context */)
{
    // Initialization
    JSC::initializeThreading();
    WTF::initializeMainThread();

    // FIXME: We do not support threaded mode for now.

    WebProcess::shared().initialize("foo", RunLoop::current());
    RunLoop::run();

    return 0;
}

CoreIPC::Connection::Identifier ProcessLauncher::createWebThread()
{
    srandom(time(0));
    int connectionIdentifier = random();

    if (!createThread(webThreadBody, reinterpret_cast<void*>(connectionIdentifier), "WebKit2: WebThread")) {
        qWarning() << "failed starting thread";
        return 0;
    }

    QString serverIdentifier = QString::number(connectionIdentifier);
    return serverIdentifier;
}

} // namespace WebKit

#include "ProcessLauncherQt.moc"
