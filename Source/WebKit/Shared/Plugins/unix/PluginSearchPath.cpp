/*
 * Copyright (C) 2015 Igalia S.L.
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
#include "PluginSearchPath.h"

#include <wtf/FileSystem.h>

namespace WebKit {
using namespace WebCore;

Vector<String> pluginsDirectories()
{
    Vector<String> result;

#if ENABLE(NETSCAPE_PLUGIN_API)
    String mozillaPaths(getenv("MOZ_PLUGIN_PATH"));
    if (!mozillaPaths.isEmpty()) {
        Vector<String> paths = mozillaPaths.split(':');
        result.appendVector(paths);
    }

    String mozillaHome(getenv("MOZILLA_HOME"));
    if (!mozillaHome.isEmpty())
        result.append(mozillaHome + "/plugins");

    result.append(FileSystem::homeDirectoryPath() + "/.mozilla/plugins");
    result.append(FileSystem::homeDirectoryPath() + "/.netscape/plugins");
    result.append("/usr/lib/browser/plugins");
    result.append("/usr/local/lib/mozilla/plugins");
    result.append("/usr/lib/firefox/plugins");
    result.append("/usr/lib64/browser-plugins");
    result.append("/usr/lib/browser-plugins");
    result.append("/usr/lib/mozilla/plugins");
    result.append("/usr/local/netscape/plugins");
    result.append("/opt/mozilla/plugins");
    result.append("/opt/mozilla/lib/plugins");
    result.append("/opt/netscape/plugins");
    result.append("/opt/netscape/communicator/plugins");
    result.append("/usr/lib/netscape/plugins");
    result.append("/usr/lib/netscape/plugins-libc5");
    result.append("/usr/lib/netscape/plugins-libc6");
    result.append("/usr/lib64/netscape/plugins");
    result.append("/usr/lib64/mozilla/plugins");
    result.append("/usr/lib/nsbrowser/plugins");
    result.append("/usr/lib64/nsbrowser/plugins");
#endif

    return result;
}

} // namespace WebKit
