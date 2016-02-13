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
#include "ProcessExecutablePath.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/FileSystem.h>
#include <WebCore/NetworkingContext.h>
#include <WebCore/ResourceHandle.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <locale.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static void childSetupFunction(gpointer userData)
{
    int socket = GPOINTER_TO_INT(userData);
    close(socket);
}

void ProcessLauncher::launchProcess()
{
    GPid pid = 0;

    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection(IPC::Connection::ConnectionOptions::SetCloexecOnServer);

    String executablePath, pluginPath;
    CString realExecutablePath, realPluginPath;
    switch (m_launchOptions.processType) {
    case ProcessLauncher::ProcessType::Web:
        executablePath = executablePathOfWebProcess();
        break;
#if ENABLE(NETSCAPE_PLUGIN_API)
    case ProcessLauncher::ProcessType::Plugin64:
    case ProcessLauncher::ProcessType::Plugin32:
        executablePath = executablePathOfPluginProcess();
#if ENABLE(PLUGIN_PROCESS_GTK2)
        if (m_launchOptions.extraInitializationData.contains("requires-gtk2"))
            executablePath.append('2');
#endif
        pluginPath = m_launchOptions.extraInitializationData.get("plugin-path");
        realPluginPath = fileSystemRepresentation(pluginPath);
        break;
#endif
    case ProcessLauncher::ProcessType::Network:
        executablePath = executablePathOfNetworkProcess();
        break;
#if ENABLE(DATABASE_PROCESS)
    case ProcessLauncher::ProcessType::Database:
        executablePath = executablePathOfDatabaseProcess();
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    realExecutablePath = fileSystemRepresentation(executablePath);
    GUniquePtr<gchar> socket(g_strdup_printf("%d", socketPair.client));

    unsigned nargs = 4; // size of the argv array for g_spawn_async()

#ifndef NDEBUG
    Vector<CString> prefixArgs;
    if (!m_launchOptions.processCmdPrefix.isNull()) {
        Vector<String> splitArgs;
        m_launchOptions.processCmdPrefix.split(' ', splitArgs);
        for (auto it = splitArgs.begin(); it != splitArgs.end(); it++)
            prefixArgs.append(it->utf8());
        nargs += prefixArgs.size();
    }
#endif

    char** argv = g_newa(char*, nargs);
    unsigned i = 0;
#ifndef NDEBUG
    // If there's a prefix command, put it before the rest of the args.
    for (auto it = prefixArgs.begin(); it != prefixArgs.end(); it++)
        argv[i++] = const_cast<char*>(it->data());
#endif
    argv[i++] = const_cast<char*>(realExecutablePath.data());
    argv[i++] = socket.get();
    argv[i++] = const_cast<char*>(realPluginPath.data());
    argv[i++] = 0;

    GUniqueOutPtr<GError> error;
    if (!g_spawn_async(0, argv, 0, G_SPAWN_LEAVE_DESCRIPTORS_OPEN, childSetupFunction, GINT_TO_POINTER(socketPair.server), &pid, &error.outPtr())) {
        g_printerr("Unable to fork a new WebProcess: %s.\n", error->message);
        ASSERT_NOT_REACHED();
    }

    // Don't expose the parent socket to potential future children.
    while (fcntl(socketPair.client, F_SETFD, FD_CLOEXEC) == -1)
        RELEASE_ASSERT(errno != EINTR);

    close(socketPair.client);
    m_processIdentifier = pid;

    // We've finished launching the process, message back to the main run loop.
    RefPtr<ProcessLauncher> protector(this);
    IPC::Connection::Identifier serverSocket = socketPair.server;
    RunLoop::main().dispatch([protector, pid, serverSocket] {
        protector->didFinishLaunchingProcess(pid, serverSocket);
    });
}

void ProcessLauncher::terminateProcess()
{
    if (m_isLaunching) {
        invalidate();
        return;
    }

    if (!m_processIdentifier)
        return;

    kill(m_processIdentifier, SIGKILL);
    m_processIdentifier = 0;
}

void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
