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
#include <WebCore/PlatformDisplay.h>
#include <wtf/text/CString.h>

namespace WebKit {

static const unsigned gSchemaVersion = 3;

PluginInfoCache& PluginInfoCache::singleton()
{
    static NeverDestroyed<PluginInfoCache> pluginInfoCache;
    return pluginInfoCache;
}

static inline const char* cacheFilenameForCurrentDisplay()
{
#if PLATFORM(X11)
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::X11)
        return "plugins-x11";
#endif
#if PLATFORM(WAYLAND)
    if (WebCore::PlatformDisplay::sharedDisplay().type() == WebCore::PlatformDisplay::Type::Wayland)
        return "plugins-wayland";
#endif

    ASSERT_NOT_REACHED();
    return "plugins";
}

PluginInfoCache::PluginInfoCache()
    : m_cacheFile(g_key_file_new())
    , m_saveToFileIdle(RunLoop::main(), this, &PluginInfoCache::saveToFile)
    , m_readOnlyMode(false)
{
    m_saveToFileIdle.setPriority(G_PRIORITY_DEFAULT_IDLE);

    GUniquePtr<char> cacheDirectory(g_build_filename(g_get_user_cache_dir(), "webkitgtk", nullptr));
    if (WebCore::FileSystem::makeAllDirectories(cacheDirectory.get())) {
        // Delete old cache file.
        GUniquePtr<char> oldCachePath(g_build_filename(cacheDirectory.get(), "plugins", nullptr));
        WebCore::FileSystem::deleteFile(WebCore::FileSystem::stringFromFileSystemRepresentation(oldCachePath.get()));

        m_cachePath.reset(g_build_filename(cacheDirectory.get(), cacheFilenameForCurrentDisplay(), nullptr));
        g_key_file_load_from_file(m_cacheFile.get(), m_cachePath.get(), G_KEY_FILE_NONE, nullptr);
    }

    if (g_key_file_has_group(m_cacheFile.get(), "schema")) {
        unsigned schemaVersion = static_cast<unsigned>(g_key_file_get_integer(m_cacheFile.get(), "schema", "version", nullptr));
        if (schemaVersion < gSchemaVersion) {
            // Cache file using an old schema, create a new empty file.
            m_cacheFile.reset(g_key_file_new());
        } else if (schemaVersion > gSchemaVersion) {
            // Cache file using a newer schema, use the cache in read only mode.
            m_readOnlyMode = true;
        } else {
            // Same schema version, we don't need to update it.
            return;
        }
    }

    g_key_file_set_integer(m_cacheFile.get(), "schema", "version", static_cast<unsigned>(gSchemaVersion));
}

PluginInfoCache::~PluginInfoCache()
{
}

void PluginInfoCache::saveToFile()
{
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

    auto lastModifiedTime = WebCore::FileSystem::getFileModificationTime(pluginPath);
    if (!lastModifiedTime)
        return false;
    time_t cachedLastModified = static_cast<time_t>(g_key_file_get_uint64(m_cacheFile.get(), pluginGroup.data(), "mtime", nullptr));
    if (lastModifiedTime->secondsSinceEpoch().secondsAs<time_t>() != cachedLastModified)
        return false;

    plugin.path = pluginPath;
    plugin.info.file = WebCore::FileSystem::pathGetFileName(pluginPath);

    GUniquePtr<char> stringValue(g_key_file_get_string(m_cacheFile.get(), pluginGroup.data(), "name", nullptr));
    plugin.info.name = String::fromUTF8(stringValue.get());

    stringValue.reset(g_key_file_get_string(m_cacheFile.get(), pluginGroup.data(), "description", nullptr));
    plugin.info.desc = String::fromUTF8(stringValue.get());

#if PLUGIN_ARCHITECTURE(UNIX)
    stringValue.reset(g_key_file_get_string(m_cacheFile.get(), pluginGroup.data(), "mime-description", nullptr));
    NetscapePluginModule::parseMIMEDescription(String::fromUTF8(stringValue.get()), plugin.info.mimes);
#endif

    plugin.requiresGtk2 = g_key_file_get_boolean(m_cacheFile.get(), pluginGroup.data(), "requires-gtk2", nullptr);

    return true;
}

void PluginInfoCache::updatePluginInfo(const String& pluginPath, const PluginModuleInfo& plugin)
{
    auto lastModifiedTime = WebCore::FileSystem::getFileModificationTime(pluginPath);
    if (!lastModifiedTime)
        return;

    CString pluginGroup = pluginPath.utf8();
    g_key_file_set_uint64(m_cacheFile.get(), pluginGroup.data(), "mtime", lastModifiedTime->secondsSinceEpoch().secondsAs<guint64>());
    g_key_file_set_string(m_cacheFile.get(), pluginGroup.data(), "name", plugin.info.name.utf8().data());
    g_key_file_set_string(m_cacheFile.get(), pluginGroup.data(), "description", plugin.info.desc.utf8().data());

#if PLUGIN_ARCHITECTURE(UNIX)
    String mimeDescription = NetscapePluginModule::buildMIMEDescription(plugin.info.mimes);
    g_key_file_set_string(m_cacheFile.get(), pluginGroup.data(), "mime-description", mimeDescription.utf8().data());
#endif

    g_key_file_set_boolean(m_cacheFile.get(), pluginGroup.data(), "requires-gtk2", plugin.requiresGtk2);

    if (m_cachePath && !m_readOnlyMode) {
        // Save the cache file in an idle to make sure it happens in the main thread and
        // it's done only once when this is called multiple times in a very short time.
        if (m_saveToFileIdle.isActive())
            return;

        m_saveToFileIdle.startOneShot(0_s);
    }
}

} // namespace WebKit

#endif // ENABLE(NETSCAPE_PLUGIN_API)
