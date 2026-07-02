/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GResources.h"

#if USE(GLIB)

#include <mutex>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTF {

#if PLATFORM(WPE)

void registerInspectorResourceIfNeeded()
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const char* dataDir = PKGDATADIR;
        GUniqueOutPtr<GError> error;

        const char* path = g_getenv("WEBKIT_INSPECTOR_RESOURCES_PATH");
        if (path && g_file_test(path, G_FILE_TEST_IS_DIR))
            dataDir = path;

        GUniquePtr<char> gResourceFilename(g_build_filename(dataDir, "inspector.gresource", nullptr));
        GRefPtr<GResource> gresource = adoptGRef(g_resource_load(gResourceFilename.get(), &error.outPtr()));
        if (!gresource) {
            g_error("Error loading inspector.gresource: %s", error->message);
            return;
        }
        g_resources_register(gresource.get());
    });
}

#endif // PLATFORM(WPE)

} // namespace WTF

#endif // USE(GLIB)
