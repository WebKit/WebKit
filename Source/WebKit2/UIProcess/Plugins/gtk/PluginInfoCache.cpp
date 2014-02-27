/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "PluginInfoCache.h"

#if ENABLE(NETSCAPE_PLUGIN_API)

#include "NetscapePluginModule.h"
#include <WebCore/FileSystem.h>
#include <wtf/text/CString.h>

namespace WebKit {

static const unsigned gSchemaVersion = 1;

PluginInfoCache& PluginInfoCache::shared()
{
    static NeverDestroyed<PluginInfoCache> pluginInfoCache;
    return pluginInfoCache;
}

PluginInfoCache::PluginInfoCache()
    : m_cacheFile(g_key_file_new())
    , m_cachePath(g_build_filename(g_get_user_cache_dir(), "webkitgtk", "plugins", nullptr))
    , m_saveToFileIdleId(0)
{
    g_key_file_load_from_file(m_cacheFile.get(), m_cachePath.get(), G_KEY_FILE_NONE, nullptr);

    if (g_key_file_has_group(m_cacheFile.get(), "schema")) {
        unsigned schemaVersion = static_cast<unsigned>(g_key_file_get_integer(m_cacheFile.get(), "schema", "version", nullptr));
        if (schemaVersion == gSchemaVersion)
            return;

        // Cache file using an old schema, create a new empty file.
        m_cacheFile.reset(g_key_file_new());
    }

    g_key_file_set_integer(m_cacheFile.get(), "schema", "version", static_cast<unsigned>(gSchemaVersion));
}

PluginInfoCache::~PluginInfoCache()
{
    if (m_saveToFileIdleId) {
        g_source_remove(m_saveToFileIdleId);
        saveToFile();
    }
}

gboolean PluginInfoCache::saveToFileIdleCallback(PluginInfoCache* cache)
{
    cache->saveToFile();
    return FALSE;
}

void PluginInfoCache::saveToFile()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_saveToFileIdleId = 0;

    gsize dataLength;
    GUniquePtr<char> data(g_key_file_to_data(m_cacheFile.get(), &dataLength, nullptr));
    if (!data)
        return;

    g_file_set_contents(m_cachePath.get(), data.get(), dataLength, nullptr);
}

bool PluginInfoCache::getPluginInfo(const String& pluginPath, PluginModuleInfo& plugin)
{
    CString pluginGroup = pluginPath.utf8();
    if (!g_key_file_has_group(m_cacheFile.get(), pluginGroup.data()))
        return false;

    time_t lastModified;
    if (!WebCore::getFileModificationTime(pluginPath, lastModified))
        return false;
    time_t cachedLastModified = static_cast<time_t>(g_key_file_get_uint64(m_cacheFile.get(), pluginGroup.data(), "mtime", nullptr));
    if (lastModified != cachedLastModified)
        return false;

    plugin.path = pluginPath;
    plugin.info.file = WebCore::pathGetFileName(pluginPath);

    GUniquePtr<char> stringValue(g_key_file_get_string(m_cacheFile.get(), pluginGroup.data(), "name", nullptr));
    plugin.info.name = String::fromUTF8(stringValue.get());

    stringValue.reset(g_key_file_get_string(m_cacheFile.get(), pluginGroup.data(), "description", nullptr));
    plugin.info.desc = String::fromUTF8(stringValue.get());

    stringValue.reset(g_key_file_get_string(m_cacheFile.get(), pluginGroup.data(), "mime-description", nullptr));
    NetscapePluginModule::parseMIMEDescription(String::fromUTF8(stringValue.get()), plugin.info.mimes);

    return true;
}

void PluginInfoCache::updatePluginInfo(const String& pluginPath, const PluginModuleInfo& plugin)
{
    time_t lastModified;
    if (!WebCore::getFileModificationTime(pluginPath, lastModified))
        return;

    CString pluginGroup = pluginPath.utf8();
    g_key_file_set_uint64(m_cacheFile.get(), pluginGroup.data(), "mtime", static_cast<guint64>(lastModified));
    g_key_file_set_string(m_cacheFile.get(), pluginGroup.data(), "name", plugin.info.name.utf8().data());
    g_key_file_set_string(m_cacheFile.get(), pluginGroup.data(), "description", plugin.info.desc.utf8().data());

    String mimeDescription = NetscapePluginModule::buildMIMEDescription(plugin.info.mimes);
    g_key_file_set_string(m_cacheFile.get(), pluginGroup.data(), "mime-description", mimeDescription.utf8().data());

    // Save the cache file in an idle to make sure it happens in the main thread and
    // it's done only once when this is called multiple times in a very short time.
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_saveToFileIdleId)
        return;

    m_saveToFileIdleId = g_idle_add(reinterpret_cast<GSourceFunc>(PluginInfoCache::saveToFileIdleCallback), this);
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
