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
#include <WebCore/FileSystem.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <wtf/RunLoop.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if PLATFORM(WPE)
#include <wpe/wpe.h>
#endif

namespace WebKit {
using namespace WebCore;

static void childSetupFunction(gpointer userData)
{
    int socket = GPOINTER_TO_INT(userData);
    close(socket);
}

void ProcessLauncher::launchProcess()
{
    GPid pid = 0;

    IPC::Connection::SocketPair socketPair = IPC::Connection::createPlatformConnection(IPC::Connection::ConnectionOptions::SetCloexecOnServer);

    String executablePath;
    CString realExecutablePath;
#if ENABLE(NETSCAPE_PLUGIN_API)
    String pluginPath;
    CString realPluginPath;
#endif
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
        realPluginPath = FileSystem::fileSystemRepresentation(pluginPath);
        break;
#endif
    case ProcessLauncher::ProcessType::Network:
        executablePath = executablePathOfNetworkProcess();
        break;
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    realExecutablePath = FileSystem::fileSystemRepresentation(executablePath);
    GUniquePtr<gchar> processIdentifier(g_strdup_printf("%" PRIu64, m_launchOptions.processIdentifier.toUInt64()));
    GUniquePtr<gchar> webkitSocket(g_strdup_printf("%d", socketPair.client));
    unsigned nargs = 5; // size of the argv array for g_spawn_async()

#if PLATFORM(WPE)
    GUniquePtr<gchar> wpeSocket;
    CString wpeBackendLibraryParameter;
    if (m_launchOptions.processType == ProcessLauncher::ProcessType::Web) {
#if defined(WPE_BACKEND_CHECK_VERSION) && WPE_BACKEND_CHECK_VERSION(0, 2, 0)
        wpeBackendLibraryParameter = FileSystem::fileSystemRepresentation(wpe_loader_get_loaded_implementation_library_name());
#endif
        nargs++;

        wpeSocket = GUniquePtr<gchar>(g_strdup_printf("%d", wpe_renderer_host_create_client()));
        nargs++;
    }
#endif

#if ENABLE(DEVELOPER_MODE)
    Vector<CString> prefixArgs;
    if (!m_launchOptions.processCmdPrefix.isNull()) {
        for (auto& arg : m_launchOptions.processCmdPrefix.split(' '))
            prefixArgs.append(arg.utf8());
        nargs += prefixArgs.size();
    }
#endif

    char** argv = g_newa(char*, nargs);
    unsigned i = 0;
#if ENABLE(DEVELOPER_MODE)
    // If there's a prefix command, put it before the rest of the args.
    for (auto& arg : prefixArgs)
        argv[i++] = const_cast<char*>(arg.data());
#endif
    argv[i++] = const_cast<char*>(realExecutablePath.data());
    argv[i++] = processIdentifier.get();
    argv[i++] = webkitSocket.get();
#if PLATFORM(WPE)
    if (m_launchOptions.processType == ProcessLauncher::ProcessType::Web) {
        argv[i++] = const_cast<char*>(wpeBackendLibraryParameter.isNull() ? "-" : wpeBackendLibraryParameter.data());
        argv[i++] = wpeSocket.get();
    }
#endif
#if ENABLE(NETSCAPE_PLUGIN_API)
    argv[i++] = const_cast<char*>(realPluginPath.data());
#else
    argv[i++] = nullptr;
#endif
    argv[i++] = nullptr;

    GUniqueOutPtr<GError> error;
    if (!g_spawn_async(nullptr, argv, nullptr, G_SPAWN_LEAVE_DESCRIPTORS_OPEN, childSetupFunction, GINT_TO_POINTER(socketPair.server), &pid, &error.outPtr()))
        g_error("Unable to fork a new child process: %s", error->message);

    // Don't expose the parent socket to potential future children.
    if (!setCloseOnExec(socketPair.client))
        RELEASE_ASSERT_NOT_REACHED();

    close(socketPair.client);
    m_processIdentifier = pid;

    // We've finished launching the process, message back to the main run loop.
    RunLoop::main().dispatch([protectedThis = makeRef(*this), this, serverSocket = socketPair.server] {
        didFinishLaunchingProcess(m_processIdentifier, serverSocket);
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
