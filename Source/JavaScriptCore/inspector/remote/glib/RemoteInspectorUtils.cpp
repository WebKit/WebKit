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
#include <wtf/glib/GSpanExtras.h>

#define INSPECTOR_BACKEND_COMMANDS_PATH "/org/webkit/inspector/UserInterface/Protocol/InspectorBackendCommands.js"

namespace Inspector {

GRefPtr<GBytes> backendCommands()
{
#if PLATFORM(WPE)
    static std::once_flag flag;
    std::call_once(flag, [] {
        const char* dataDir = PKGDATADIR;
        GUniqueOutPtr<GError> error;

        const char* path = g_getenv("WEBKIT_INSPECTOR_RESOURCES_PATH");
        if (path && g_file_test(path, G_FILE_TEST_IS_DIR))
            dataDir = path;

        GUniquePtr<char> gResourceFilename(g_build_filename(dataDir, "inspector.gresource", nullptr));
        GRefPtr<GResource> gresource = adoptGRef(g_resource_load(gResourceFilename.get(), &error.outPtr()));
        if (!gresource) {
            g_error("Error loading inspector.gresource: %s", error->message);
        }
        g_resources_register(gresource.get());
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
        auto bytesSpan = span(bytes);
        ASSERT(bytesSpan.size());
        SHA1 sha1;
        sha1.addBytes(bytesSpan);
        hexDigest = sha1.computeHexDigest();
    }
    return hexDigest;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
