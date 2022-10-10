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
#include "IPCUtilities.h"
#include "ProcessExecutablePath.h"
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/Sandbox.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#if USE(LIBWPE)
#include "ProcessProviderLibWPE.h"
#endif

#if !USE(SYSTEM_MALLOC) && OS(LINUX)
#include <bmalloc/valgrind.h>
#endif

namespace WebKit {

#if OS(LINUX)
static bool isFlatpakSpawnUsable()
{
    static std::optional<bool> ret;
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

static int connectionOptions()
{
#if USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)
    // When using the WPE process launcher API, we cannot use CLOEXEC for the client socket because
    // we need to leak it to the child process.
    if (ProcessProviderLibWPE::singleton().isEnabled())
        return IPC::PlatformConnectionOptions::SetCloexecOnServer;
#endif

    // We use CLOEXEC for the client socket here even though we need to leak it to the child,
    // because we don't want it leaking to xdg-dbus-proxy. If the IPC socket is unexpectedly open in
    // an extra subprocess, WebKit won't notice when its child process crashes. We can ensure it
    // gets leaked into only the correct subprocess by using g_subprocess_launcher_take_fd() later.
    return IPC::PlatformConnectionOptions::SetCloexecOnClient | IPC::PlatformConnectionOptions::SetCloexecOnServer;
}

void ProcessLauncher::launchProcess()
{
    IPC::SocketPair socketPair = IPC::createPlatformConnection(connectionOptions());

    GUniquePtr<gchar> processIdentifier(g_strdup_printf("%" PRIu64, m_launchOptions.processIdentifier.toUInt64()));
    GUniquePtr<gchar> webkitSocket(g_strdup_printf("%d", socketPair.client));

#if USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)
    if (ProcessProviderLibWPE::singleton().isEnabled()) {
        unsigned nargs = 3;
        char** argv = g_newa(char*, nargs);
        unsigned i = 0;
        argv[i++] = processIdentifier.get();
        argv[i++] = webkitSocket.get();
        argv[i++] = nullptr;

        m_processIdentifier = ProcessProviderLibWPE::singleton().launchProcess(m_launchOptions, argv, socketPair.client);
        if (m_processIdentifier <= -1)
            g_error("Unable to spawn a new child process");

        // We've finished launching the process, message back to the main run loop.
        RunLoop::main().dispatch([protectedThis = Ref { *this }, this, serverSocket = socketPair.server] {
            didFinishLaunchingProcess(m_processIdentifier, IPC::Connection::Identifier { serverSocket });
        });

        return;
    }
#endif

    String executablePath;
    CString realExecutablePath;
    switch (m_launchOptions.processType) {
    case ProcessLauncher::ProcessType::Web:
        executablePath = executablePathOfWebProcess();
        break;
    case ProcessLauncher::ProcessType::Network:
        executablePath = executablePathOfNetworkProcess();
        break;
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        executablePath = executablePathOfGPUProcess();
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        return;
    }

    realExecutablePath = FileSystem::fileSystemRepresentation(executablePath);
    unsigned nargs = 4; // size of the argv array for g_spawn_async()

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
    argv[i++] = nullptr;

    // Warning: we want GIO to be able to spawn with posix_spawn() rather than fork()/exec(), in
    // order to better accommodate applications that use a huge amount of memory or address space
    // in the UI process, like Eclipse. This means we must use GSubprocess in a manner that follows
    // the rules documented in g_spawn_async_with_pipes_and_fds() for choosing between posix_spawn()
    // (optimized/ideal codepath) vs. fork()/exec() (fallback codepath). As of GLib 2.74, the rules
    // relevant to GSubprocess are (a) must inherit fds, (b) must not search path from envp, and (c)
    // must not use a child setup fuction.
    //
    // Please keep this comment in sync with the duplicate comment in XDGDBusProxy::launch.
    GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_INHERIT_FDS));
    g_subprocess_launcher_take_fd(launcher.get(), socketPair.client, socketPair.client);

    GUniqueOutPtr<GError> error;
    GRefPtr<GSubprocess> process;

#if OS(LINUX)
    const char* sandboxEnv = g_getenv("WEBKIT_FORCE_SANDBOX");
    bool sandboxEnabled = m_launchOptions.extraInitializationData.get<HashTranslatorASCIILiteral>("enable-sandbox"_s) == "true"_s;

    if (sandboxEnv)
        sandboxEnabled = !strcmp(sandboxEnv, "1");

#if !USE(SYSTEM_MALLOC)
    if (RUNNING_ON_VALGRIND)
        sandboxEnabled = false;
#endif

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
        g_error("Unable to spawn a new child process: %s", error->message);

    const char* processIdStr = g_subprocess_get_identifier(process.get());
    if (!processIdStr)
        g_error("Spawned process died immediately. This should not happen.");

    m_processIdentifier = g_ascii_strtoll(processIdStr, nullptr, 0);
    RELEASE_ASSERT(m_processIdentifier);

    // We've finished launching the process, message back to the main run loop.
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this, serverSocket = socketPair.server] {
        didFinishLaunchingProcess(m_processIdentifier, IPC::Connection::Identifier { serverSocket });
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

#if USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)
    if (ProcessProviderLibWPE::singleton().isEnabled())
        ProcessProviderLibWPE::singleton().kill(m_processIdentifier);
    else
        kill(m_processIdentifier, SIGKILL);
#else
    kill(m_processIdentifier, SIGKILL);
#endif

    m_processIdentifier = 0;
}

void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
