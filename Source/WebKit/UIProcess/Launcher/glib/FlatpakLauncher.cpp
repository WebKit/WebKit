/*
 * Copyright (C) 2020 Igalia S.L.
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

#include <gio/gio.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/Sandbox.h>

namespace WebKit {

#if OS(LINUX)
static bool isFlatpakSpawnUsable()
{
    ASSERT(isInsideFlatpak());
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

GRefPtr<GSubprocess> flatpakSpawn(GSubprocessLauncher* launcher, const WebKit::ProcessLauncher::LaunchOptions& launchOptions, char** argv, int childProcessSocket, int pidSocket, GError** error)
{
    ASSERT(launcher);

    if (!isFlatpakSpawnUsable())
        g_error("Cannot spawn WebKit auxiliary process because flatpak-spawn failed sanity check");

    // When we are running inside of flatpak's sandbox we do not have permissions to use the same
    // bubblewrap sandbox we do outside but flatpak offers the ability to create new sandboxes
    // for us using flatpak-spawn.

    GUniquePtr<char> childProcessSocketArg(g_strdup_printf("--forward-fd=%d", childProcessSocket));
    GUniquePtr<char> pidSocketArg(g_strdup_printf("--forward-fd=%d", pidSocket));
    Vector<CString> flatpakArgs = {
        "flatpak-spawn",
        childProcessSocketArg.get(),
        pidSocketArg.get(),
        "--expose-pids",
        "--watch-bus"
    };

    if (launchOptions.processType == ProcessLauncher::ProcessType::Web) {
        flatpakArgs.appendVector(Vector<CString>({
            "--sandbox",
            "--no-network",
            "--sandbox-flag=share-gpu",
            "--sandbox-flag=share-display",
            "--sandbox-flag=share-sound",
            "--sandbox-flag=allow-a11y",
            "--sandbox-flag=allow-dbus", // Note that this only allows portals and $appid.Sandbox.* access
        }));

        for (const auto& pathAndPermission : launchOptions.extraSandboxPaths) {
            const char* formatString = pathAndPermission.value == SandboxPermission::ReadOnly ? "--sandbox-expose-path-ro=%s": "--sandbox-expose-path=%s";
            GUniquePtr<gchar> pathArg(g_strdup_printf(formatString, pathAndPermission.key.data()));
            flatpakArgs.append(pathArg.get());
        }
    }

    // We need to pass our full environment to the subprocess.
    GUniquePtr<char*> environ(g_get_environ());
    for (char** variable = environ.get(); variable && *variable; variable++) {
        GUniquePtr<char> arg(g_strconcat("--env=", *variable, nullptr));
        flatpakArgs.append(arg.get());
    }

    char** newArgv = g_newa(char*, g_strv_length(argv) + flatpakArgs.size() + 1);
    size_t i = 0;

    for (const auto& arg : flatpakArgs)
        newArgv[i++] = const_cast<char*>(arg.data());
    for (size_t x = 0; argv[x]; x++)
        newArgv[i++] = argv[x];
    newArgv[i++] = nullptr;

    return adoptGRef(g_subprocess_launcher_spawnv(launcher, newArgv, error));
}

};

#endif // OS(LINUX)
