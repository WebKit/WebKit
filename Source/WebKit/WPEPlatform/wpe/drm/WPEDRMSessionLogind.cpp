/*
 * Copyright (C) 2023 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEDRMSessionLogind.h"

#if ENABLE(JOURNALD_LOG)
#include <errno.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <systemd/sd-login.h>
#include <unistd.h>
#include <wtf/Seconds.h>

namespace WPE {

namespace DRM {

std::unique_ptr<Session> SessionLogind::create()
{
    GUniqueOutPtr<char> session;
    int retval = sd_pid_get_session(getpid(), &session.outPtr());
    if (retval < 0) {
        if (retval != -ENODATA)
            return nullptr;

        // Not inside a systemd session, look if there is a suitable one.
        if (sd_uid_get_display(getuid(), &session.outPtr()) < 0)
            return nullptr;
    }

    GUniqueOutPtr<char> seat;
    if (sd_session_get_seat(session.get(), &seat.outPtr()) < 0)
        return nullptr;

    GUniquePtr<char> path(g_strdup_printf("/org/freedesktop/login1/session/%s", session.get()));
    GRefPtr<GDBusProxy> sessionProxy = adoptGRef(g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM, G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START, nullptr, "org.freedesktop.login1", path.get(), "org.freedesktop.login1.Session", nullptr, nullptr));
    return makeUnique<SessionLogind>(WTFMove(sessionProxy), GUniquePtr<char>(seat.release()));
}

SessionLogind::SessionLogind(GRefPtr<GDBusProxy>&& sessionProxy, GUniquePtr<char>&& seatID)
    : m_sessionProxy(WTFMove(sessionProxy))
    , m_seatID(WTFMove(seatID))
{
    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(m_sessionProxy.get(), "TakeControl", g_variant_new("(b)", FALSE),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error.outPtr()));
    if (!result) {
        g_warning("Failed to take control of session: %s", error->message);
        return;
    }

    m_inControl = true;
    g_dbus_proxy_call(m_sessionProxy.get(), "Activate", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
}

SessionLogind::~SessionLogind()
{
    if (!m_inControl)
        return;

    g_dbus_proxy_call(m_sessionProxy.get(), "ReleaseControl", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, nullptr, nullptr);
}

int SessionLogind::openDevice(const char* path, int flags)
{
    if (!m_inControl)
        return Session::openDevice(path, flags);

    struct stat st;
    int retval = stat(path, &st);
    if (retval < 0)
        return -1;

    if (!S_ISCHR(st.st_mode)) {
        errno = ENODEV;
        return -1;
    }

    GRefPtr<GUnixFDList> fdList;
    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_with_unix_fd_list_sync(m_sessionProxy.get(), "TakeDevice", g_variant_new("(uu)", major(st.st_rdev), minor(st.st_rdev)),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &fdList.outPtr(), nullptr, &error.outPtr()));
    if (!result) {
        g_warning("Session failed to take device %s: %s", path, error->message);
        errno = ENODEV;
        return -1;
    }

    int handler;
    g_variant_get(result.get(), "(hb)", &handler, nullptr);
    return g_unix_fd_list_get(fdList.get(), handler, nullptr);
}

int SessionLogind::closeDevice(int deviceID)
{
    if (!m_inControl)
        return Session::closeDevice(deviceID);

    struct stat st;
    int retval = fstat(deviceID, &st);
    if (retval < 0)
        return -1;

    if (!S_ISCHR(st.st_mode)) {
        errno = ENODEV;
        return -1;
    }

    GUniqueOutPtr<GError> error;
    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(m_sessionProxy.get(), "ReleaseDevice", g_variant_new("(uu)", major(st.st_rdev), minor(st.st_rdev)),
        G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error.outPtr()));
    if (!result) {
        g_warning("Session failed to release device %d: %s", deviceID, error->message);
        errno = ENODEV;
        return -1;
    }

    return 0;
}

} // namespace DRM

} // namespace WPE

#endif // ENABLE(JOURNALD_LOG)
