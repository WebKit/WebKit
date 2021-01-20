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

namespace WebKit {

GRefPtr<GSubprocess> flatpakSpawn(GSubprocessLauncher* launcher, const WebKit::ProcessLauncher::LaunchOptions& launchOptions, char** argv, int childProcessSocket, GError** error)
{
    ASSERT(launcher);

    // When we are running inside of flatpak's sandbox we do not have permissions to use the same
    // bubblewrap sandbox we do outside but flatpak offers the ability to create new sandboxes
    // for us using flatpak-spawn.

    GUniquePtr<gchar> childProcessSocketArg(g_strdup_printf("--forward-fd=%d", childProcessSocket));
    Vector<CString> flatpakArgs = {
        "flatpak-spawn",
        childProcessSocketArg.get(),
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

        for (const auto& pathAndPermission : launchOptions.extraWebProcessSandboxPaths) {
            const char* formatString = pathAndPermission.value == SandboxPermission::ReadOnly ? "--sandbox-expose-path-ro=%s": "--sandbox-expose-path=%s";
            GUniquePtr<gchar> pathArg(g_strdup_printf(formatString, pathAndPermission.key.data()));
            flatpakArgs.append(pathArg.get());
        }
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
