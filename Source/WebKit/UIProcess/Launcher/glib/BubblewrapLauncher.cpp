/*
 * Copyright (C) 2018 Igalia S.L.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "BubblewrapLauncher.h"

#if ENABLE(BUBBLEWRAP_SANDBOX)

#include <WebCore/PlatformDisplay.h>
#include <fcntl.h>
#include <glib.h>
#include <seccomp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

#if !defined(MFD_ALLOW_SEALING) && HAVE(LINUX_MEMFD_H)
#include <linux/memfd.h>
#endif

#include "Syscalls.h"

#if PLATFORM(GTK)
#include "WaylandCompositor.h"
#endif

#if PLATFORM(GTK)
#define BASE_DIRECTORY "webkitgtk"
#elif PLATFORM(WPE)
#define BASE_DIRECTORY "wpe"
#endif

#if !defined(MFD_ALLOW_SEALING) && HAVE(LINUX_MEMFD_H)

// These defines were added in glibc 2.27, the same release that added memfd_create.
// But the kernel added all of this in Linux 3.17. So it's totally safe for us to
// depend on, as long as we define it all ourselves. Remove this once we depend on
// glibc 2.27.

#define F_ADD_SEALS 1033
#define F_GET_SEALS 1034

#define F_SEAL_SEAL   0x0001
#define F_SEAL_SHRINK 0x0002
#define F_SEAL_GROW   0x0004
#define F_SEAL_WRITE  0x0008

static int memfd_create(const char* name, unsigned flags)
{
    return syscall(__NR_memfd_create, name, flags);
}
#endif // #if !defined(MFD_ALLOW_SEALING) && HAVE(LINUX_MEMFD_H)

namespace WebKit {
using namespace WebCore;

static int createSealedMemFdWithData(const char* name, gconstpointer data, size_t size)
{
    int fd = memfd_create(name, MFD_ALLOW_SEALING);
    if (fd == -1) {
        g_warning("memfd_create failed: %s", g_strerror(errno));
        return -1;
    }

    ssize_t bytesWritten = write(fd, data, size);
    if (bytesWritten < 0) {
        g_warning("Writing args to memfd failed: %s", g_strerror(errno));
        close(fd);
        return -1;
    }

    if (static_cast<size_t>(bytesWritten) != size) {
        g_warning("Failed to write all args to memfd");
        close(fd);
        return -1;
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        g_warning("lseek failed: %s", g_strerror(errno));
        close(fd);
        return -1;
    }

    if (fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL) == -1) {
        g_warning("Failed to seal memfd: %s", g_strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static int
argsToFd(const Vector<CString>& args, const char *name)
{
    GString* buffer = g_string_new(nullptr);

    for (const auto& arg : args)
        g_string_append_len(buffer, arg.data(), arg.length() + 1); // Include NUL

    GRefPtr<GBytes> bytes = adoptGRef(g_string_free_to_bytes(buffer));

    size_t size;
    gconstpointer data = g_bytes_get_data(bytes.get(), &size);

    int memfd = createSealedMemFdWithData(name, data, size);
    if (memfd == -1)
        g_error("Failed to write memfd");

    return memfd;
}

static const char* applicationId(GError** error)
{
    GApplication* app = g_application_get_default();
    if (!app) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "GApplication is required.");
        return nullptr;
    }

    const char* appID = g_application_get_application_id(app);
    if (!appID) {
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA, "GApplication must have a valid ID.");
        return nullptr;
    }
    return appID;
}

static int createFlatpakInfo()
{
    static NeverDestroyed<GUniquePtr<char>> data;
    static size_t size;

    if (!data.get()) {
        // xdg-desktop-portal relates your name to certain permissions so we want
        // them to be application unique which is best done via GApplication.
        GUniqueOutPtr<GError> error;
        const char* appID = applicationId(&error.outPtr());
        if (!appID)
            g_error("Unable to configure xdg-desktop-portal access in the WebKit sandbox: %s", error->message);

        GUniquePtr<GKeyFile> keyFile(g_key_file_new());
        g_key_file_set_string(keyFile.get(), "Application", "name", appID);
        data->reset(g_key_file_to_data(keyFile.get(), &size, nullptr));
    }

    return createSealedMemFdWithData("flatpak-info", data->get(), size);
}

enum class DBusAddressType {
    Normal,
    Abstract,
};

class XDGDBusProxyLauncher {
public:
    void setAddress(const char* dbusAddress, DBusAddressType addressType)
    {
        CString dbusPath = dbusAddressToPath(dbusAddress, addressType);
        if (dbusPath.isNull())
            return;

        GUniquePtr<char> appRunDir(g_build_filename(g_get_user_runtime_dir(), BASE_DIRECTORY, nullptr));
        m_proxyPath = makeProxyPath(appRunDir.get());

        m_socket = dbusAddress;
        m_path = WTFMove(dbusPath);
    }

    bool isRunning() const { return m_isRunning; };
    const CString& path() const { return m_path; };
    const CString& proxyPath() const { return m_proxyPath; };

    void setPermissions(Vector<CString>&& permissions)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isRunning());
        m_permissions = WTFMove(permissions);
    };

    void launch(bool enableLogging)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isRunning());

        if (m_socket.isNull() || m_path.isNull() || m_proxyPath.isNull())
            return;

        int syncFds[2];
        if (pipe(syncFds) == -1)
            g_error("Failed to make syncfds for dbus-proxy: %s", g_strerror(errno));
        setCloseOnExec(syncFds[0]);

        GUniquePtr<char> syncFdStr(g_strdup_printf("--fd=%d", syncFds[1]));

        Vector<CString> proxyArgs = {
            m_socket, m_proxyPath,
            "--filter",
            syncFdStr.get(),
        };

        if (enableLogging)
            proxyArgs.append("--log");

        proxyArgs.appendVector(m_permissions);

        int proxyFd = argsToFd(proxyArgs, "dbus-proxy");
        GUniquePtr<char> proxyArgsStr(g_strdup_printf("--args=%d", proxyFd));

        // We have to run xdg-dbus-proxy under bubblewrap because we need /.flatpak-info to exist in
        // xdg-dbus-proxy's mount namespace. Portals may use this as a trusted way to get the
        // sandboxed process's application ID, and will break if it's missing.
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

        GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_INHERIT_FDS));
        g_subprocess_launcher_take_fd(launcher.get(), proxyFd, proxyFd);
        g_subprocess_launcher_take_fd(launcher.get(), syncFds[1], syncFds[1]);

        // We are purposefully leaving syncFds[0] open here.
        // xdg-dbus-proxy will exit() itself once that is closed on our exit

        ProcessLauncher::LaunchOptions launchOptions;
        launchOptions.processType = ProcessLauncher::ProcessType::DBusProxy;
        GUniqueOutPtr<GError> error;
        GRefPtr<GSubprocess> process = bubblewrapSpawn(launcher.get(), launchOptions, argv, &error.outPtr());
        if (!process.get())
            g_error("Failed to start dbus proxy: %s", error->message);

        char out;
        // We need to ensure the proxy has created the socket.
        // FIXME: This is more blocking IO.
        if (read(syncFds[0], &out, 1) != 1)
            g_error("Failed to fully launch dbus-proxy %s", g_strerror(errno));

        m_isRunning = true;
    };

private:
    static CString makeProxyPath(const char* appRunDir)
    {
        if (g_mkdir_with_parents(appRunDir, 0700) == -1) {
            g_warning("Failed to mkdir for dbus proxy (%s): %s", appRunDir, g_strerror(errno));
            return { };
        }

        GUniquePtr<char> proxySocketTemplate(g_build_filename(appRunDir, "dbus-proxy-XXXXXX", nullptr));
        int fd;
        if ((fd = g_mkstemp(proxySocketTemplate.get())) == -1) {
            g_warning("Failed to make socket file for dbus proxy: %s", g_strerror(errno));
            return { };
        }

        close(fd);
        return CString(proxySocketTemplate.get());
    };

    static CString dbusAddressToPath(const char* address, DBusAddressType addressType = DBusAddressType::Normal)
    {
        if (!address)
            return { };

        if (!g_str_has_prefix(address, "unix:"))
            return { };

        const char* path = strstr(address, addressType == DBusAddressType::Abstract ? "abstract=" : "path=");
        if (!path)
            return { };

        path += strlen(addressType == DBusAddressType::Abstract ? "abstract=" : "path=");
        const char* pathEnd = path;
        while (*pathEnd && *pathEnd != ',')
            pathEnd++;

        return CString(path, pathEnd - path);
}

    CString m_socket;
    CString m_path;
    CString m_proxyPath;
    bool m_isRunning;
    Vector<CString> m_permissions;
};

enum class BindFlags {
    ReadOnly,
    ReadWrite,
    Device,
};

static void bindIfExists(Vector<CString>& args, const char* path, BindFlags bindFlags = BindFlags::ReadOnly)
{
    if (!path || path[0] == '\0')
        return;

    const char* bindType;
    if (bindFlags == BindFlags::Device)
        bindType = "--dev-bind-try";
    else if (bindFlags == BindFlags::ReadOnly)
        bindType = "--ro-bind-try";
    else
        bindType = "--bind-try";
    args.appendVector(Vector<CString>({ bindType, path, path }));
}

static void bindDBusSession(Vector<CString>& args, XDGDBusProxyLauncher& proxy)
{
    if (!proxy.isRunning())
        proxy.setAddress(g_getenv("DBUS_SESSION_BUS_ADDRESS"), DBusAddressType::Normal);

    if (proxy.proxyPath().data()) {
        args.appendVector(Vector<CString>({
            "--bind", proxy.proxyPath(), proxy.path(),
        }));
    }
}

#if PLATFORM(X11)
static void bindX11(Vector<CString>& args)
{
    const char* display = g_getenv("DISPLAY");
    if (display && display[0] == ':' && g_ascii_isdigit(const_cast<char*>(display)[1])) {
        const char* displayNumber = &display[1];
        const char* displayNumberEnd = displayNumber;
        while (g_ascii_isdigit(*displayNumberEnd))
            displayNumberEnd++;

        GUniquePtr<char> displayString(g_strndup(displayNumber, displayNumberEnd - displayNumber));
        GUniquePtr<char> x11File(g_strdup_printf("/tmp/.X11-unix/X%s", displayString.get()));
        bindIfExists(args, x11File.get(), BindFlags::ReadWrite);
    }

    const char* xauth = g_getenv("XAUTHORITY");
    if (!xauth) {
        const char* homeDir = g_get_home_dir();
        GUniquePtr<char> xauthFile(g_build_filename(homeDir, ".Xauthority", nullptr));
        bindIfExists(args, xauthFile.get());
    } else
        bindIfExists(args, xauth);
}
#endif

#if PLATFORM(WAYLAND) && USE(EGL)
static void bindWayland(Vector<CString>& args)
{
    const char* display = g_getenv("WAYLAND_DISPLAY");
    if (!display)
        display = "wayland-0";

    const char* runtimeDir = g_get_user_runtime_dir();
    GUniquePtr<char> waylandRuntimeFile(g_build_filename(runtimeDir, display, nullptr));
    bindIfExists(args, waylandRuntimeFile.get(), BindFlags::ReadWrite);

#if !USE(WPE_RENDERER)
    if (WaylandCompositor::singleton().isRunning()) {
        String displayName = WaylandCompositor::singleton().displayName();
        waylandRuntimeFile.reset(g_build_filename(runtimeDir, displayName.utf8().data(), nullptr));
        bindIfExists(args, waylandRuntimeFile.get(), BindFlags::ReadWrite);
    }
#endif
}
#endif

static void bindPulse(Vector<CString>& args)
{
    // FIXME: The server can be defined in config files we'd have to parse.
    // They can also be set as X11 props but that is getting a bit ridiculous.
    const char* pulseServer = g_getenv("PULSE_SERVER");
    if (pulseServer) {
        if (g_str_has_prefix(pulseServer, "unix:"))
            bindIfExists(args, pulseServer + 5, BindFlags::ReadWrite);
        // else it uses tcp
    } else {
        const char* runtimeDir = g_get_user_runtime_dir();
        GUniquePtr<char> pulseRuntimeDir(g_build_filename(runtimeDir, "pulse", nullptr));
        bindIfExists(args, pulseRuntimeDir.get(), BindFlags::ReadWrite);
    }

    const char* pulseConfig = g_getenv("PULSE_CLIENTCONFIG");
    if (pulseConfig)
        bindIfExists(args, pulseConfig);

    const char* configDir = g_get_user_config_dir();
    GUniquePtr<char> pulseConfigDir(g_build_filename(configDir, "pulse", nullptr));
    bindIfExists(args, pulseConfigDir.get());

    const char* homeDir = g_get_home_dir();
    GUniquePtr<char> pulseHomeConfigDir(g_build_filename(homeDir, ".pulse", nullptr));
    GUniquePtr<char> asoundHomeConfigDir(g_build_filename(homeDir, ".asoundrc", nullptr));
    bindIfExists(args, pulseHomeConfigDir.get());
    bindIfExists(args, asoundHomeConfigDir.get());

    // This is the ultimate fallback to raw ALSA
    bindIfExists(args, "/dev/snd", BindFlags::Device);
}

static void bindSndio(Vector<CString>& args)
{
    bindIfExists(args, "/tmp/sndio", BindFlags::ReadWrite);

    GUniquePtr<char> sndioUidDir(g_strdup_printf("/tmp/sndio-%d", getuid()));
    bindIfExists(args, sndioUidDir.get(), BindFlags::ReadWrite);

    const char* homeDir = g_get_home_dir();
    GUniquePtr<char> sndioHomeDir(g_build_filename(homeDir, ".sndio", nullptr));
    bindIfExists(args, sndioHomeDir.get(), BindFlags::ReadWrite);
}

static void bindFonts(Vector<CString>& args)
{
    const char* configDir = g_get_user_config_dir();
    const char* homeDir = g_get_home_dir();
    const char* dataDir = g_get_user_data_dir();
    const char* cacheDir = g_get_user_cache_dir();

    // Configs can include custom dirs but then we have to parse them...
    GUniquePtr<char> fontConfig(g_build_filename(configDir, "fontconfig", nullptr));
    GUniquePtr<char> fontConfigHome(g_build_filename(homeDir, ".fontconfig", nullptr));
    GUniquePtr<char> fontCache(g_build_filename(cacheDir, "fontconfig", nullptr));
    GUniquePtr<char> fontHomeConfig(g_build_filename(homeDir, ".fonts.conf", nullptr));
    GUniquePtr<char> fontHomeConfigDir(g_build_filename(configDir, ".fonts.conf.d", nullptr));
    GUniquePtr<char> fontData(g_build_filename(dataDir, "fonts", nullptr));
    GUniquePtr<char> fontHomeData(g_build_filename(homeDir, ".fonts", nullptr));
    bindIfExists(args, fontConfig.get());
    bindIfExists(args, fontConfigHome.get());
    bindIfExists(args, fontCache.get(), BindFlags::ReadWrite);
    bindIfExists(args, fontHomeConfig.get());
    bindIfExists(args, fontHomeConfigDir.get());
    bindIfExists(args, fontData.get());
    bindIfExists(args, fontHomeData.get());
    bindIfExists(args, "/var/cache/fontconfig"); // Used by Debian.
}

#if PLATFORM(GTK)
static void bindGtkData(Vector<CString>& args)
{
    const char* configDir = g_get_user_config_dir();
    const char* dataDir = g_get_user_data_dir();
    const char* homeDir = g_get_home_dir();

    GUniquePtr<char> gtkConfig(g_build_filename(configDir, "gtk-3.0", nullptr));
    GUniquePtr<char> themeData(g_build_filename(dataDir, "themes", nullptr));
    GUniquePtr<char> themeHomeData(g_build_filename(homeDir, ".themes", nullptr));
    GUniquePtr<char> iconHomeData(g_build_filename(homeDir, ".icons", nullptr));
    bindIfExists(args, gtkConfig.get());
    bindIfExists(args, themeData.get());
    bindIfExists(args, themeHomeData.get());
    bindIfExists(args, iconHomeData.get());
}

static void bindA11y(Vector<CString>& args)
{
    static XDGDBusProxyLauncher proxy;

    if (!proxy.isRunning()) {
        // FIXME: Avoid blocking IO... (It is at least a one-time cost)
        GRefPtr<GDBusConnection> sessionBus = adoptGRef(g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr));
        if (!sessionBus.get())
            return;

        GRefPtr<GDBusMessage> msg = adoptGRef(g_dbus_message_new_method_call(
            "org.a11y.Bus", "/org/a11y/bus", "org.a11y.Bus", "GetAddress"));
        g_dbus_message_set_body(msg.get(), g_variant_new("()"));
        GRefPtr<GDBusMessage> reply = adoptGRef(g_dbus_connection_send_message_with_reply_sync(
            sessionBus.get(), msg.get(),
            G_DBUS_SEND_MESSAGE_FLAGS_NONE,
            30000,
            nullptr,
            nullptr,
            nullptr));

        if (reply.get()) {
            GUniqueOutPtr<GError> error;
            if (g_dbus_message_to_gerror(reply.get(), &error.outPtr())) {
                if (!g_error_matches(error.get(), G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN))
                    g_warning("Can't find a11y bus: %s", error->message);
            } else {
                GUniqueOutPtr<char> a11yAddress;
                g_variant_get(g_dbus_message_get_body(reply.get()), "(s)", &a11yAddress.outPtr());
                proxy.setAddress(a11yAddress.get(), DBusAddressType::Abstract);
            }
        }

        proxy.setPermissions({
            "--sloppy-names",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.Socket.Embed@/org/a11y/atspi/accessible/root",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.Socket.Unembed@/org/a11y/atspi/accessible/root",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.Registry.GetRegisteredEvents@/org/a11y/atspi/registry",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.GetKeystrokeListeners@/org/a11y/atspi/registry/deviceeventcontroller",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.GetDeviceEventListeners@/org/a11y/atspi/registry/deviceeventcontroller",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.NotifyListenersSync@/org/a11y/atspi/registry/deviceeventcontroller",
            "--call=org.a11y.atspi.Registry=org.a11y.atspi.DeviceEventController.NotifyListenersAsync@/org/a11y/atspi/registry/deviceeventcontroller",
        });

        proxy.launch(!g_strcmp0(g_getenv("WEBKIT_ENABLE_A11Y_DBUS_PROXY_LOGGING"), "1"));
    }

    if (proxy.proxyPath().data()) {
        GUniquePtr<char> proxyAddress(g_strdup_printf("unix:path=%s", proxy.proxyPath().data()));
        args.appendVector(Vector<CString>({
            "--ro-bind", proxy.proxyPath(), proxy.proxyPath(),
            "--setenv", "AT_SPI_BUS_ADDRESS", proxyAddress.get(),
        }));
    }
}
#endif

static bool bindPathVar(Vector<CString>& args, const char* varname)
{
    const char* pathValue = g_getenv(varname);
    if (!pathValue)
        return false;

    GUniquePtr<char*> splitPaths(g_strsplit(pathValue, ":", -1));
    for (size_t i = 0; splitPaths.get()[i]; ++i)
        bindIfExists(args, splitPaths.get()[i]);

    return true;
}

static void bindGStreamerData(Vector<CString>& args)
{
    if (!bindPathVar(args, "GST_PLUGIN_PATH_1_0"))
        bindPathVar(args, "GST_PLUGIN_PATH");

    if (!bindPathVar(args, "GST_PLUGIN_SYSTEM_PATH_1_0")) {
        if (!bindPathVar(args, "GST_PLUGIN_SYSTEM_PATH")) {
            GUniquePtr<char> gstData(g_build_filename(g_get_user_data_dir(), "gstreamer-1.0", nullptr));
            bindIfExists(args, gstData.get());
        }
    }

    GUniquePtr<char> gstCache(g_build_filename(g_get_user_cache_dir(), "gstreamer-1.0", nullptr));
    bindIfExists(args, gstCache.get(), BindFlags::ReadWrite);

    // /usr/lib is already added so this is only requried for other dirs
    const char* scannerPath = g_getenv("GST_PLUGIN_SCANNER") ?: "/usr/libexec/gstreamer-1.0/gst-plugin-scanner";
    const char* helperPath = g_getenv("GST_INSTALL_PLUGINS_HELPER ") ?: "/usr/libexec/gst-install-plugins-helper";

    bindIfExists(args, scannerPath);
    bindIfExists(args, helperPath);
}

static void bindOpenGL(Vector<CString>& args)
{
    args.appendVector(Vector<CString>({
        "--dev-bind-try", "/dev/dri", "/dev/dri",
        // Mali
        "--dev-bind-try", "/dev/mali", "/dev/mali",
        "--dev-bind-try", "/dev/mali0", "/dev/mali0",
        "--dev-bind-try", "/dev/umplock", "/dev/umplock",
        // Nvidia
        "--dev-bind-try", "/dev/nvidiactl", "/dev/nvidiactl",
        "--dev-bind-try", "/dev/nvidia0", "/dev/nvidia0",
        "--dev-bind-try", "/dev/nvidia", "/dev/nvidia",
        // Adreno
        "--dev-bind-try", "/dev/kgsl-3d0", "/dev/kgsl-3d0",
        "--dev-bind-try", "/dev/ion", "/dev/ion",
#if PLATFORM(WPE)
        "--dev-bind-try", "/dev/fb0", "/dev/fb0",
        "--dev-bind-try", "/dev/fb1", "/dev/fb1",
#endif
    }));
}

static void bindV4l(Vector<CString>& args)
{
    args.appendVector(Vector<CString>({
        "--dev-bind-try", "/dev/v4l", "/dev/v4l",
        // Not pretty but a stop-gap for pipewire anyway.
        "--dev-bind-try", "/dev/video0", "/dev/video0",
        "--dev-bind-try", "/dev/video1", "/dev/video1",
        "--dev-bind-try", "/dev/video2", "/dev/video2",
        "--dev-bind-try", "/dev/media0", "/dev/media0",
    }));
}

static void bindSymlinksRealPath(Vector<CString>& args, const char* path)
{
    char realPath[PATH_MAX];

    if (realpath(path, realPath) && strcmp(path, realPath)) {
        args.appendVector(Vector<CString>({
            "--ro-bind", realPath, realPath,
        }));
    }
}

// Translate a libseccomp error code into an error message. libseccomp
// mostly returns negative errno values such as -ENOMEM, but some
// standard errno values are used for non-standard purposes where their
// strerror() would be misleading.
static const char* seccompStrerror(int negativeErrno)
{
    RELEASE_ASSERT_WITH_MESSAGE(negativeErrno < 0, "Non-negative error value from libseccomp?");
    RELEASE_ASSERT_WITH_MESSAGE(negativeErrno > INT_MIN, "Out of range error value from libseccomp?");

    switch (negativeErrno) {
    case -EDOM:
        return "Architecture-specific failure";
    case -EFAULT:
        return "Internal libseccomp failure (unknown syscall?)";
    case -ECANCELED:
        return "System failure beyond the control of libseccomp";
    }

    // e.g. -ENOMEM: the result of strerror() is good enough
    return g_strerror(-negativeErrno);
}

static int setupSeccomp()
{
    // NOTE: This is shared code (flatpak-run.c - LGPLv2.1+)
    // There are today a number of different Linux container
    // implementations. That will likely continue for long into the
    // future. But we can still try to share code, and it's important
    // to do so because it affects what library and application writers
    // can do, and we should support code portability between different
    // container tools.
    //
    // This syscall block list is copied from linux-user-chroot, which was in turn
    // clearly influenced by the Sandstorm.io block list.
    //
    // If you make any changes here, I suggest sending the changes along
    // to other sandbox maintainers. Using the libseccomp list is also
    // an appropriate venue:
    // https://groups.google.com/forum/#!topic/libseccomp
    //
    // A non-exhaustive list of links to container tooling that might
    // want to share this block list:
    //
    //  https://github.com/sandstorm-io/sandstorm
    //    in src/sandstorm/supervisor.c++
    //  http://cgit.freedesktop.org/xdg-app/xdg-app/
    //    in common/flatpak-run.c
    //  https://git.gnome.org/browse/linux-user-chroot
    //    in src/setup-seccomp.c
    //
    // Other useful resources:
    // https://github.com/systemd/systemd/blob/HEAD/src/shared/seccomp-util.c
    // https://github.com/moby/moby/blob/HEAD/profiles/seccomp/default.json

#if defined(__s390__) || defined(__s390x__) || defined(__CRIS__)
    // Architectures with CONFIG_CLONE_BACKWARDS2: the child stack
    // and flags arguments are reversed so the flags come second.
    struct scmp_arg_cmp cloneArg = SCMP_A1(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER);
#else
    // Normally the flags come first.
    struct scmp_arg_cmp cloneArg = SCMP_A0(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER);
#endif

    struct scmp_arg_cmp ttyArg = SCMP_A1(SCMP_CMP_MASKED_EQ, 0xFFFFFFFFu, TIOCSTI);
    struct {
        int scall;
        int errnum;
        struct scmp_arg_cmp* arg;
    } syscallBlockList[] = {
        // Block dmesg
        { SCMP_SYS(syslog), EPERM, nullptr },
        // Useless old syscall.
        { SCMP_SYS(uselib), EPERM, nullptr },
        // Don't allow disabling accounting.
        { SCMP_SYS(acct), EPERM, nullptr },
        // 16-bit code is unnecessary in the sandbox, and modify_ldt is a
        // historic source of interesting information leaks.
        { SCMP_SYS(modify_ldt), EPERM, nullptr },
        // Don't allow reading current quota use.
        { SCMP_SYS(quotactl), EPERM, nullptr },

        // Don't allow access to the kernel keyring.
        { SCMP_SYS(add_key), EPERM, nullptr },
        { SCMP_SYS(keyctl), EPERM, nullptr },
        { SCMP_SYS(request_key), EPERM, nullptr },

        // Scary VM/NUMA ops 
        { SCMP_SYS(move_pages), EPERM, nullptr },
        { SCMP_SYS(mbind), EPERM, nullptr },
        { SCMP_SYS(get_mempolicy), EPERM, nullptr },
        { SCMP_SYS(set_mempolicy), EPERM, nullptr },
        { SCMP_SYS(migrate_pages), EPERM, nullptr },

        // Don't allow subnamespace setups:
        { SCMP_SYS(unshare), EPERM, nullptr },
        { SCMP_SYS(setns), EPERM, nullptr },
        { SCMP_SYS(mount), EPERM, nullptr },
        { SCMP_SYS(umount), EPERM, nullptr },
        { SCMP_SYS(umount2), EPERM, nullptr },
        { SCMP_SYS(pivot_root), EPERM, nullptr },
        { SCMP_SYS(chroot), EPERM, nullptr },
        { SCMP_SYS(clone), EPERM, &cloneArg },

        // Don't allow faking input to the controlling tty (CVE-2017-5226)
        { SCMP_SYS(ioctl), EPERM, &ttyArg },

        // seccomp can't look into clone3()'s struct clone_args to check whether
        // the flags are OK, so we have no choice but to block clone3().
        // Return ENOSYS so user-space will fall back to clone().
        // (GHSA-67h7-w3jq-vh4q; see also https://github.com/moby/moby/commit/9f6b562d)
        { SCMP_SYS(clone3), ENOSYS, nullptr },

        // New mount manipulation APIs can also change our VFS. There's no
        // legitimate reason to do these in the sandbox, so block all of them
        // rather than thinking about which ones might be dangerous.
        // (GHSA-67h7-w3jq-vh4q)
        { SCMP_SYS(open_tree), ENOSYS, nullptr },
        { SCMP_SYS(move_mount), ENOSYS, nullptr },
        { SCMP_SYS(fsopen), ENOSYS, nullptr },
        { SCMP_SYS(fsconfig), ENOSYS, nullptr },
        { SCMP_SYS(fsmount), ENOSYS, nullptr },
        { SCMP_SYS(fspick), ENOSYS, nullptr },
        { SCMP_SYS(mount_setattr), ENOSYS, nullptr },

        // Profiling operations; we expect these to be done by tools from outside
        // the sandbox. In particular perf has been the source of many CVEs.
        { SCMP_SYS(perf_event_open), EPERM, nullptr },
        // Don't allow you to switch to bsd emulation or whatnot.
        { SCMP_SYS(personality), EPERM, nullptr },
        { SCMP_SYS(ptrace), EPERM, nullptr }
    };

    scmp_filter_ctx seccomp = seccomp_init(SCMP_ACT_ALLOW);
    if (!seccomp)
        g_error("Failed to init seccomp");

    for (auto& rule : syscallBlockList) {
        int r;
        if (rule.arg)
            r = seccomp_rule_add(seccomp, SCMP_ACT_ERRNO(rule.errnum), rule.scall, 1, *rule.arg);
        else
            r = seccomp_rule_add(seccomp, SCMP_ACT_ERRNO(rule.errnum), rule.scall, 0);
        // EFAULT means "internal libseccomp error", but in practice we get
        // this for syscall numbers added via Syscalls.h (flatpak-syscalls-private.h)
        // when trying to filter them on a non-native architecture, because
        // libseccomp cannot map the syscall number to a name and back to a
        // number for the non-native architecture.
        if (r == -EFAULT)
            g_info("Unable to block syscall %d: syscall not known to libseccomp?", rule.scall);
        else if (r < 0)
            g_error("Failed to block syscall %d: %s", rule.scall, seccompStrerror(r));
    }

    int tmpfd = memfd_create("seccomp-bpf", 0);
    if (tmpfd == -1)
        g_error("Failed to create memfd: %s", g_strerror(errno));

    if (int r = seccomp_export_bpf(seccomp, tmpfd))
        g_error("Failed to export seccomp bpf: %s", seccompStrerror(r));

    if (lseek(tmpfd, 0, SEEK_SET) < 0)
        g_error("lseek failed: %s", g_strerror(errno));

    seccomp_release(seccomp);
    return tmpfd;
}

static bool shouldUnshareNetwork(ProcessLauncher::ProcessType processType)
{
    // xdg-dbus-proxy needs access to host abstract sockets to connect to the a11y bus. Secure
    // host services must not use abstract sockets.
    if (processType == ProcessLauncher::ProcessType::DBusProxy)
        return false;

#if PLATFORM(X11)
    // Also, the web process needs access to host networking if the X server is running over TCP or
    // on a different host's Unix socket; this is likely the case if the first character of DISPLAY
    // is not a colon.
    if (processType == ProcessLauncher::ProcessType::Web && PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11) {
        const char* display = g_getenv("DISPLAY");
        if (display && display[0] != ':')
            return false;
    }
#endif

    // Otherwise, only the network process should have network access. If we are the network
    // process, then we are not sandboxed and have already bailed out before this point.
    return true;
}

GRefPtr<GSubprocess> bubblewrapSpawn(GSubprocessLauncher* launcher, const ProcessLauncher::LaunchOptions& launchOptions, char** argv, GError **error)
{
    ASSERT(launcher);

    // For now we are just considering the network process trusted as it
    // requires a lot of access but doesn't execute arbitrary code like
    // the WebProcess where our focus lies.
    if (launchOptions.processType == ProcessLauncher::ProcessType::Network)
        return adoptGRef(g_subprocess_launcher_spawnv(launcher, argv, error));

    const char* runDir = g_get_user_runtime_dir();
    Vector<CString> sandboxArgs = {
        "--die-with-parent",
        "--unshare-pid",
        "--unshare-uts",

        // We assume /etc has safe permissions.
        // At a later point we can start masking privacy-concerning files.
        "--ro-bind", "/etc", "/etc",
        "--dev", "/dev",
        "--proc", "/proc",
        "--tmpfs", "/tmp",
        "--unsetenv", "TMPDIR",
        "--dir", runDir,
        "--setenv", "XDG_RUNTIME_DIR", runDir,
        "--symlink", "../run", "/var/run",
        "--symlink", "../tmp", "/var/tmp",
        "--ro-bind", "/sys/block", "/sys/block",
        "--ro-bind", "/sys/bus", "/sys/bus",
        "--ro-bind", "/sys/class", "/sys/class",
        "--ro-bind", "/sys/dev", "/sys/dev",
        "--ro-bind", "/sys/devices", "/sys/devices",

        "--ro-bind-try", "/usr/share", "/usr/share",
        "--ro-bind-try", "/usr/local/share", "/usr/local/share",
        "--ro-bind-try", DATADIR, DATADIR,

        // We only grant access to the libdirs webkit is built with and
        // guess system libdirs. This will always have some edge cases.
        "--ro-bind-try", "/lib", "/lib",
        "--ro-bind-try", "/usr/lib", "/usr/lib",
        "--ro-bind-try", "/usr/local/lib", "/usr/local/lib",
        "--ro-bind-try", LIBDIR, LIBDIR,
#if CPU(ADDRESS64)
        "--ro-bind-try", "/lib64", "/lib64",
        "--ro-bind-try", "/usr/lib64", "/usr/lib64",
        "--ro-bind-try", "/usr/local/lib64", "/usr/local/lib64",
#else
        "--ro-bind-try", "/lib32", "/lib32",
        "--ro-bind-try", "/usr/lib32", "/usr/lib32",
        "--ro-bind-try", "/usr/local/lib32", "/usr/local/lib32",
#endif

        "--ro-bind-try", PKGLIBEXECDIR, PKGLIBEXECDIR,
    };

    if (launchOptions.processType == ProcessLauncher::ProcessType::DBusProxy) {
        sandboxArgs.appendVector(Vector<CString>({
            "--ro-bind", DBUS_PROXY_EXECUTABLE, DBUS_PROXY_EXECUTABLE,
            // This is a lot of access, but xdg-dbus-proxy is trusted so that's OK. It's sandboxed
            // only because we have to mount .flatpak-info in its mount namespace. The user rundir
            // is where we mount our proxy socket.
            "--bind", runDir, runDir,
        }));
    }

    if (shouldUnshareNetwork(launchOptions.processType))
        sandboxArgs.append("--unshare-net");

    // We would have to parse ld config files for more info.
    bindPathVar(sandboxArgs, "LD_LIBRARY_PATH");

    const char* libraryPath = g_getenv("LD_LIBRARY_PATH");
    if (libraryPath && libraryPath[0]) {
        // On distros using a suid bwrap it drops this env var
        // so we have to pass it through to the children.
        sandboxArgs.appendVector(Vector<CString>({
            "--setenv", "LD_LIBRARY_PATH", libraryPath,
        }));
    }

    bindSymlinksRealPath(sandboxArgs, "/etc/resolv.conf");
    bindSymlinksRealPath(sandboxArgs, "/etc/localtime");

    // xdg-desktop-portal defaults to assuming you are host application with
    // full permissions unless it can identify you as a snap or flatpak.
    // The easiest method is for us to pretend to be a flatpak and if that
    // fails just blocking portals entirely as it just becomes a sandbox escape.
    int flatpakInfoFd = createFlatpakInfo();
    if (flatpakInfoFd != -1) {
        g_subprocess_launcher_take_fd(launcher, flatpakInfoFd, flatpakInfoFd);
        GUniquePtr<char> flatpakInfoFdStr(g_strdup_printf("%d", flatpakInfoFd));

        sandboxArgs.appendVector(Vector<CString>({
            "--ro-bind-data", flatpakInfoFdStr.get(), "/.flatpak-info"
        }));
    }

    if (launchOptions.processType == ProcessLauncher::ProcessType::Web) {
        static XDGDBusProxyLauncher proxy;

#if PLATFORM(WAYLAND) && USE(EGL)
        if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
            bindWayland(sandboxArgs);
            sandboxArgs.append("--unshare-ipc");
        }
#endif
#if PLATFORM(X11)
        if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::X11)
            bindX11(sandboxArgs);
#endif

        for (const auto& pathAndPermission : launchOptions.extraWebProcessSandboxPaths) {
            sandboxArgs.appendVector(Vector<CString>({
                pathAndPermission.value == SandboxPermission::ReadOnly ? "--ro-bind-try": "--bind-try",
                pathAndPermission.key, pathAndPermission.key
            }));
        }

        Vector<String> extraPaths = { "applicationCacheDirectory", "mediaKeysDirectory", "waylandSocket", "webSQLDatabaseDirectory" };
        for (const auto& path : extraPaths) {
            String extraPath = launchOptions.extraInitializationData.get(path);
            if (!extraPath.isEmpty())
                sandboxArgs.appendVector(Vector<CString>({ "--bind-try", extraPath.utf8(), extraPath.utf8() }));
        }

        bindDBusSession(sandboxArgs, proxy);
        // FIXME: We should move to Pipewire as soon as viable, Pulse doesn't restrict clients atm.
        bindPulse(sandboxArgs);
        bindSndio(sandboxArgs);
        bindFonts(sandboxArgs);
        bindGStreamerData(sandboxArgs);
        bindOpenGL(sandboxArgs);
        // FIXME: This is also fixed by Pipewire once in use.
        bindV4l(sandboxArgs);
#if PLATFORM(GTK)
        bindA11y(sandboxArgs);
        bindGtkData(sandboxArgs);
#endif

        if (!proxy.isRunning()) {
            Vector<CString> permissions = {
                // GStreamers plugin install helper.
                "--call=org.freedesktop.PackageKit=org.freedesktop.PackageKit.Modify2.InstallGStreamerResources@/org/freedesktop/PackageKit"
            };
            if (flatpakInfoFd != -1) {
                // xdg-desktop-portal used by GTK and us.
                permissions.append("--talk=org.freedesktop.portal.Desktop");
            }
            proxy.setPermissions(WTFMove(permissions));
            proxy.launch(!g_strcmp0(g_getenv("WEBKIT_ENABLE_DBUS_PROXY_LOGGING"), "1"));
        }
    } else {
        // Only X11 users need this for XShm which is only the Web process.
        sandboxArgs.append("--unshare-ipc");
    }

#if ENABLE(DEVELOPER_MODE)
    const char* execDirectory = g_getenv("WEBKIT_EXEC_PATH");
    if (execDirectory) {
        String parentDir = FileSystem::parentPath(FileSystem::stringFromFileSystemRepresentation(execDirectory));
        bindIfExists(sandboxArgs, parentDir.utf8().data());
    }

    CString executablePath = getCurrentExecutablePath();
    if (!executablePath.isNull()) {
        // Our executable is `/foo/bar/bin/Process`, we want `/foo/bar` as a usable prefix
        String parentDir = FileSystem::parentPath(FileSystem::parentPath(FileSystem::stringFromFileSystemRepresentation(executablePath.data())));
        bindIfExists(sandboxArgs, parentDir.utf8().data());
    }
#endif

    int seccompFd = setupSeccomp();
    GUniquePtr<char> fdStr(g_strdup_printf("%d", seccompFd));
    g_subprocess_launcher_take_fd(launcher, seccompFd, seccompFd);
    sandboxArgs.appendVector(Vector<CString>({ "--seccomp", fdStr.get() }));

    int bwrapFd = argsToFd(sandboxArgs, "bwrap");
    GUniquePtr<char> bwrapFdStr(g_strdup_printf("%d", bwrapFd));
    g_subprocess_launcher_take_fd(launcher, bwrapFd, bwrapFd);

    Vector<CString> bwrapArgs = {
        BWRAP_EXECUTABLE,
        "--args",
        bwrapFdStr.get(),
        "--",
    };

    char** newArgv = g_newa(char*, g_strv_length(argv) + bwrapArgs.size() + 1);
    size_t i = 0;

    for (auto& arg : bwrapArgs)
        newArgv[i++] = const_cast<char*>(arg.data());
    for (size_t x = 0; argv[x]; x++)
        newArgv[i++] = argv[x];
    newArgv[i++] = nullptr;

    return adoptGRef(g_subprocess_launcher_spawnv(launcher, newArgv, error));
}

};

#endif // ENABLE(BUBBLEWRAP_SANDBOX)
