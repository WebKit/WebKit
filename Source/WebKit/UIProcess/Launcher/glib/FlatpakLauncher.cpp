/*
 * Copyright (C) 2020, 2026 Igalia S.L.
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
#include "FlatpakLauncher.h"

#if OS(LINUX)

#include "Logging.h"
#include <fcntl.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/OptionSet.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/Sandbox.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebKit {

// See org.freedesktop.portal.Flatpak.xml in the flatpak sources.
enum class FlatpakSpawnFlags : uint32_t {
    Sandbox = 1 << 2,
    NoNetwork = 1 << 3,
    WatchBus = 1 << 4,
    ExposePids = 1 << 5,
};

enum class FlatpakSpawnSandboxFlags : uint32_t {
    ShareDisplay = 1 << 0,
    ShareSound = 1 << 1,
    ShareGPU = 1 << 2,
    AllowDBus = 1 << 3,
    AllowA11y = 1 << 4,
};

bool isFlatpakSpawnUsable()
{
    ASSERT(isInsideFlatpak());
    static bool usable = [] {
        // Version 3 (flatpak 1.6.0) added sandbox flags, exposing paths by fd and exposing pids.
        // We need the latter to get the pid of the child from its IPC socket.
        bool usable = checkFlatpakPortalVersion(3) && flatpakPortalSupportsExposePids();
        if (!usable)
            g_warning("The Flatpak portal cannot spawn sandboxed processes. Sandboxed processes will be spawned without a sandbox.");
        return usable;
    }();
    return usable;
}

static bool canPossiblyExposePath(std::span<const char> path)
{
    // The child process's /app and /usr will both be identical to the parent process's.
    return !spanHasPrefix(path, "/app"_span) && !spanHasPrefix(path, "/usr"_span);
}

static UnixFileDescriptor openPath(const char* path)
{
    int fd;
    while ((fd = open(path, O_PATH | O_CLOEXEC | O_NOFOLLOW)) == -1 && errno == EINTR) { }
    return UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

// Paths are file system bytes and not necessarily UTF-8.
static UnixFileDescriptor openPathToExpose(const char* path, const char* flatpakID, const char* homeRealPath)
{
    auto fd = openPath(path);
    if (!fd) {
        g_warning("Failed to open %s to expose in the sandbox: %s", path, g_strerror(errno));
        return { };
    }

    // The portal resolves the fd to a path outside of our sandbox, where files in $HOME that
    // are bind mounted from ~/.var/app/$FLATPAK_ID only exist under the latter, as flatpak-spawn does.
    if (!homeRealPath)
        return fd;

    GUniquePtr<char> realPath(realpath(path, nullptr));
    if (!realPath)
        return fd;

    auto home = unsafeSpan(homeRealPath);
    auto real = unsafeSpan(realPath.get());
    if (!spanHasPrefix(real, home) || (real.size() > home.size() && real[home.size()] != '/'))
        return fd;

    GUniquePtr<char> varPath(g_build_filename(homeRealPath, ".var", "app", flatpakID, real.subspan(home.size()).data(), nullptr));
    auto varFD = openPath(varPath.get());
    struct stat buffer;
    struct stat varBuffer;
    if (varFD && !fstat(fd.value(), &buffer) && !fstat(varFD.value(), &varBuffer) && buffer.st_dev == varBuffer.st_dev && buffer.st_ino == varBuffer.st_ino)
        return varFD;
    return fd;
}

// Spawned processes by their pid in the portal's namespace, which SpawnExited reports.
static HashMap<uint32_t, WebCore::ProcessIdentifier>& spawnedProcesses()
{
    static NeverDestroyed<HashMap<uint32_t, WebCore::ProcessIdentifier>> processes;
    return processes;
}

static void spawnExited(GDBusConnection*, const char*, const char*, const char*, const char*, GVariant* parameters, gpointer)
{
    uint32_t pid;
    uint32_t waitStatus;
    g_variant_get(parameters, "(uu)", &pid, &waitStatus);
    auto identifier = spawnedProcesses().takeOptional(pid);
    if (!identifier)
        return;

    if (WIFEXITED(waitStatus)) {
        if (int status = WEXITSTATUS(waitStatus))
            g_warning("Sandboxed process %" PRIu64 " exited with status %d", identifier->toUInt64(), status);
        else
            RELEASE_LOG(Process, "Sandboxed process %" PRIu64 " exited", identifier->toUInt64());
        return;
    }

    if (WIFSIGNALED(waitStatus)) {
        int signal = WTERMSIG(waitStatus);
        // These are how ProcessLauncher terminates its children.
        if (signal == SIGKILL || signal == SIGTERM)
            RELEASE_LOG(Process, "Sandboxed process %" PRIu64 " was terminated by signal %d", identifier->toUInt64(), signal);
        else
            g_warning("Sandboxed process %" PRIu64 " was killed by signal %d (%s)%s", identifier->toUInt64(), signal, g_strsignal(signal), WCOREDUMP(waitStatus) ? ", core dumped" : "");
    }
}

void flatpakSpawn(const WebKit::ProcessLauncher::LaunchOptions& launchOptions, Vector<char*>& argv, std::span<const int> inheritedFDs)
{
    // When we are running inside of flatpak's sandbox we do not have permissions to use the same
    // bubblewrap sandbox we do outside but flatpak offers the ability to create new sandboxes
    // for us using its portal. Any failure closes the child's end of the IPC socket, which
    // ProcessLauncher reports as a failed launch.
    GUniqueOutPtr<GError> error;
    GRefPtr<GDBusConnection> sessionBus = adoptGRef(g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error.outPtr()));
    if (!sessionBus) {
        g_warning("Failed to spawn a sandboxed process, could not connect to the session bus: %s", error->message);
        return;
    }

    static bool subscribedToSpawnExited = false;
    if (!subscribedToSpawnExited) {
        g_dbus_connection_signal_subscribe(sessionBus.get(), "org.freedesktop.portal.Flatpak", "org.freedesktop.portal.Flatpak", "SpawnExited", "/org/freedesktop/portal/Flatpak",
            nullptr, G_DBUS_SIGNAL_FLAGS_NONE, spawnExited, nullptr, nullptr);
        subscribedToSpawnExited = true;
    }

    GRefPtr<GUnixFDList> fdList = adoptGRef(g_unix_fd_list_new());
    GVariantBuilder fdsBuilder;
    g_variant_builder_init(&fdsBuilder, G_VARIANT_TYPE("a{uh}"));
    auto inherit = [&](int fd) {
        if (int handle = g_unix_fd_list_append(fdList.get(), fd, nullptr); handle != -1)
            g_variant_builder_add(&fdsBuilder, "{uh}", fd, handle);
    };
    for (int fd : { STDIN_FILENO, STDOUT_FILENO, STDERR_FILENO })
        inherit(fd);
    for (int fd : inheritedFDs)
        inherit(fd);

    // We pass our full environment to the subprocess, but D-Bus strings must be UTF-8.
    GVariantBuilder envsBuilder;
    g_variant_builder_init(&envsBuilder, G_VARIANT_TYPE("a{ss}"));
    GUniquePtr<char*> names(g_listenv());
    for (auto* name : span(names)) {
        const char* value = g_getenv(name);
        if (!value)
            continue;
        if (!g_utf8_validate(name, -1, nullptr) || !g_utf8_validate(value, -1, nullptr)) {
            g_warning("Not passing %s=%s to a sandboxed process, it is not valid UTF-8", name, value);
            continue;
        }
        g_variant_builder_add(&envsBuilder, "{ss}", name, value);
    }

    OptionSet<FlatpakSpawnFlags> flags { FlatpakSpawnFlags::WatchBus, FlatpakSpawnFlags::ExposePids };
    GVariantBuilder optionsBuilder;
    g_variant_builder_init(&optionsBuilder, G_VARIANT_TYPE_VARDICT);
    GUniquePtr<char> workingDirectory;

    if (launchOptions.processType == ProcessLauncher::ProcessType::Web) {
        flags.add({ FlatpakSpawnFlags::Sandbox, FlatpakSpawnFlags::NoNetwork });
        // The current working directory might not be visible in the sub-sandbox.
        workingDirectory.reset(g_strdup("/"));

        // Note that allowing D-Bus only allows portals and $appid.Sandbox.* access.
        OptionSet<FlatpakSpawnSandboxFlags> sandboxFlags { FlatpakSpawnSandboxFlags::ShareGPU, FlatpakSpawnSandboxFlags::ShareDisplay,
            FlatpakSpawnSandboxFlags::ShareSound, FlatpakSpawnSandboxFlags::AllowA11y, FlatpakSpawnSandboxFlags::AllowDBus };
        g_variant_builder_add(&optionsBuilder, "{sv}", "sandbox-flags", g_variant_new_uint32(sandboxFlags.toRaw()));

        const char* flatpakID = g_getenv("FLATPAK_ID");
        GUniquePtr<char> homeRealPath(flatpakID ? realpath(g_get_home_dir(), nullptr) : nullptr);
        GVariantBuilder exposeBuilder;
        g_variant_builder_init(&exposeBuilder, G_VARIANT_TYPE("ah"));
        GVariantBuilder exposeReadOnlyBuilder;
        g_variant_builder_init(&exposeReadOnlyBuilder, G_VARIANT_TYPE("ah"));
        auto expose = [&](const char* path, SandboxPermission permission) {
            if (!canPossiblyExposePath(unsafeSpan(path)))
                return;
            if (auto fd = openPathToExpose(path, flatpakID, homeRealPath.get())) {
                if (int handle = g_unix_fd_list_append(fdList.get(), fd.value(), nullptr); handle != -1)
                    g_variant_builder_add(permission == SandboxPermission::ReadOnly ? &exposeReadOnlyBuilder : &exposeBuilder, "h", handle);
            }
        };

        // GST_DEBUG_FILE is a file and GST_DEBUG_DUMP_DOT_DIR might not exist when the application
        // starts, so we need write permissions for their parent directories.
        for (const char* variable : { "GST_DEBUG_FILE", "GST_DEBUG_DUMP_DOT_DIR" }) {
            if (const char* path = g_getenv(variable)) {
                GUniquePtr<char> parentDirectory(g_path_get_dirname(path));
                expose(parentDirectory.get(), SandboxPermission::ReadWrite);
            }
        }

        for (const auto& pathAndPermission : launchOptions.extraSandboxPaths)
            expose(pathAndPermission.key.legacyCStringPointer(), pathAndPermission.value);

        g_variant_builder_add(&optionsBuilder, "{sv}", "sandbox-expose-fd", g_variant_builder_end(&exposeBuilder));
        g_variant_builder_add(&optionsBuilder, "{sv}", "sandbox-expose-fd-ro", g_variant_builder_end(&exposeReadOnlyBuilder));

#if USE(ATSPI)
        if (checkFlatpakPortalVersion(7)) {
            auto busName = launchOptions.extraInitializationData.get<HashTranslatorASCIILiteral>("accessibilityBusName"_s).utf8();
            const char* busNameString = busName.legacyCStringPointer();
            g_variant_builder_add(&optionsBuilder, "{sv}", "sandbox-a11y-own-names", g_variant_new_strv(&busNameString, 1));
        }
#endif
    } else
        workingDirectory.reset(g_get_current_dir());

    g_dbus_connection_call_with_unix_fd_list(sessionBus.get(), "org.freedesktop.portal.Flatpak", "/org/freedesktop/portal/Flatpak", "org.freedesktop.portal.Flatpak", "Spawn",
        g_variant_new("(^ay^aay@a{uh}@a{ss}u@a{sv})", workingDirectory.get(), argv.span().data(), g_variant_builder_end(&fdsBuilder),
            g_variant_builder_end(&envsBuilder), flags.toRaw(), g_variant_builder_end(&optionsBuilder)),
        G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, fdList.get(), nullptr, [](GObject* connection, GAsyncResult* result, gpointer userData) {
            std::unique_ptr<WebCore::ProcessIdentifier> identifier(static_cast<WebCore::ProcessIdentifier*>(userData));
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> reply = adoptGRef(g_dbus_connection_call_with_unix_fd_list_finish(G_DBUS_CONNECTION(connection), nullptr, result, &error.outPtr()));
            if (!reply) {
                g_dbus_error_strip_remote_error(error.get());
                g_warning("The Flatpak portal failed to spawn sandboxed process %" PRIu64 ": %s", identifier->toUInt64(), error->message);
                return;
            }

            uint32_t pid;
            g_variant_get(reply.get(), "(u)", &pid);
            spawnedProcesses().set(pid, *identifier);
        }, makeUniqueWithoutFastMallocCheck<WebCore::ProcessIdentifier>(launchOptions.processIdentifier).release());
}

};

#endif // OS(LINUX)
