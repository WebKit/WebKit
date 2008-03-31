/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
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
#include "PluginDatabase.h"

#include "CString.h"
#include "PluginPackage.h"

namespace WebCore {

PluginSet PluginDatabase::getPluginsInPaths() const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;
    PluginSet plugins;

    Vector<String>::const_iterator end = m_pluginPaths.end();
    for (Vector<String>::const_iterator it = m_pluginPaths.begin(); it != end; ++it) {
        GDir* dir = g_dir_open((it->utf8()).data(), 0, 0);
        if (!dir)
            continue;

        while (const char* name = g_dir_read_name(dir)) {
            if (!g_str_has_suffix(name, "." G_MODULE_SUFFIX))
                continue;

            gchar* filename = g_build_filename((it->utf8()).data(), name, 0);
            RefPtr<PluginPackage> pluginPackage = PluginPackage::createPackage(filename, time(0));
            if (pluginPackage)
                plugins.add(pluginPackage);
            g_free(filename);
        }
        g_dir_close(dir);
    }

    return plugins;
}

Vector<String> PluginDatabase::defaultPluginPaths()
{
    Vector<String> paths;
    gchar* path;

#if defined(GDK_WINDOWING_X11)
    path = g_build_filename(g_get_home_dir(), ".mozilla", "plugins", 0);
    paths.append(path);
    g_free(path);

    const gchar* mozPath = g_getenv("MOZ_PLUGIN_PATH");
    if (mozPath) {
        gchar** pluginPaths = g_strsplit(mozPath, ":", -1);

        for (gint i = 0; pluginPaths[i]; i++)
            paths.append(pluginPaths[i]);

        g_strfreev(pluginPaths);
    }

    path = g_build_filename(G_DIR_SEPARATOR_S "usr", "lib", "browser", "plugins", 0);
    paths.append(path);
    g_free(path);
    path = g_build_filename(G_DIR_SEPARATOR_S "usr", "local", "lib", "mozilla", "plugins", 0);
    paths.append(path);
    g_free (path);
    path = g_build_filename(G_DIR_SEPARATOR_S "usr", "lib", "mozilla", "plugins", 0);
    paths.append(path);
    g_free (path);
#elif defined(GDK_WINDOWING_WIN32)
    path = g_build_filename(g_get_home_dir(), "Application Data", "Mozilla", "plugins", 0);
    paths.append(path);
    g_free(path);
#endif

    return paths;
}

bool PluginDatabase::isPreferredPluginPath(const String& path)
{
    return false;
}

}
