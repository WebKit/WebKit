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

#include "BubblewrapLauncher.h"
#include "Connection.h"
#include "FlatpakLauncher.h"
#include "ProcessExecutablePath.h"
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

static void childSetupFunction(gpointer userData)
{
    int socket = GPOINTER_TO_INT(userData);
    close(socket);
}

#if OS(LINUX)
static bool isFlatpakSpawnUsable()
{
    static Optional<bool> ret;
    if (ret)
        return *ret;

    // For our usage to work we need flatpak >= 1.5.2 on the host and flatpak-xdg-utils > 1.0.1 in the sandbox
    GRefPtr<GSubprocess> process = adoptGRef(g_subprocess_new(static_cast<GSubprocessFlags>(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE),
        nullptr, "flatpak-spawn", "--sandbox", "--sandbox-expose-path-ro-try=/this_path_doesnt_exist", "echo", nullptr));

    if (!process.get())
        ret = false;
    else
        ret = g_subprocess_wait_check(process.get(), nullptr, nullptr);

    return *ret;
}
#endif

#if ENABLE(BUBBLEWRAP_SANDBOX)
static bool isInsideDocker()
{
    static Optional<bool> ret;
    if (ret)
        return *ret;

    ret = g_file_test("/.dockerenv", G_FILE_TEST_EXISTS);
    return *ret;
}

static bool isInsideFlatpak()
{
    static Optional<bool> ret;
    if (ret)
        return *ret;

    ret = g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);
    return *ret;
}

static bool isInsideSnap()
{
    static Optional<bool> ret;
    if (ret)
        return *ret;

    // The "SNAP" environment variable is not unlikely to be set for/by something other
    // than Snap, so check a couple of additional variables to avoid false positives.
    // See: https://snapcraft.io/docs/environment-variables
    ret = g_getenv("SNAP") && g_getenv("SNAP_NAME") && g_getenv("SNAP_REVISION");
    return *ret;
}
#endif

void ProcessLauncher::launchProcess()
{
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

#if ENABLE(DEVELOPER_MODE)
    Vector<CString> prefixArgs;
    if (!m_launchOptions.processCmdPrefix.isNull()) {
        for (auto& arg : m_launchOptions.processCmdPrefix.split(' '))
            prefixArgs.append(arg.utf8());
        nargs += prefixArgs.size();
    }

    bool configureJSCForTesting = false;
    if (m_launchOptions.processType == ProcessLauncher::ProcessType::Web && m_client && m_client->shouldConfigureJSCForTesting()) {
        configureJSCForTesting = true;
        nargs++;
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
#if ENABLE(DEVELOPER_MODE)
    if (configureJSCForTesting)
        argv[i++] = const_cast<char*>("--configure-jsc-for-testing");
#endif
#if ENABLE(NETSCAPE_PLUGIN_API)
    argv[i++] = const_cast<char*>(realPluginPath.data());
#else
    argv[i++] = nullptr;
#endif
    argv[i++] = nullptr;

    GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_INHERIT_FDS));
    g_subprocess_launcher_set_child_setup(launcher.get(), childSetupFunction, GINT_TO_POINTER(socketPair.server), nullptr);
    g_subprocess_launcher_take_fd(launcher.get(), socketPair.client, socketPair.client);

    GUniqueOutPtr<GError> error;
    GRefPtr<GSubprocess> process;

#if OS(LINUX)
    const char* sandboxEnv = g_getenv("WEBKIT_FORCE_SANDBOX");
    bool sandboxEnabled = m_launchOptions.extraInitializationData.get("enable-sandbox") == "true";

    if (sandboxEnv)
        sandboxEnabled = !strcmp(sandboxEnv, "1");

    if (sandboxEnabled && isFlatpakSpawnUsable())
        process = flatpakSpawn(launcher.get(), m_launchOptions, argv, socketPair.client, &error.outPtr());
#if ENABLE(BUBBLEWRAP_SANDBOX)
    // You cannot use bubblewrap within Flatpak or Docker so lets ensure it never happens.
    // Snap can allow it but has its own limitations that require workarounds.
    else if (sandboxEnabled && !isInsideFlatpak() && !isInsideSnap() && !isInsideDocker())
        process = bubblewrapSpawn(launcher.get(), m_launchOptions, argv, &error.outPtr());
#endif // ENABLE(BUBBLEWRAP_SANDBOX)
    else
#endif // OS(LINUX)
        process = adoptGRef(g_subprocess_launcher_spawnv(launcher.get(), argv, &error.outPtr()));

    if (!process.get())
        g_error("Unable to fork a new child process: %s", error->message);

    const char* processIdStr = g_subprocess_get_identifier(process.get());
    m_processIdentifier = g_ascii_strtoll(processIdStr, nullptr, 0);
    RELEASE_ASSERT(m_processIdentifier);

    // Don't expose the parent socket to potential future children.
    if (!setCloseOnExec(socketPair.client))
        RELEASE_ASSERT_NOT_REACHED();

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
