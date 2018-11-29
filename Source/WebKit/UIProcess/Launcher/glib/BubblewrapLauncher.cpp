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

#include <WebCore/FileSystem.h>
#include <WebCore/PlatformDisplay.h>
#include <fcntl.h>
#include <glib.h>
#include <seccomp.h>
#include <sys/ioctl.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

#if __has_include(<sys/memfd.h>)

#include <sys/memfd.h>

#else

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

#define MFD_ALLOW_SEALING 2U

static int memfd_create(const char* name, unsigned flags)
{
    return syscall(__NR_memfd_create, name, flags);
}
#endif

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

enum class DBusAddressType {
    Normal,
    Abstract,
};

class XDGDBusProxyLauncher {
public:
    void setAddress(const char* dbusAddress, DBusAddressType addressType)
    {
        GUniquePtr<char> dbusPath = dbusAddressToPath(dbusAddress, addressType);
        if (!dbusPath.get())
            return;

        GUniquePtr<char> appRunDir(g_build_filename(g_get_user_runtime_dir(), g_get_prgname(), nullptr));
        m_proxyPath = makeProxyPath(appRunDir.get()).get();

        m_socket = dbusAddress;
        m_path = dbusPath.get();
    }

    bool isRunning() const { return m_isRunning; };
    const CString& path() const { return m_path; };
    const CString& proxyPath() const { return m_proxyPath; };

    void setPermissions(Vector<CString>&& permissions)
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isRunning());
        m_permissions = WTFMove(permissions);
    };

    void launch()
    {
        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(!isRunning());

        if (m_socket.isNull() || m_path.isNull() || m_proxyPath.isNull())
            return;

        int syncFds[2];
        if (pipe2 (syncFds, O_CLOEXEC) == -1)
            g_error("Failed to make syncfds for dbus-proxy: %s", g_strerror(errno));

        GUniquePtr<char> syncFdStr(g_strdup_printf("--fd=%d", syncFds[1]));

        Vector<CString> proxyArgs = {
            m_socket, m_proxyPath,
            "--filter",
            syncFdStr.get(),
        };

        if (!g_strcmp0(g_getenv("WEBKIT_ENABLE_DBUS_PROXY_LOGGING"), "1"))
            proxyArgs.append("--log");

        proxyArgs.appendVector(m_permissions);

        int proxyFd = argsToFd(proxyArgs, "dbus-proxy");
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

        GRefPtr<GSubprocessLauncher> launcher = adoptGRef(g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_INHERIT_FDS));
        g_subprocess_launcher_set_child_setup(launcher.get(), childSetupFunc, GINT_TO_POINTER(syncFds[1]), nullptr);
        g_subprocess_launcher_take_fd(launcher.get(), proxyFd, proxyFd);
        g_subprocess_launcher_take_fd(launcher.get(), syncFds[1], syncFds[1]);
        // We are purposefully leaving syncFds[0] open here.
        // xdg-dbus-proxy will exit() itself once that is closed on our exit

        GUniqueOutPtr<GError> error;
        GRefPtr<GSubprocess> process = adoptGRef(g_subprocess_launcher_spawnv(launcher.get(), argv, &error.outPtr()));
        if (!process.get())
            g_error("Failed to start dbus proxy: %s", error->message);

        char out;
        // We need to ensure the proxy has created the socket.
        // FIXME: This is more blocking IO.
        if (read (syncFds[0], &out, 1) != 1)
            g_error("Failed to fully launch dbus-proxy %s", g_strerror(errno));

        m_isRunning = true;
    };

