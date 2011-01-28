/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MOTOROLA INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA INC. OR ITS CONTRIBUTORS
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
#include <WebCore/FileSystem.h>
#include <WebCore/ResourceHandle.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>
#include <wtf/gobject/GOwnPtr.h>

using namespace WebCore;

namespace WebKit {

const char* gWebKitWebProcessName = "WebKitWebProcess";

void ProcessLauncher::launchProcess()
{
    pid_t pid = 0;

    int sockets[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        fprintf(stderr, "Creation of socket failed with errno %d.\n", errno);
        ASSERT_NOT_REACHED();
        return;
    }

    pid = fork();
    if (!pid) { // child process
        close(sockets[1]);
        String socket = String::format("%d", sockets[0]);
        GOwnPtr<gchar> binaryPath(g_build_filename(applicationDirectoryPath().data(), gWebKitWebProcessName, NULL));
        execl(binaryPath.get(), gWebKitWebProcessName, socket.utf8().data(), NULL);
    } else if (pid > 0) { // parent process
        close(sockets[0]);
        m_processIdentifier = pid;
        // We've finished launching the process, message back to the main run loop.
        RunLoop::main()->scheduleWork(WorkItem::create(this, &ProcessLauncher::didFinishLaunchingProcess, pid, sockets[1]));
    } else {
        fprintf(stderr, "Unable to fork a new WebProcess with errno: %d.\n", errno);
        ASSERT_NOT_REACHED();
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
