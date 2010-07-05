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
#include <WebCore/PlatformString.h>
#include <runtime/InitializeThreading.h>
#include <string>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

#if USE(MEEGOTOUCH)
#include <meegotouch/MComponentData>
#endif

#ifdef Q_OS_UNIX
#include <signal.h>
#endif

#include <QApplication>
#include <QDebug>
#include <QProcess>

#include <QtCore/qglobal.h>

#include <sys/resource.h>
#include <unistd.h>

#if !defined(QWEBKIT_EXPORT)
#  if defined(QT_SHARED)
#    define QWEBKIT_EXPORT Q_DECL_EXPORT
#  else
#    define QWEBKIT_EXPORT
#  endif
#endif

using namespace WebCore;

namespace WebKit {

void ProcessLauncher::launchProcess()
{
    srandom(time(0));
    QString connectionIdentifier = QString::number(random());

    QString program("QtWebProcess " + connectionIdentifier);

    QProcess* webProcess = new QProcess;
    webProcess->start(program);

    if (!webProcess->waitForStarted()) {
        qDebug() << "Failed to start" << program;
        ASSERT_NOT_REACHED();
    }

    PlatformProcessIdentifier processIdentifier = webProcess->pid();
    setpriority(PRIO_PROCESS, processIdentifier, 10);

    qDebug() << program << "nice" << getpriority(PRIO_PROCESS, processIdentifier);

    // We've finished launching the process, message back to the run loop.
    RunLoop::main()->scheduleWork(WorkItem::create(this, &ProcessLauncher::didFinishLaunchingProcess, processIdentifier, connectionIdentifier));
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processIdentifier)
        return;

#ifdef Q_OS_UNIX
    kill(m_processIdentifier, SIGKILL);
#endif
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

QWEBKIT_EXPORT int webProcessMain(int argc, char** argv)
{
    QApplication* app = new QApplication(argc, argv);

#if USE(MEEGOTOUCH)
    new MComponentData(argc, argv);
#endif

    srandom(time(0));

    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    // Create the connection.
    QString identifier(app->arguments().size() > 1 ? app->arguments().at(1) : "");
    WebKit::WebProcess::shared().initialize(identifier, RunLoop::main());

    RunLoop::run();

    // FIXME: Do more cleanup here.

    return 0;
}
