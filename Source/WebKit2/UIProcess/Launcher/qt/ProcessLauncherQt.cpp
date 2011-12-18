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
#include "ProcessLauncher.h"

#include "Connection.h"
#include "RunLoop.h"
#include "WebProcess.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QLocalServer>
#include <QMetaType>
#include <QProcess>
#include <QString>
#include <QtCore/qglobal.h>
#include <WebCore/NotImplemented.h>
#include <errno.h>
#include <fcntl.h>
#include <runtime/InitializeThreading.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/WTFString.h>
#if defined(Q_OS_LINUX)
#include <sys/prctl.h>
#include <signal.h>
#endif

#if OS(DARWIN)
#include <mach/mach_init.h>
#include <servers/bootstrap.h>

extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);
#endif

#if defined(SOCK_SEQPACKET) && !defined(Q_OS_MACX)
#define SOCKET_TYPE SOCK_SEQPACKET
#else
#define SOCKET_TYPE SOCK_DGRAM
#endif

using namespace WebCore;

namespace WebKit {

class QtWebProcess : public QProcess
{
    Q_OBJECT
public:
    QtWebProcess(QObject* parent = 0)
        : QProcess(parent)
    {
    }

protected:
    virtual void setupChildProcess();
};

void QtWebProcess::setupChildProcess()
{
#if defined(Q_OS_LINUX)
#ifndef NDEBUG
    if (getenv("QT_WEBKIT_KEEP_ALIVE_WEB_PROCESS"))
        return;
#endif
    prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
#if defined(Q_OS_MACX)
    qputenv("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", QByteArray("1"));
#endif
}

void ProcessLauncher::launchProcess()
{
    QString applicationPath = QLatin1String("%1 %2");

    if (QFile::exists(QCoreApplication::applicationDirPath() + QLatin1String("/QtWebProcess"))) {
        applicationPath = applicationPath.arg(QCoreApplication::applicationDirPath() + QLatin1String("/QtWebProcess"));
    } else {
        applicationPath = applicationPath.arg(QLatin1String("QtWebProcess"));
    }

#if OS(DARWIN)
    // Create the listening port.
    mach_port_t connector;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &connector);

    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), connector, connector, MACH_MSG_TYPE_MAKE_SEND);

    // Register port with a service name to the system.
    QString serviceName = QString("com.nokia.Qt.WebKit.QtWebProcess-%1-%2");
    serviceName = serviceName.arg(QString().setNum(getpid()), QString().setNum((size_t)this));
    kern_return_t kr = bootstrap_register2(bootstrap_port, const_cast<char*>(serviceName.toUtf8().data()), connector, 0);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);

    QString program(applicationPath.arg(serviceName));
#else
    int sockets[2];
    if (socketpair(AF_UNIX, SOCKET_TYPE, 0, sockets) == -1) {
        qDebug() << "Creation of socket failed with errno:" << errno;
        ASSERT_NOT_REACHED();
        return;
    }

    // Don't expose the ui socket to the web process
    while (fcntl(sockets[1], F_SETFD, FD_CLOEXEC)  == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            while (close(sockets[0]) == -1 && errno == EINTR) { }
            while (close(sockets[1]) == -1 && errno == EINTR) { }
            return;
        }
    }

    int connector = sockets[1];
    QString program(applicationPath.arg(sockets[0]));
#endif

    QProcess* webProcess = new QtWebProcess();
    webProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    webProcess->start(program);

#if !OS(DARWIN)
    // Don't expose the web socket to possible future web processes
    while (fcntl(sockets[0], F_SETFD, FD_CLOEXEC) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            delete webProcess;
            return;
        }
    }
#endif

    if (!webProcess->waitForStarted()) {
        qDebug() << "Failed to start" << program;
        ASSERT_NOT_REACHED();
#if OS(DARWIN)
        mach_port_deallocate(mach_task_self(), connector);
        mach_port_mod_refs(mach_task_self(), connector, MACH_PORT_RIGHT_RECEIVE, -1);
#endif
        delete webProcess;
        return;
    }

    setpriority(PRIO_PROCESS, webProcess->pid(), 10);

    RunLoop::main()->dispatch(bind(&WebKit::ProcessLauncher::didFinishLaunchingProcess, this, webProcess, connector));
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processIdentifier)
        return;

    QObject::connect(m_processIdentifier, SIGNAL(finished(int)), m_processIdentifier, SLOT(deleteLater()), Qt::QueuedConnection);
    m_processIdentifier->terminate();
}

void ProcessLauncher::platformInvalidate()
{

}

} // namespace WebKit

#include "ProcessLauncherQt.moc"
