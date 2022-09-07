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

XDGDBusProxy::XDGDBusProxy(Type type, bool allowPortals)
    : m_type(type)
{
    switch (m_type) {
    case Type::SessionBus:
        m_dbusAddress = g_getenv("DBUS_SESSION_BUS_ADDRESS");
        break;
    case Type::AccessibilityBus:
#if ENABLE(ACCESSIBILITY)
        m_dbusAddress = WebCore::PlatformDisplay::sharedDisplay().accessibilityBusAddress().utf8();
#endif
        break;
    }

    if (m_dbusAddress.isNull() || !g_str_has_prefix(m_dbusAddress.data(), "unix:"))
        return;

    m_proxyPath = makeProxy();
    if (m_proxyPath.isNull())
        return;

#if USE(ATSPI)
    if (m_type == Type::AccessibilityBus)
        WebCore::PlatformDisplay::sharedDisplay().setAccessibilityBusAddress(makeString("unix:path=", m_proxyPath.data()));
#endif

    if (const char* path = strstr(m_dbusAddress.data(), "path=")) {
        path += strlen("path=");
        const char* pathEnd = path;
        while (*pathEnd && *pathEnd != ',')
            pathEnd++;

        m_path = CString(path, pathEnd - path);
    }

    m_syncFD = launch(allowPortals);
}

XDGDBusProxy::~XDGDBusProxy()
{
    if (m_syncFD != -1)
        close(m_syncFD);
}

CString XDGDBusProxy::makeProxy() const
{
    GUniquePtr<char> appRunDir(g_build_filename(g_get_user_runtime_dir(), BASE_DIRECTORY, nullptr));
    if (g_mkdir_with_parents(appRunDir.get(), 0700) == -1) {
        g_warning("Failed to mkdir for dbus proxy (%s): %s", appRunDir.get(), g_strerror(errno));
        return { };
    }

    auto proxyTemplate = [](Type type) -> const char* {
        switch (type) {
        case Type::SessionBus:
            return "bus-proxy-XXXXXX";
        case Type::AccessibilityBus:
            return "a11y-proxy-XXXXXX";
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    GUniquePtr<char> proxySocketTemplate(g_build_filename(appRunDir.get(), proxyTemplate(m_type), nullptr));
    int fd;
    if ((fd = g_mkstemp(proxySocketTemplate.get())) == -1) {
        g_warning("Failed to make socket file %s for dbus proxy: %s", proxySocketTemplate.get(), g_strerror(errno));
        return { };
    }

    close(fd);
    return proxySocketTemplate.get();
}

int XDGDBusProxy::launch(bool allowPortals) const
{
    int syncFds[2];
    if (pipe(syncFds) == -1)
        g_error("Failed to make syncfds for dbus-proxy: %s", g_strerror(errno));
    setCloseOnExec(syncFds[0]);

    GUniquePtr<char> syncFdStr(g_strdup_printf("--fd=%d", syncFds[1]));
    Vector<CString> proxyArgs = {
        m_dbusAddress, m_proxyPath,
        "--filter",
        syncFdStr.get()
    };

    auto enableLogging = [](Type type) -> bool {
        switch (type) {
        case Type::SessionBus:
            return !g_strcmp0(g_getenv("WEBKIT_ENABLE_DBUS_PROXY_LOGGING"), "1");
        case Type::AccessibilityBus:
            return !g_strcmp0(g_getenv("WEBKIT_ENABLE_A11Y_DBUS_PROXY_LOGGING"), "1");
        }
        RELEASE_ASSERT_NOT_REACHED();
    };
    if (enableLogging(m_type))
        proxyArgs.append("--log");

    switch (m_type) {
    case Type::SessionBus: {
#if ENABLE(MEDIA_SESSION)
        if (auto* app = g_application_get_default()) {
            if (const char* appID = g_application_get_application_id(app)) {
                auto mprisSessionID = makeString("--own=org.mpris.MediaPlayer2.", appID, ".*");
                proxyArgs.append(mprisSessionID.ascii().data());
            }
        }
#endif
        // GStreamers plugin install helper.
        proxyArgs.append("--call=org.freedesktop.PackageKit=org.freedesktop.PackageKit.Modify2.InstallGStreamerResources@/org/freedesktop/PackageKit");

        if (allowPortals)
            proxyArgs.append("--talk=org.freedesktop.portal.Desktop");
        break;
    }
    case Type::AccessibilityBus:
#if ENABLE(ACCESSIBILITY)
        proxyArgs.appendVector(Vector<CString> {
            "--sloppy-names",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.Socket.Embed@/org/a11y/atspi/accessible/root",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.Socket.Unembed@/org/a11y/atspi/accessible/root",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.Registry.GetRegisteredEvents@/org/a11y/atspi/registry",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.GetKeystrokeListeners@/org/a11y/atspi/registry/deviceeventcontroller",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.GetDeviceEventListeners@/org/a11y/atspi/registry/deviceeventcontroller",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.NotifyListenersSync@/org/a11y/atspi/registry/deviceeventcontroller",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.NotifyListenersAsync@/org/a11y/atspi/registry/deviceeventcontroller",
        });
#endif
        break;
    }

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

    // We are purposefully leaving syncFds[0] open here.
    // xdg-dbus-proxy will exit() itself once that is closed on our exit.

    ProcessLauncher::LaunchOptions launchOptions;
    launchOptions.processType = ProcessLauncher::ProcessType::DBusProxy;
#if ENABLE(ACCESSIBILITY)
    if (m_type == Type::AccessibilityBus && !m_path.isNull())
        launchOptions.extraSandboxPaths.add(m_path, SandboxPermission::ReadOnly);
#endif
    GUniqueOutPtr<GError> error;
    GRefPtr<GSubprocess> process = bubblewrapSpawn(launcher.get(), launchOptions, argv, &error.outPtr());
    if (!process)
        g_error("Failed to start dbus proxy: %s", error->message);

    // We need to ensure the proxy has created the socket.
    // FIXME: This is more blocking IO.
    char out;
    if (read(syncFds[0], &out, 1) != 1)
        g_error("Failed to fully launch dbus-proxy: %s", g_strerror(errno));

    return syncFds[0];
}

} // namespace WebKit

#endif // ENABLE(BUBBLEWRAP_SANDBOX)
