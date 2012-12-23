/*
    Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "ProcessLauncher.h"

#include "Connection.h"
#include "ProcessExecutablePath.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/FileSystem.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/RunLoop.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

void ProcessLauncher::launchProcess()
{
    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        ASSERT_NOT_REACHED();
        return;
    }

    const char* executablePath = 0;
    switch (m_launchOptions.processType) {
    case WebProcess:
        executablePath = executablePathOfWebProcess().utf8().data();
        break;
#if ENABLE(PLUGIN_PROCESS)
    case PluginProcess:
        executablePath = executablePathOfPluginProcess().utf8().data();
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    char socket[5];
    snprintf(socket, sizeof(socket), "%d", sockets[0]);

#ifndef NDEBUG
    const char* prefixedExecutablePath = 0;
    String prefixedExecutablePathStr;
    if (!m_launchOptions.processCmdPrefix.isEmpty()) {
        prefixedExecutablePathStr = m_launchOptions.processCmdPrefix + ' ' + executablePath + ' ' + socket;
        prefixedExecutablePath = prefixedExecutablePathStr.utf8().data();
    }
#endif

    // Do not perform memory allocation in the middle of the fork()
    // exec() below. FastMalloc can potentially deadlock because
    // the fork() doesn't inherit the running threads.
    pid_t pid = fork();
    if (!pid) { // Child process.
        close(sockets[1]);
#ifndef NDEBUG
        if (prefixedExecutablePath) {
            // FIXME: This is not correct because it invokes the shell
            // and keeps this process waiting. Should be changed to
            // something like execvp().
            if (system(prefixedExecutablePath) == -1) {
                ASSERT_NOT_REACHED();
                exit(EXIT_FAILURE);
            } else
                exit(EXIT_SUCCESS);
        }
#endif
        execl(executablePath, executablePath, socket, static_cast<char*>(0));
    } else if (pid > 0) { // parent process;
        close(sockets[0]);
        m_processIdentifier = pid;
        // We've finished launching the process, message back to the main run loop.
        RunLoop::main()->dispatch(bind(&ProcessLauncher::didFinishLaunchingProcess, this, pid, sockets[1]));
    } else {
        ASSERT_NOT_REACHED();
        return;
    }
}

void ProcessLauncher::terminateProcess()
{
    if (!m_processIdentifier)
        return;
    kill(m_processIdentifier, SIGKILL);
}

void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
