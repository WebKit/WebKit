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
#include <WebCore/FileSystem.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/RunLoop.h>
#include <libgen.h>
#include <unistd.h>
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

    pid_t pid = fork();
    if (!pid) { // child process
        close(sockets[1]);
        String socket = String::format("%d", sockets[0]);
        String processName;
        switch (m_launchOptions.processType) {
        case WebProcess:
            processName = "WebProcess";
            break;
        case PluginProcess:
            processName = "PluginProcess";
            break;
        default:
            ASSERT_NOT_REACHED();
            return;
        }

        String executablePath;
        char readLinkBuffer[PATH_MAX];
        memset(readLinkBuffer, 0, PATH_MAX);
        ssize_t result = readlink("/proc/self/exe", readLinkBuffer, PATH_MAX);

        if (result == -1)
            executablePath = String("/usr/bin");
        else {
            char* executablePathPtr = dirname(readLinkBuffer);
            executablePath = String(executablePathPtr);
        }
        String fullPath = executablePath + "/" + processName;
        execl(fullPath.utf8().data(), processName.utf8().data(), socket.utf8().data(), static_cast<char*>(0));
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
