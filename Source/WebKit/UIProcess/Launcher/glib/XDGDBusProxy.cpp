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
#include <WebCore/PlatformDisplay.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(GTK)
#define BASE_DIRECTORY "webkitgtk"
#elif PLATFORM(WPE)
#define BASE_DIRECTORY "wpe"
#endif

namespace WebKit {

CString XDGDBusProxy::makePath(const char* dbusAddress)
{
    if (!dbusAddress || !g_str_has_prefix(dbusAddress, "unix:"))
        return { };

    if (const char* path = strstr(dbusAddress, "path=")) {
        path += strlen("path=");
        const char* pathEnd = path;
        while (*pathEnd && *pathEnd != ',')
            pathEnd++;

        return CString(path, pathEnd - path);
    }

    return { };
}

CString XDGDBusProxy::makeProxy(const char* proxyTemplate)
{
    GUniquePtr<char> appRunDir(g_build_filename(g_get_user_runtime_dir(), BASE_DIRECTORY, nullptr));
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

std::optional<std::pair<CString, CString>> XDGDBusProxy::dbusSessionProxy(AllowPortals allowPortals)
{
    if (!m_dbusSessionProxyPath.isNull())
        return std::pair<CString, CString> { m_dbusSessionProxyPath, m_dbusSessionPath };

    const char* dbusAddress = g_getenv("DBUS_SESSION_BUS_ADDRESS");
    m_dbusSessionPath = makePath(dbusAddress);
    if (m_dbusSessionPath.isNull())
        return std::nullopt;

    m_dbusSessionProxyPath = makeProxy("bus-proxy-XXXXXX");
    if (m_dbusSessionProxyPath.isNull()) {
        m_dbusSessionPath = { };
        return std::nullopt;
    }

    m_args.appendVector(Vector<CString> {
        CString(dbusAddress), m_dbusSessionProxyPath,
        "--filter",
        // GStreamers plugin install helper.
        "--call=org.freedesktop.PackageKit=org.freedesktop.PackageKit.Modify2.InstallGStreamerResources@/org/freedesktop/PackageKit"
    });

#if ENABLE(MEDIA_SESSION)
    if (auto* app = g_application_get_default()) {
        if (const char* appID = g_application_get_application_id(app)) {
            auto mprisSessionID = makeString("--own=org.mpris.MediaPlayer2.", appID, ".*");
            m_args.append(mprisSessionID.ascii().data());
        }
    }
#endif

    if (allowPortals == AllowPortals::Yes)
        m_args.append("--talk=org.freedesktop.portal.Desktop");

    if (!g_strcmp0(g_getenv("WEBKIT_ENABLE_DBUS_PROXY_LOGGING"), "1"))
        m_args.append("--log");

    return std::pair<CString, CString> { m_dbusSessionProxyPath, m_dbusSessionPath };
}

std::optional<std::pair<CString, CString>> XDGDBusProxy::accessibilityProxy()
{
#if ENABLE(ACCESSIBILITY)
    if (!m_accessibilityProxyPath.isNull())
        return std::pair<CString, CString> { m_accessibilityProxyPath, m_accessibilityPath };

    auto dbusAddress = WebCore::PlatformDisplay::sharedDisplay().accessibilityBusAddress().utf8();
    m_accessibilityPath = makePath(dbusAddress.data());
    if (m_accessibilityPath.isNull())
        return std::nullopt;

    m_accessibilityProxyPath = makeProxy("a11y-proxy-XXXXXX");
    if (m_accessibilityProxyPath.isNull()) {
        m_accessibilityPath = { };
        return std::nullopt;
    }

#if USE(ATSPI)
    WebCore::PlatformDisplay::sharedDisplay().setAccessibilityBusAddress(makeString("unix:path=", m_accessibilityPath.data()));
#endif

    m_args.appendVector(Vector<CString> {
        WTFMove(dbusAddress), m_accessibilityProxyPath,
        "--filter",
        "--sloppy-names",
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

    return std::pair<CString, CString> { m_accessibilityProxyPath, m_accessibilityPath };
#else
    return std::nullopt;
#endif
}

bool XDGDBusProxy::launch()
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

    // Warning: do not set a child setup function, because we want GIO to be able to spawn with
    // posix_spawn() rather than fork()/exec(), in order to better accomodate applications that use
    // a huge amount of memory or address space in the UI process, like Eclipse.
    GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE));
    g_subprocess_launcher_take_fd(launcher.get(), proxyFd, proxyFd);
    g_subprocess_launcher_take_fd(launcher.get(), syncFds[1], syncFds[1]);
    m_syncFD = UnixFileDescriptor(syncFds[0], UnixFileDescriptor::Adopt);

    // We are purposefully leaving syncFds[0] open here.
    // xdg-dbus-proxy will exit() itself once that is closed on our exit.

    ProcessLauncher::LaunchOptions launchOptions;
    launchOptions.processType = ProcessLauncher::ProcessType::DBusProxy;

    GUniqueOutPtr<GError> error;
    GRefPtr<GSubprocess> process = bubblewrapSpawn(launcher.get(), launchOptions, argv, &error.outPtr());
    if (!process)
        g_error("Failed to start dbus proxy: %s", error->message);

    // We need to ensure the proxy has created the socket.
    // FIXME: This is more blocking IO.
    char out;
    if (read(syncFds[0], &out, 1) != 1)
        g_error("Failed to fully launch dbus-proxy: %s", g_strerror(errno));

    return true;
}

} // namespace WebKit

#endif // ENABLE(BUBBLEWRAP_SANDBOX)
