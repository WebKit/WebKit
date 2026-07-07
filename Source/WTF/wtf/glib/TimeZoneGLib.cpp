/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/TimeZone.h>

#include <gio/gio.h>
#include <mutex>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/FilePathWatcher.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTF {

// /etc/localtime is the canonical location of the host time-zone binary on
// glibc/musl-based systems (localtime(5):
// https://man7.org/linux/man-pages/man5/localtime.5.html). The time-conversion
// functions implicitly call tzset(), which re-reads this file, so any change
// that takes effect for libc consumers necessarily mutates the path we watch
// here. See tzset(3):
//   https://man7.org/linux/man-pages/man3/tzset.3.html
// FilePathWatcher catches symlink swap, replacement, and in-place rewrite —
// see the CHANGES_DONE_HINT / CREATED filter in FilePathWatcher.cpp.
static std::unique_ptr<FilePathWatcher>& localtimeWatcher()
{
    static NeverDestroyed<std::unique_ptr<FilePathWatcher>> watcher;
    return watcher.get();
}

// Listener for the "Timezone" property of org.freedesktop.timedate1, which
// systemd-timedated (and a few API-compatible reimplementations) emit on
// PropertiesChanged whenever timedatectl set-timezone (or any client of the
// same D-Bus method) succeeds. Spec:
//   https://www.freedesktop.org/software/systemd/man/latest/org.freedesktop.timedate1.html
//   https://github.com/systemd/systemd/blob/main/src/timedate/timedated.c
//
// Coverage:
//   * Caught by the file watcher AND this listener: any tool that changes the
//     TZ via timedatectl / GNOME / KDE / SetTimezone() — these always go
//     through timedated, which rewrites /etc/localtime.
//   * Caught by the file watcher only: a direct symlink swap of /etc/localtime
//     by a privileged process that bypasses timedated.
//   * Caught by this listener only: a TZ change inside a container/sandbox
//     where /etc/localtime is bind-mounted from the host (so inotify on the
//     guest path never fires) but the system bus is forwarded.
//   * Not caught by either: a process-local TZ override applied to the
//     calling process via setenv("TZ", ...) — there is no kernel notifier
//     for environment changes, and tzset(3) defines TZ as a per-process
//     override (https://man7.org/linux/man-pages/man3/tzset.3.html). Callers
//     that need that path should call timeZoneDidChange() themselves (this is
//     what setHostTimeZoneForTesting() does).
static GRefPtr<GDBusProxy>& timedate1Proxy()
{
    static NeverDestroyed<GRefPtr<GDBusProxy>> proxy;
    return proxy.get();
}

static void timedate1PropertiesChanged(GDBusProxy*, GVariant* changedProperties, char** invalidatedProperties, gpointer)
{
    // "&s" points into changedProperties without allocating; we only need to
    // know whether Timezone is present, not its value.
    const char* timeZone = nullptr;
    if (g_variant_lookup(changedProperties, "Timezone", "&s", &timeZone))
        timeZoneDidChange();
    else if (invalidatedProperties && g_strv_contains(invalidatedProperties, "Timezone"))
        timeZoneDidChange();
}

static void onTimedate1ProxyReady(GObject*, GAsyncResult* result, gpointer)
{
    // g_dbus_proxy_new_for_bus_finish returns null if the system bus is
    // unreachable (e.g. sandboxed without bus forwarding) or proxy setup
    // failed; in that case we fall back to the file watcher.
    GUniqueOutPtr<GError> error;
    GRefPtr<GDBusProxy> proxy = adoptGRef(g_dbus_proxy_new_for_bus_finish(result, &error.outPtr()));
    if (!proxy) {
        g_debug("TimeZone: timedate1 proxy unavailable, relying on the /etc/localtime watcher: %s", error.get() ? error->message : "unknown error");
        return;
    }

    timedate1Proxy() = proxy;
    g_signal_connect(proxy.get(), "g-properties-changed", G_CALLBACK(timedate1PropertiesChanged), nullptr);
}

void listenForTimeZoneChangeNotifications()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        // Both the file monitor and the D-Bus proxy attach to the GMainContext
        // that is thread-default when this first runs (the first DateCache
        // construction drives it), so this must be reached on the main thread
        // for its run loop to dispatch the notifications.
        localtimeWatcher() = makeUnique<FilePathWatcher>("/etc/localtime"_s, [] {
            timeZoneDidChange();
        });
        // Async proxy construction so we don't block on timedated being
        // present; DO_NOT_AUTO_START avoids spuriously activating it just to
        // observe its absence. If the service has no owner now, the proxy's
        // NameOwnerChanged subscription will pick it up later.
        g_dbus_proxy_new_for_bus(G_BUS_TYPE_SYSTEM,
            G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
            nullptr,
            "org.freedesktop.timedate1",
            "/org/freedesktop/timedate1",
            "org.freedesktop.timedate1",
            nullptr, onTimedate1ProxyReady, nullptr);
    });
}

} // namespace WTF
