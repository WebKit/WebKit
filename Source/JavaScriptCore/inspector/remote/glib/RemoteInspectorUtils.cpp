/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RemoteInspectorUtils.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <gio/gio.h>
#include <mutex>
#include <wtf/SHA1.h>
#include <wtf/glib/GUniquePtr.h>

#define INSPECTOR_BACKEND_COMMANDS_PATH "/org/webkit/inspector/UserInterface/Protocol/InspectorBackendCommands.js"

namespace Inspector {

GRefPtr<GBytes> backendCommands()
{
#if PLATFORM(WPE)
    static std::once_flag flag;
    std::call_once(flag, [] {
        const char* libDir = PKGLIBDIR;
#if ENABLE(DEVELOPER_MODE)
        // Probably no need for a specific env var here. Assume the inspector resources.so file is
        // in the same directory as the injected bundle lib, for developer builds.
        const char* path = g_getenv("WEBKIT_INJECTED_BUNDLE_PATH");
        if (path && g_file_test(path, G_FILE_TEST_IS_DIR))
            libDir = path;
#endif
        GUniquePtr<char> bundleFilename(g_build_filename(libDir, "libWPEWebInspectorResources.so", nullptr));
        GModule* resourcesModule = g_module_open(bundleFilename.get(), G_MODULE_BIND_LAZY);
        if (!resourcesModule) {
            WTFLogAlways("Error loading libWPEWebInspectorResources.so: %s", g_module_error());
            return;
        }

        g_module_make_resident(resourcesModule);
    });
#endif
    GRefPtr<GBytes> bytes = adoptGRef(g_resources_lookup_data(INSPECTOR_BACKEND_COMMANDS_PATH, G_RESOURCE_LOOKUP_FLAGS_NONE, nullptr));
    ASSERT(bytes);
    return bytes;
}

const CString& backendCommandsHash()
{
    static CString hexDigest;
    if (hexDigest.isNull()) {
        auto bytes = backendCommands();
        size_t dataSize;
        gconstpointer data = g_bytes_get_data(bytes.get(), &dataSize);
        ASSERT(dataSize);
        SHA1 sha1;
        sha1.addBytes(static_cast<const uint8_t*>(data), dataSize);
        hexDigest = sha1.computeHexDigest();
    }
    return hexDigest;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
