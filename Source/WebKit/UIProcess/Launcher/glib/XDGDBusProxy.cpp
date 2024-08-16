/*
 * Copyright (C) 2018, 2022 Igalia S.L.
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
#include "XDGDBusProxy.h"

#if ENABLE(BUBBLEWRAP_SANDBOX)
#include "BubblewrapLauncher.h"
#include <gio/gunixinputstream.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/Application.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/Sandbox.h>
#include <wtf/text/MakeString.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(XDGDBusProxy);

CString XDGDBusProxy::makeProxy(const char* baseDirectory, const char* proxyTemplate)
{
    GUniquePtr<char> appRunDir(g_build_filename(g_get_user_runtime_dir(), baseDirectory, nullptr));
    if (g_mkdir_with_parents(appRunDir.get(), 0700) == -1) {
        g_warning("Failed to mkdir for dbus proxy (%s): %s", appRunDir.get(), g_strerror(errno));
        return { };
    }

    GUniquePtr<char> proxySocketTemplate(g_build_filename(appRunDir.get(), proxyTemplate, nullptr));
    UnixFileDescriptor fd(g_mkstemp(proxySocketTemplate.get()), UnixFileDescriptor::Adopt);
    if (!fd) {
        g_warning("Failed to make socket file %s for dbus proxy: %s", proxySocketTemplate.get(), g_strerror(errno));
        return { };
    }

    return proxySocketTemplate.get();
}

std::optional<CString> XDGDBusProxy::dbusSessionProxy(const char* baseDirectory, AllowPortals allowPortals)
{
    if (!m_dbusSessionProxyPath.isNull())
        return m_dbusSessionProxyPath;

    const char* dbusAddress = g_getenv("DBUS_SESSION_BUS_ADDRESS");
    if (!dbusAddress)
        return std::nullopt;

    m_dbusSessionProxyPath = makeProxy(baseDirectory, "bus-proxy-XXXXXX");
    if (m_dbusSessionProxyPath.isNull())
        return std::nullopt;

    m_args.appendVector(Vector<CString> {
        CString(dbusAddress), m_dbusSessionProxyPath,
        "--filter"
    });

#if ENABLE(MEDIA_SESSION)
    auto mprisSessionID = makeString("--own=org.mpris.MediaPlayer2."_s, WTF::applicationID().span(), ".Sandboxed.*"_s);
    m_args.append(mprisSessionID.ascii().data());
#endif

    if (allowPortals == AllowPortals::Yes)
        m_args.append("--talk=org.freedesktop.portal.Desktop");

    if (!g_strcmp0(g_getenv("WEBKIT_ENABLE_DBUS_PROXY_LOGGING"), "1"))
        m_args.append("--log");

    return m_dbusSessionProxyPath;
}

#if USE(ATSPI)
std::optional<CString> XDGDBusProxy::accessibilityProxy(const char* baseDirectory, const String& accessibilityBusAddress)
{
    if (!m_accessibilityProxyPath.isNull())
        return m_accessibilityProxyPath;

    m_accessibilityProxyPath = makeProxy(baseDirectory, "a11y-proxy-XXXXXX");
    if (m_accessibilityProxyPath.isNull())
        return std::nullopt;

    m_args.appendVector(Vector<CString> {
        accessibilityBusAddress.utf8(), m_accessibilityProxyPath,
        "--filter",
        "--sloppy-names",
        "--broadcast=org.a11y.atspi.Registry.EventListenerRegistered=@/org/a11y/atspi/registry",
        "--broadcast=org.a11y.atspi.Registry.EventListenerDeregistered=@/org/a11y/atspi/registry",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.Socket.Embed@/org/a11y/atspi/accessible/root",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.Socket.Unembed@/org/a11y/atspi/accessible/root",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.Registry.GetRegisteredEvents@/org/a11y/atspi/registry",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.GetKeystrokeListeners@/org/a11y/atspi/registry/deviceeventcontroller",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.GetDeviceEventListeners@/org/a11y/atspi/registry/deviceeventcontroller",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.NotifyListenersSync@/org/a11y/atspi/registry/deviceeventcontroller",
        "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.NotifyListenersAsync@/org/a11y/atspi/registry/deviceeventcontroller",
    });

    if (!g_strcmp0(g_getenv("WEBKIT_ENABLE_A11Y_DBUS_PROXY_LOGGING"), "1"))
        m_args.append("--log");

    return m_accessibilityProxyPath;
}
#endif

static void waitUntilSyncedOrDie(GSubprocess* subprocess, int syncFd)
{
    GRefPtr<GMainContext> context = adoptGRef(g_main_context_new());
    g_main_context_push_thread_default(context.get());

    struct AsyncData {
        int opsFinished = 0;
        GUniqueOutPtr<GError> error;
    } data;
    GRefPtr<GCancellable> cancellable = adoptGRef(g_cancellable_new());

    g_subprocess_wait_check_async(subprocess, cancellable.get(), +[](GObject* source, GAsyncResult* result, gpointer data) {
        GUniqueOutPtr<GError> error;
        auto* asyncData = static_cast<AsyncData*>(data);
        g_subprocess_wait_check_finish(G_SUBPROCESS(source), result, &error.outPtr());
        if (error && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_propagate_error(&asyncData->error.outPtr(), error.release());
        asyncData->opsFinished++;
    }, &data);

    GRefPtr<GInputStream> inputStream = adoptGRef(g_unix_input_stream_new(syncFd, FALSE));
    char out;
    g_input_stream_read_async(inputStream.get(), &out, 1, G_PRIORITY_DEFAULT, cancellable.get(), +[](GObject* source, GAsyncResult* result, gpointer data) {
        GUniqueOutPtr<GError> error;
        auto* asyncData = static_cast<AsyncData*>(data);
        g_input_stream_read_finish(G_INPUT_STREAM(source), result, &error.outPtr());
        if (error && !g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
            g_propagate_error(&asyncData->error.outPtr(), error.release());
        asyncData->opsFinished++;
    }, &data);

    // As soon as one operation finishes, cancel the other. There are two cases:
    //
    // * Normal case: read op finishes, indicating xdg-dbus-proxy is ready. Now we can cancel the
    //   wait op, which was used to check if process launching failed. It didn't.
    // * Error case: wait op finishes. Cancel the read op so it doesn't hang forever, then crash.
    while (data.opsFinished != 2) {
        g_main_context_iteration(context.get(), TRUE);
        if (data.opsFinished == 1 && cancellable) {
            g_cancellable_cancel(cancellable.get());
            cancellable = nullptr;
        }
    }
    g_main_context_pop_thread_default(context.get());

    if (data.error)
        g_error("Failed to fully launch dbus-proxy: %s", data.error->message);
}

bool XDGDBusProxy::launch(const ProcessLauncher::LaunchOptions& webProcessLaunchOptions)
{
    if (m_syncFD)
        return true;

    if (m_args.isEmpty())
        return false;

    int syncFds[2];
    if (pipe(syncFds) == -1)
        g_error("Failed to make syncfds for dbus-proxy: %s", g_strerror(errno));
    setCloseOnExec(syncFds[0]);

    GUniquePtr<char> syncFdStr(g_strdup_printf("--fd=%d", syncFds[1]));
    Vector<CString> proxyArgs = { syncFdStr.get() };
    proxyArgs.appendVector(WTFMove(m_args));

    // We have to run xdg-dbus-proxy under bubblewrap because we need /.flatpak-info to exist in
    // xdg-dbus-proxy's mount namespace. Portals may use this as a trusted way to get the
    // sandboxed process's application ID, and will break if it's missing.
    int proxyFd = argumentsToFileDescriptor(proxyArgs, "dbus-proxy");
    GUniquePtr<char> proxyArgsStr(g_strdup_printf("--args=%d", proxyFd));
    Vector<CString> args = {
        DBUS_PROXY_EXECUTABLE,
        proxyArgsStr.get(),
    };
    int nargs = args.size() + 1;
    int i = 0;
    char** argv = g_newa(char*, nargs);
    for (const auto& arg : args)
        argv[i++] = const_cast<char*>(arg.data());
    argv[i] = nullptr;

    // Warning: we want GIO to be able to spawn with posix_spawn() rather than fork()/exec(), in
    // order to better accommodate applications that use a huge amount of memory or address space
    // in the UI process, like Eclipse. This means we must use GSubprocess in a manner that follows
    // the rules documented in g_spawn_async_with_pipes_and_fds() for choosing between posix_spawn()
    // (optimized/ideal codepath) vs. fork()/exec() (fallback codepath). As of GLib 2.74, the rules
    // relevant to GSubprocess are (a) must inherit fds, (b) must not search path from envp, and (c)
    // must not use a child setup fuction.
    //
    // Please keep this comment in sync with the duplicate comment in ProcessLauncher::launchProcess.
    GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_INHERIT_FDS));
    g_subprocess_launcher_take_fd(launcher.get(), proxyFd, proxyFd);
    g_subprocess_launcher_take_fd(launcher.get(), syncFds[1], syncFds[1]);
    m_syncFD = UnixFileDescriptor(syncFds[0], UnixFileDescriptor::Adopt);

    // We are purposefully leaving syncFds[0] open here.
    // xdg-dbus-proxy will exit() itself once that is closed on our exit.

    ProcessLauncher::LaunchOptions launchOptions;
    launchOptions.processType = ProcessLauncher::ProcessType::DBusProxy;

#if USE(ATSPI)
    launchOptions.extraInitializationData.set("accessibilityBusAddress"_s, webProcessLaunchOptions.extraInitializationData.get("accessibilityBusAddress"_s));
#else
    UNUSED_PARAM(webProcessLaunchOptions);
#endif

    GUniqueOutPtr<GError> error;
    GRefPtr<GSubprocess> subprocess = bubblewrapSpawn(launcher.get(), launchOptions, argv, &error.outPtr());
    if (!subprocess)
        g_error("Failed to start dbus proxy: %s", error->message);

    waitUntilSyncedOrDie(subprocess.get(), syncFds[0]);
    return true;
}

} // namespace WebKit

#endif // ENABLE(BUBBLEWRAP_SANDBOX)