private:
    static void childSetupFunc(gpointer userdata)
    {
        int fd = GPOINTER_TO_INT(userdata);
        fcntl(fd, F_SETFD, 0); // Unset CLOEXEC
    }

    static GUniquePtr<char> makeProxyPath(const char* appRunDir)
    {
        if (g_mkdir_with_parents(appRunDir, 0700) == -1) {
            g_warning("Failed to mkdir for dbus proxy (%s): %s", appRunDir, g_strerror(errno));
            return GUniquePtr<char>(nullptr);
        }

        GUniquePtr<char> proxySocketTemplate(g_build_filename(appRunDir, "dbus-proxy-XXXXXX", nullptr));
        int fd;
        if ((fd = g_mkstemp(proxySocketTemplate.get())) == -1) {
            g_warning("Failed to make socket file for dbus proxy: %s", g_strerror(errno));
            return GUniquePtr<char>(nullptr);
        }

        close(fd);
        return proxySocketTemplate;
    };

    static GUniquePtr<char> dbusAddressToPath(const char* address, DBusAddressType addressType = DBusAddressType::Normal)
    {
        if (!address)
            return nullptr;

        if (!g_str_has_prefix(address, "unix:"))
            return nullptr;

        const char* path = strstr(address, addressType == DBusAddressType::Abstract ? "abstract=" : "path=");
        if (!path)
            return nullptr;

        path += strlen(addressType == DBusAddressType::Abstract ? "abstract=" : "path=");
        const char* pathEnd = path;
        while (*pathEnd && *pathEnd != ',')
            pathEnd++;

        return GUniquePtr<char>(g_strndup(path, pathEnd - path));
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
    if (!path)
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

static void bindX11(Vector<CString>& args)
{
    const char* display = g_getenv("DISPLAY");
    if (!display || display[0] != ':' || !g_ascii_isdigit(const_cast<char*>(display)[1]))
        display = ":0";
    GUniquePtr<char> x11File(g_strdup_printf("/tmp/.X11-unix/X%s", display + 1));
    bindIfExists(args, x11File.get(), BindFlags::ReadWrite);

    const char* xauth = g_getenv("XAUTHORITY");
    if (!xauth) {
        const char* homeDir = g_get_home_dir();
        GUniquePtr<char> xauthFile(g_build_filename(homeDir, ".Xauthority", nullptr));
        bindIfExists(args, xauthFile.get());
    } else
        bindIfExists(args, xauth);
}

static void bindDconf(Vector<CString>& args)
{
    const char* runtimeDir = g_get_user_runtime_dir();
    GUniquePtr<char> dconfRuntimeDir(g_build_filename(runtimeDir, "dconf", nullptr));
    args.appendVector(Vector<CString>({ "--bind", dconfRuntimeDir.get(), dconfRuntimeDir.get() }));

    const char* dconfDir = g_getenv("DCONF_USER_CONFIG_DIR");
    if (dconfDir)
        bindIfExists(args, dconfDir);
    else {
        const char* configDir = g_get_user_config_dir();
        GUniquePtr<char> dconfConfigDir(g_build_filename(configDir, "dconf", nullptr));
        bindIfExists(args, dconfConfigDir.get(), BindFlags::ReadWrite);
    }
}

#if PLATFORM(WAYLAND) && USE(EGL)
static void bindWayland(Vector<CString>& args)
{
    const char* display = g_getenv("WAYLAND_DISPLAY");
    if (!display)
        display = "wayland-0";

    const char* runtimeDir = g_get_user_runtime_dir();
    GUniquePtr<char> waylandRuntimeFile(g_build_filename(runtimeDir, display, nullptr));
    bindIfExists(args, waylandRuntimeFile.get(), BindFlags::ReadWrite);
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

static void bindFonts(Vector<CString>& args)
{
    const char* configDir = g_get_user_config_dir();
    const char* homeDir = g_get_home_dir();
    const char* dataDir = g_get_user_data_dir();
    const char* cacheDir = g_get_user_cache_dir();

    // Configs can include custom dirs but then we have to parse them...
    GUniquePtr<char> fontConfig(g_build_filename(configDir, "fontconfig", nullptr));
    GUniquePtr<char> fontCache(g_build_filename(cacheDir, "fontconfig", nullptr));
    GUniquePtr<char> fontHomeConfig(g_build_filename(homeDir, ".fonts.conf", nullptr));
    GUniquePtr<char> fontHomeConfigDir(g_build_filename(configDir, ".fonts.conf.d", nullptr));
    GUniquePtr<char> fontData(g_build_filename(dataDir, "fonts", nullptr));
    GUniquePtr<char> fontHomeData(g_build_filename(homeDir, ".fonts", nullptr));
    bindIfExists(args, fontConfig.get());
    bindIfExists(args, fontCache.get(), BindFlags::ReadWrite);
    bindIfExists(args, fontHomeConfig.get());
    bindIfExists(args, fontHomeConfigDir.get());
    bindIfExists(args, fontData.get());
    bindIfExists(args, fontHomeData.get());
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

        proxy.launch();
    }

    if (proxy.proxyPath().data()) {
        args.appendVector(Vector<CString>({
            "--bind", proxy.proxyPath(), proxy.path(),
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
    // This syscall blacklist is copied from linux-user-chroot, which was in turn
    // clearly influenced by the Sandstorm.io blacklist.
    //
    // If you make any changes here, I suggest sending the changes along
    // to other sandbox maintainers. Using the libseccomp list is also
    // an appropriate venue:
    // https://groups.google.com/forum/#!topic/libseccomp
    //
    // A non-exhaustive list of links to container tooling that might
    // want to share this blacklist:
    //
    //  https://github.com/sandstorm-io/sandstorm
    //    in src/sandstorm/supervisor.c++
    //  http://cgit.freedesktop.org/xdg-app/xdg-app/
    //    in common/flatpak-run.c
    //  https://git.gnome.org/browse/linux-user-chroot
    //    in src/setup-seccomp.c
    struct scmp_arg_cmp cloneArg = SCMP_A0(SCMP_CMP_MASKED_EQ, CLONE_NEWUSER, CLONE_NEWUSER);
    struct scmp_arg_cmp ttyArg = SCMP_A1(SCMP_CMP_EQ, static_cast<scmp_datum_t>(TIOCSTI), static_cast<scmp_datum_t>(0));
    struct {
        int scall;
        struct scmp_arg_cmp* arg;
    } syscallBlacklist[] = {
        // Block dmesg
        { SCMP_SYS(syslog), nullptr },
        // Useless old syscall.
        { SCMP_SYS(uselib), nullptr },
        // Don't allow disabling accounting.
        { SCMP_SYS(acct), nullptr },
        // 16-bit code is unnecessary in the sandbox, and modify_ldt is a
        // historic source of interesting information leaks.
        { SCMP_SYS(modify_ldt), nullptr },
        // Don't allow reading current quota use.
        { SCMP_SYS(quotactl), nullptr },

        // Don't allow access to the kernel keyring.
        { SCMP_SYS(add_key), nullptr },
        { SCMP_SYS(keyctl), nullptr },
        { SCMP_SYS(request_key), nullptr },

        // Scary VM/NUMA ops 
        { SCMP_SYS(move_pages), nullptr },
        { SCMP_SYS(mbind), nullptr },
        { SCMP_SYS(get_mempolicy), nullptr },
        { SCMP_SYS(set_mempolicy), nullptr },
        { SCMP_SYS(migrate_pages), nullptr },

        // Don't allow subnamespace setups:
        { SCMP_SYS(unshare), nullptr },
        { SCMP_SYS(mount), nullptr },
        { SCMP_SYS(pivot_root), nullptr },
        { SCMP_SYS(clone), &cloneArg },

        // Don't allow faking input to the controlling tty (CVE-2017-5226)
        { SCMP_SYS(ioctl), &ttyArg },

        // Profiling operations; we expect these to be done by tools from outside
        // the sandbox. In particular perf has been the source of many CVEs.
        { SCMP_SYS(perf_event_open), nullptr },
        // Don't allow you to switch to bsd emulation or whatnot.
        { SCMP_SYS(personality), nullptr },
        { SCMP_SYS(ptrace), nullptr }
    };

    scmp_filter_ctx seccomp = seccomp_init(SCMP_ACT_ALLOW);
    if (!seccomp)
        g_error("Failed to init seccomp");

    for (auto& rule : syscallBlacklist) {
        int scall = rule.scall;
        int r;
        if (rule.arg)
            r = seccomp_rule_add(seccomp, SCMP_ACT_ERRNO(EPERM), scall, 1, rule.arg);
        else
            r = seccomp_rule_add(seccomp, SCMP_ACT_ERRNO(EPERM), scall, 0);
        if (r == -EFAULT) {
            seccomp_release(seccomp);
            g_error("Failed to add seccomp rule");
        }
    }

    int tmpfd = memfd_create("seccomp-bpf", 0);
    if (tmpfd == -1) {
        seccomp_release(seccomp);
        g_error("Failed to create memfd: %s", g_strerror(errno));
    }

    if (seccomp_export_bpf(seccomp, tmpfd)) {
        seccomp_release(seccomp);
        close(tmpfd);
        g_error("Failed to export seccomp bpf");
    }

    if (lseek(tmpfd, 0, SEEK_SET) < 0)
        g_error("lseek failed: %s", g_strerror(errno));

    seccomp_release(seccomp);
    return tmpfd;
}

static int createFlatpakInfo()
{
    GUniquePtr<GKeyFile> keyFile(g_key_file_new());

    const char* sharedPermissions[] = { "network", nullptr };
    g_key_file_set_string_list(keyFile.get(), "Context", "shared", sharedPermissions, sizeof(sharedPermissions));

    // xdg-desktop-portal relates your name to certain permissions so we want
    // them to be application unique which is best done via GApplication.
    GApplication* app = g_application_get_default();
    if (!app) {
        g_warning("GApplication is required for xdg-desktop-portal access in the WebKit sandbox. Actions that require xdg-desktop-portal will be broken.");
        return -1;
    }
    g_key_file_set_string(keyFile.get(), "Application", "name", g_application_get_application_id(app));

    size_t size;
    GUniqueOutPtr<GError> error;
    GUniquePtr<char> data(g_key_file_to_data(keyFile.get(), &size, &error.outPtr()));
    if (error.get()) {
        g_warning("%s", error->message);
        return -1;
    }

    return createSealedMemFdWithData("flatpak-info", data.get(), size);
}

GRefPtr<GSubprocess> bubblewrapSpawn(GSubprocessLauncher* launcher, const ProcessLauncher::LaunchOptions& launchOptions, char** argv, GError **error)
{
    ASSERT(launcher);

    // It is impossible to know what access arbitrary plugins need and since it is for legacy
    // reasons lets just leave it unsandboxed.
    if (launchOptions.processType == ProcessLauncher::ProcessType::Plugin64
        || launchOptions.processType == ProcessLauncher::ProcessType::Plugin32)
        return adoptGRef(g_subprocess_launcher_spawnv(launcher, argv, error));

    // For now we are just considering the network process trusted as it
    // requires a lot of access but doesn't execute arbitrary code like
    // the WebProcess where our focus lies.
    if (launchOptions.processType == ProcessLauncher::ProcessType::Network)
        return adoptGRef(g_subprocess_launcher_spawnv(launcher, argv, error));

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
        "--dir", "/run",
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
        "--ro-bind-try", "/lib64", "/lib64",
        "--ro-bind-try", "/usr/lib64", "/usr/lib64",
        "--ro-bind-try", "/usr/local/lib64", "/usr/local/lib64",

        "--ro-bind-try", PKGLIBEXECDIR, PKGLIBEXECDIR,
    };
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

    // NOTE: This has network access for HLS via GStreamer.
    if (launchOptions.processType == ProcessLauncher::ProcessType::Web) {
        static XDGDBusProxyLauncher proxy;

        // If Wayland in use don't grant X11
#if PLATFORM(WAYLAND) && USE(EGL)
        if (PlatformDisplay::sharedDisplay().type() == PlatformDisplay::Type::Wayland) {
            bindWayland(sandboxArgs);
            sandboxArgs.append("--unshare-ipc");
        } else
#endif
            bindX11(sandboxArgs);

        // NOTE: This is not a great solution but we just assume that applications create this directory
        // ahead of time if they require it.
        GUniquePtr<char> configDir(g_build_filename(g_get_user_config_dir(), g_get_prgname(), nullptr));
        GUniquePtr<char> cacheDir(g_build_filename(g_get_user_cache_dir(), g_get_prgname(), nullptr));

        Vector<String> extraPaths = { "applicationCacheDirectory", "waylandSocket"};
        for (const auto& path : extraPaths) {
            String extraPath = launchOptions.extraInitializationData.get(path);
            if (!extraPath.isEmpty())
                sandboxArgs.appendVector(Vector<CString>({ "--bind-try", extraPath.utf8(), extraPath.utf8() }));
        }

        sandboxArgs.appendVector(Vector<CString>({
            "--ro-bind-try", cacheDir.get(), cacheDir.get(),
            "--ro-bind-try", configDir.get(), configDir.get(),
        }));

        bindDBusSession(sandboxArgs, proxy);
        // FIXME: This needs to be restricted, upstream is working on it.
        bindDconf(sandboxArgs);
        // FIXME: We should move to Pipewire as soon as viable, Pulse doesn't restrict clients atm.
        bindPulse(sandboxArgs);
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
                // FIXME: Used by GTK on Wayland.
                "--talk=ca.desrt.dconf",
                // GStreamers plugin install helper.
                "--call=org.freedesktop.PackageKit=org.freedesktop.PackageKit.Modify2.InstallGStreamerResources@/org/freedesktop/PackageKit"
            };
            if (flatpakInfoFd != -1) {
                // xdg-desktop-portal used by GTK and us.
                permissions.append("--talk=org.freedesktop.portal.Desktop");
            }
            proxy.setPermissions(WTFMove(permissions));
            proxy.launch();
        }
    } else {
        // Only X11 users need this for XShm which is only the Web process.
        sandboxArgs.append("--unshare-ipc");
    }

#if ENABLE(DEVELOPER_MODE)
    const char* execDirectory = g_getenv("WEBKIT_EXEC_PATH");
    if (execDirectory) {
        String parentDir = FileSystem::directoryName(FileSystem::stringFromFileSystemRepresentation(execDirectory));
        bindIfExists(sandboxArgs, parentDir.utf8().data());
    }

    CString executablePath = getCurrentExecutablePath();
    if (!executablePath.isNull()) {
        // Our executable is `/foo/bar/bin/Process`, we want `/foo/bar` as a usable prefix
        String parentDir = FileSystem::directoryName(FileSystem::directoryName(FileSystem::stringFromFileSystemRepresentation(executablePath.data())));
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
