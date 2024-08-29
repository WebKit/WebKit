/*
 * Copyright (C) 2021 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/glib/Sandbox.h>

#include <glib.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>

namespace WTF {

bool isInsideFlatpak()
{
    static bool returnValue = g_file_test("/.flatpak-info", G_FILE_TEST_EXISTS);
    return returnValue;
}

#if ENABLE(BUBBLEWRAP_SANDBOX)
bool isInsideUnsupportedContainer()
{
    static bool inContainer = g_file_test("/run/.containerenv", G_FILE_TEST_EXISTS);
    static int supportedContainer = -1;

    // Being in a container does not mean sub-containers cannot work. It depends upon various details such as
    // docker vs podman, which permissions are given, is it privileged or unprivileged, and are unprivileged user namespaces enabled.
    // So this just does a basic test of if `bwrap` runs successfully.
    if (inContainer && supportedContainer == -1) {
        const char* bwrapArgs[] = {
            BWRAP_EXECUTABLE,
            "--ro-bind", "/", "/",
            "--proc", "/proc",
            "--dev", "/dev",
            "--unshare-all",
            "true",
            nullptr
        };
        int waitStatus = 0;
        gboolean spawnSucceeded = g_spawn_sync(nullptr, const_cast<char**>(bwrapArgs), nullptr,
            G_SPAWN_STDERR_TO_DEV_NULL, nullptr, nullptr, nullptr, nullptr, &waitStatus, nullptr);
        supportedContainer = spawnSucceeded && g_spawn_check_exit_status(waitStatus, nullptr);
        if (!supportedContainer)
            WTFLogAlways("Bubblewrap does not work inside of this container, sandboxing will be disabled.");
    }

    return inContainer && !supportedContainer;
}
#endif

bool isInsideSnap()
{
    // The "SNAP" environment variable is not unlikely to be set for/by something other
    // than Snap, so check a couple of additional variables to avoid false positives.
    // See: https://snapcraft.io/docs/environment-variables
    static bool returnValue = g_getenv("SNAP") && g_getenv("SNAP_NAME") && g_getenv("SNAP_REVISION");
    return returnValue;
}

bool shouldUseBubblewrap()
{
#if ENABLE(BUBBLEWRAP_SANDBOX)
    return !isInsideFlatpak() && !isInsideSnap() && !isInsideUnsupportedContainer();
#else
    return false;
#endif
}

bool shouldUsePortal()
{
    static bool returnValue = []() -> bool {
        const char* usePortal = isInsideFlatpak() || isInsideSnap() ? "1" : g_getenv("WEBKIT_USE_PORTAL");
        return usePortal && usePortal[0] != '0';
    }();
    return returnValue;
}

const CString& sandboxedUserRuntimeDirectory()
{
    static LazyNeverDestroyed<CString> userRuntimeDirectory;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
#if PLATFORM(GTK)
        static constexpr ASCIILiteral baseDirectory = "webkitgtk"_s;
#elif PLATFORM(WPE)
        static constexpr ASCIILiteral baseDirectory = "wpe"_s;
#endif
        userRuntimeDirectory.construct(FileSystem::pathByAppendingComponent(FileSystem::stringFromFileSystemRepresentation(g_get_user_runtime_dir()), baseDirectory).utf8());
    });
    return userRuntimeDirectory.get();
}

} // namespace WTF
