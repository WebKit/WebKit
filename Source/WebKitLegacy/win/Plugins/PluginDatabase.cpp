/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd.  All rights reserved.
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

#include "PluginDatabase.h"

#include "PluginPackage.h"
#include <WebCore/Frame.h>
#include <stdlib.h>
#include <wtf/URL.h>
#include <wtf/text/CString.h>

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
#include <wtf/FileSystem.h>
#endif

namespace WebCore {

typedef HashMap<String, RefPtr<PluginPackage> > PluginPackageByNameMap;

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)

static const size_t maximumPersistentPluginMetadataCacheSize = 32768;

static bool gPersistentPluginMetadataCacheIsEnabled;

String& persistentPluginMetadataCachePath()
{
    static NeverDestroyed<String> cachePath;
    return cachePath;
}

#endif

PluginDatabase::PluginDatabase()
#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
    : m_persistentMetadataCacheIsLoaded(false)
#endif
{
}

PluginDatabase* PluginDatabase::installedPlugins(bool populate)
{
    static PluginDatabase* plugins = 0;

    if (!plugins) {
        plugins = new PluginDatabase;

        if (populate) {
            plugins->setPluginDirectories(PluginDatabase::defaultPluginDirectories());
            plugins->refresh();
        }
    }

    return plugins;
}

bool PluginDatabase::isMIMETypeRegistered(const String& mimeType)
{
    if (mimeType.isNull())
        return false;
    if (m_registeredMIMETypes.contains(mimeType))
        return true;
    // No plugin was found, try refreshing the database and searching again
    return (refresh() && m_registeredMIMETypes.contains(mimeType));
}

void PluginDatabase::addExtraPluginDirectory(const String& directory)
{
    m_pluginDirectories.append(directory);
    refresh();
}

bool PluginDatabase::refresh()
{
#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
    if (!m_persistentMetadataCacheIsLoaded)
        loadPersistentMetadataCache();
#endif
    bool pluginSetChanged = false;

    if (!m_plugins.isEmpty()) {
        PluginSet pluginsToUnload;
        getDeletedPlugins(pluginsToUnload);

        // Unload plugins
        auto end = pluginsToUnload.end();
        for (auto it = pluginsToUnload.begin(); it != end; ++it)
            remove(it->get());

        pluginSetChanged = !pluginsToUnload.isEmpty();
    }

    HashSet<String> paths;
    getPluginPathsInDirectories(paths);

    HashMap<String, time_t> pathsWithTimes;

    // We should only skip unchanged files if we didn't remove any plugins above. If we did remove
    // any plugins, we need to look at every plugin file so that, e.g., if the user has two versions
    // of RealPlayer installed and just removed the newer one, we'll pick up the older one.
    bool shouldSkipUnchangedFiles = !pluginSetChanged;

    auto pathsEnd = paths.end();
    for (auto it = paths.begin(); it != pathsEnd; ++it) {
        auto lastModifiedTime = FileSystem::getFileModificationTime(*it);
        if (!lastModifiedTime)
            continue;
        time_t lastModified = lastModifiedTime->secondsSinceEpoch().secondsAs<time_t>();

        pathsWithTimes.add(*it, lastModified);

        // If the path's timestamp hasn't changed since the last time we ran refresh(), we don't have to do anything.
        if (shouldSkipUnchangedFiles && m_pluginPathsWithTimes.get(*it) == lastModified)
            continue;

        if (RefPtr<PluginPackage> oldPackage = m_pluginsByPath.get(*it)) {
            ASSERT(!shouldSkipUnchangedFiles || oldPackage->lastModified() != lastModified);
            remove(oldPackage.get());
        }

        auto package = PluginPackage::createPackage(*it, lastModified);
        if (package && add(package.releaseNonNull()))
            pluginSetChanged = true;
    }

    // Cache all the paths we found with their timestamps for next time.
    pathsWithTimes.swap(m_pluginPathsWithTimes);

    if (!pluginSetChanged)
        return false;

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
    updatePersistentMetadataCache();
#endif

    m_registeredMIMETypes.clear();

    // Register plug-in MIME types
    auto end = m_plugins.end();
    for (auto it = m_plugins.begin(); it != end; ++it) {
        // Get MIME types
        auto map_it = (*it)->mimeToDescriptions().begin();
        auto map_end = (*it)->mimeToDescriptions().end();
        for (; map_it != map_end; ++map_it)
            m_registeredMIMETypes.add(map_it->key);
    }

    return true;
}

Vector<PluginPackage*> PluginDatabase::plugins() const
{
    Vector<PluginPackage*> result;

    auto end = m_plugins.end();
    for (auto it = m_plugins.begin(); it != end; ++it)
        result.append((*it).get());

    return result;
}

int PluginDatabase::preferredPluginCompare(const void* a, const void* b)
{
    PluginPackage* pluginA = *static_cast<PluginPackage* const*>(a);
    PluginPackage* pluginB = *static_cast<PluginPackage* const*>(b);

    return pluginA->compare(*pluginB);
}

PluginPackage* PluginDatabase::pluginForMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return nullptr;

    PluginPackage* preferredPlugin = m_preferredPlugins.get(mimeType);
    if (preferredPlugin
        && preferredPlugin->isEnabled()
        && preferredPlugin->mimeToDescriptions().contains(mimeType)) {
        return preferredPlugin;
    }

    Vector<PluginPackage*, 2> pluginChoices;

    auto end = m_plugins.end();
    for (auto it = m_plugins.begin(); it != end; ++it) {
        PluginPackage* plugin = (*it).get();

        if (!plugin->isEnabled())
            continue;

        if (plugin->mimeToDescriptions().contains(mimeType)) {
#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
            if (!plugin->ensurePluginLoaded())
                continue;
#endif
            pluginChoices.append(plugin);
        }
    }

    if (pluginChoices.isEmpty())
        return 0;

    qsort(pluginChoices.data(), pluginChoices.size(), sizeof(PluginPackage*), PluginDatabase::preferredPluginCompare);

    return pluginChoices[0];
}

String PluginDatabase::MIMETypeForExtension(const String& extension) const
{
    if (extension.isEmpty())
        return String();

    auto end = m_plugins.end();
    String mimeType;
    Vector<PluginPackage*, 2> pluginChoices;
    HashMap<PluginPackage*, String> mimeTypeForPlugin;

    for (auto it = m_plugins.begin(); it != end; ++it) {
        if (!(*it)->isEnabled())
            continue;

        auto mime_end = (*it)->mimeToExtensions().end();

        for (auto mime_it = (*it)->mimeToExtensions().begin(); mime_it != mime_end; ++mime_it) {
            mimeType = mime_it->key;
            PluginPackage* preferredPlugin = m_preferredPlugins.get(mimeType);
            const Vector<String>& extensions = mime_it->value;
            bool foundMapping = false;
            for (unsigned i = 0; i < extensions.size(); i++) {
                if (equalIgnoringASCIICase(extensions[i], extension)) {
                    PluginPackage* plugin = (*it).get();

                    if (preferredPlugin && PluginPackage::equal(*plugin, *preferredPlugin))
                        return mimeType;

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
                    if (!plugin->ensurePluginLoaded())
                        continue;
#endif
                    pluginChoices.append(plugin);
                    mimeTypeForPlugin.add(plugin, mimeType);
                    foundMapping = true;
                    break;
                }
            }
            if (foundMapping)
                break;
        }
    }

    if (pluginChoices.isEmpty())
        return String();

    qsort(pluginChoices.data(), pluginChoices.size(), sizeof(PluginPackage*), PluginDatabase::preferredPluginCompare);

    return mimeTypeForPlugin.get(pluginChoices[0]);
}

PluginPackage* PluginDatabase::findPlugin(const URL& url, String& mimeType)
{
    if (!mimeType.isEmpty())
        return pluginForMIMEType(mimeType);
    
    String filename = url.lastPathComponent();
    if (filename.endsWith('/'))
        return 0;
    
    int extensionPos = filename.reverseFind('.');
    if (extensionPos == -1)
        return 0;
    
    String mimeTypeForExtension = MIMETypeForExtension(filename.substring(extensionPos + 1));
    PluginPackage* plugin = pluginForMIMEType(mimeTypeForExtension);
    if (!plugin) {
        // FIXME: if no plugin could be found, query Windows for the mime type
        // corresponding to the extension.
        return 0;
    }
    
    mimeType = mimeTypeForExtension;
    return plugin;
}

void PluginDatabase::setPreferredPluginForMIMEType(const String& mimeType, PluginPackage* plugin)
{
    if (!plugin || plugin->mimeToExtensions().contains(mimeType))
        m_preferredPlugins.set(mimeType, plugin);
}

bool PluginDatabase::fileExistsAndIsNotDisabled(const String& filePath) const
{
    // Skip plugin files that are disabled by filename.
    if (m_disabledPluginFiles.contains(FileSystem::pathGetFileName(filePath)))
        return false;

    return FileSystem::fileExists(filePath);
}

void PluginDatabase::getDeletedPlugins(PluginSet& plugins) const
{
    auto end = m_plugins.end();
    for (auto it = m_plugins.begin(); it != end; ++it) {
        if (!fileExistsAndIsNotDisabled((*it)->path()))
            plugins.add(*it);
    }
}

bool PluginDatabase::add(Ref<PluginPackage>&& package)
{
    if (!m_plugins.add(package.copyRef()).isNewEntry)
        return false;

    m_pluginsByPath.add(package->path(), package.copyRef());
    return true;
}

void PluginDatabase::remove(PluginPackage* package)
{
    auto it = package->mimeToExtensions().begin();
    auto end = package->mimeToExtensions().end();
    for ( ; it != end; ++it) {
        auto packageInMap = m_preferredPlugins.find(it->key);
        if (packageInMap != m_preferredPlugins.end() && packageInMap->value == package)
            m_preferredPlugins.remove(packageInMap);
    }

    m_plugins.remove(package);
    m_pluginsByPath.remove(package->path());
}

void PluginDatabase::clear()
{
    m_plugins.clear();
    m_pluginsByPath.clear();
    m_pluginPathsWithTimes.clear();
    m_registeredMIMETypes.clear();
    m_preferredPlugins.clear();
#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)
    m_persistentMetadataCacheIsLoaded = false;
#endif
}

bool PluginDatabase::removeDisabledPluginFile(const String& fileName)
{
    return m_disabledPluginFiles.remove(fileName);
}

bool PluginDatabase::addDisabledPluginFile(const String& fileName)
{
    return m_disabledPluginFiles.add(fileName).isNewEntry;
}

#if !ENABLE(NETSCAPE_PLUGIN_API)
// For Safari/Win the following three methods are implemented
// in PluginDatabaseWin.cpp, but if we can use WebCore constructs
// for the logic we should perhaps move it here under XP_WIN?

Vector<String> PluginDatabase::defaultPluginDirectories()
{
    Vector<String> paths;

    String userPluginPath = FileSystem::homeDirectoryPath();
    userPluginPath.append(String("\\Application Data\\Mozilla\\plugins"));
    paths.append(userPluginPath);

    return paths;
}

bool PluginDatabase::isPreferredPluginDirectory(const String& path)
{
    String preferredPath = FileSystem::homeDirectoryPath();

    preferredPath.append(String("\\Application Data\\Mozilla\\plugins"));

    // TODO: We should normalize the path before doing a comparison.
    return path == preferredPath;
}

void PluginDatabase::getPluginPathsInDirectories(HashSet<String>& paths) const
{
    // FIXME: This should be a case insensitive set.
    HashSet<String> uniqueFilenames;

    String fileNameFilter("");

    auto dirsEnd = m_pluginDirectories.end();
    for (auto dIt = m_pluginDirectories.begin(); dIt != dirsEnd; ++dIt) {
        Vector<String> pluginPaths = FileSystem::listDirectory(*dIt, fileNameFilter);
        auto pluginsEnd = pluginPaths.end();
        for (auto pIt = pluginPaths.begin(); pIt != pluginsEnd; ++pIt) {
            if (!fileExistsAndIsNotDisabled(*pIt))
                continue;

            paths.add(*pIt);
        }
    }
}

#endif // !ENABLE(NETSCAPE_PLUGIN_API)

#if ENABLE(NETSCAPE_PLUGIN_METADATA_CACHE)

static void fillBufferWithContentsOfFile(FileSystem::PlatformFileHandle file, Vector<char>& buffer)
{
    size_t bufferSize = 0;
    size_t bufferCapacity = 1024;
    buffer.resize(bufferCapacity);

    do {
        bufferSize += FileSystem::readFromFile(file, buffer.data() + bufferSize, bufferCapacity - bufferSize);
        if (bufferSize == bufferCapacity) {
            if (bufferCapacity < maximumPersistentPluginMetadataCacheSize) {
                bufferCapacity *= 2;
                buffer.resize(bufferCapacity);
            } else {
                buffer.clear();
                return;
            }
        } else
            break;
    } while (true);

    buffer.shrink(bufferSize);
}

static bool readUTF8String(String& resultString, char*& start, const char* end)
{
    if (start >= end)
        return false;

    int len = strlen(start);
    resultString = String::fromUTF8(start, len);
    start += len + 1;

    return true;
}

static bool readTime(time_t& resultTime, char*& start, const char* end)
{
    if (start + sizeof(time_t) >= end)
        return false;

    // The stream is not necessary aligned.
    memcpy(&resultTime, start, sizeof(time_t));
    start += sizeof(time_t);

    return true;
}

static const char schemaVersion = '1';
static const char persistentPluginMetadataCacheFilename[] = "PluginMetadataCache.bin";

void PluginDatabase::loadPersistentMetadataCache()
{
    if (!isPersistentMetadataCacheEnabled() || persistentMetadataCachePath().isEmpty())
        return;

    FileSystem::PlatformFileHandle file;
    String absoluteCachePath = FileSystem::pathByAppendingComponent(persistentMetadataCachePath(), persistentPluginMetadataCacheFilename);
    file = FileSystem::openFile(absoluteCachePath, FileSystem::FileOpenMode::Read);

    if (!FileSystem::isHandleValid(file))
        return;

    // Mark cache as loaded regardless of success or failure. If
    // there's error in the cache, we won't try to load it anymore.
    m_persistentMetadataCacheIsLoaded = true;

    Vector<char> fileContents;
    fillBufferWithContentsOfFile(file, fileContents);
    FileSystem::closeFile(file);

    if (fileContents.size() < 2 || fileContents.first() != schemaVersion || fileContents.last() != '\0') {
        LOG_ERROR("Unable to read plugin metadata cache: corrupt schema");
        FileSystem::deleteFile(absoluteCachePath);
        return;
    }

    char* bufferPos = fileContents.data() + 1;
    char* end = fileContents.data() + fileContents.size();

    PluginSet cachedPlugins;
    HashMap<String, time_t> cachedPluginPathsWithTimes;
    HashMap<String, RefPtr<PluginPackage> > cachedPluginsByPath;

    while (bufferPos < end) {
        String path;
        time_t lastModified;
        String name;
        String desc;
        String mimeDesc;
        if (!(readUTF8String(path, bufferPos, end)
              && readTime(lastModified, bufferPos, end)
              && readUTF8String(name, bufferPos, end)
              && readUTF8String(desc, bufferPos, end)
              && readUTF8String(mimeDesc, bufferPos, end))) {
            LOG_ERROR("Unable to read plugin metadata cache: corrupt data");
            FileSystem::deleteFile(absoluteCachePath);
            return;
        }

        // Skip metadata that points to plugins from directories that
        // are not part of plugin directory list anymore.
        String pluginDirectoryName = FileSystem::directoryName(path);
        if (m_pluginDirectories.find(pluginDirectoryName) == WTF::notFound)
            continue;

        auto package = PluginPackage::createPackageFromCache(path, lastModified, name, desc, mimeDesc);

        if (cachedPlugins.add(package.copyRef()).isNewEntry) {
            cachedPluginPathsWithTimes.add(package->path(), package->lastModified());
            cachedPluginsByPath.add(package->path(), package);
        }
    }

    m_plugins.swap(cachedPlugins);
    m_pluginsByPath.swap(cachedPluginsByPath);
    m_pluginPathsWithTimes.swap(cachedPluginPathsWithTimes);
}

static bool writeUTF8String(FileSystem::PlatformFileHandle file, const String& string)
{
    CString utf8String = string.utf8();
    int length = utf8String.length() + 1;
    return FileSystem::writeToFile(file, utf8String.data(), length) == length;
}

static bool writeTime(FileSystem::PlatformFileHandle file, const time_t& time)
{
    return FileSystem::writeToFile(file, reinterpret_cast<const char*>(&time), sizeof(time_t)) == sizeof(time_t);
}

void PluginDatabase::updatePersistentMetadataCache()
{
    if (!isPersistentMetadataCacheEnabled() || persistentMetadataCachePath().isEmpty())
        return;

    FileSystem::makeAllDirectories(persistentMetadataCachePath());
    String absoluteCachePath = FileSystem::pathByAppendingComponent(persistentMetadataCachePath(), persistentPluginMetadataCacheFilename);
    FileSystem::deleteFile(absoluteCachePath);

    if (m_plugins.isEmpty())
        return;

    FileSystem::PlatformFileHandle file;
    file = FileSystem::openFile(absoluteCachePath, FileSystem::FileOpenMode::Write);

    if (!FileSystem::isHandleValid(file)) {
        LOG_ERROR("Unable to open plugin metadata cache for saving");
        return;
    }

    char localSchemaVersion = schemaVersion;
    if (FileSystem::writeToFile(file, &localSchemaVersion, 1) != 1) {
        LOG_ERROR("Unable to write plugin metadata cache schema");
        FileSystem::closeFile(file);
        FileSystem::deleteFile(absoluteCachePath);
        return;
    }

    auto end = m_plugins.end();
    for (auto it = m_plugins.begin(); it != end; ++it) {
        if (!(writeUTF8String(file, (*it)->path())
              && writeTime(file, (*it)->lastModified())
              && writeUTF8String(file, (*it)->name())
              && writeUTF8String(file, (*it)->description())
              && writeUTF8String(file, (*it)->fullMIMEDescription()))) {
            LOG_ERROR("Unable to write plugin metadata to cache");
            FileSystem::closeFile(file);
            FileSystem::deleteFile(absoluteCachePath);
            return;
        }
    }

    FileSystem::closeFile(file);
}

bool PluginDatabase::isPersistentMetadataCacheEnabled()
{
    return gPersistentPluginMetadataCacheIsEnabled;
}

void PluginDatabase::setPersistentMetadataCacheEnabled(bool isEnabled)
{
    gPersistentPluginMetadataCacheIsEnabled = isEnabled;
}

String PluginDatabase::persistentMetadataCachePath()
{
    return WebCore::persistentPluginMetadataCachePath();
}

void PluginDatabase::setPersistentMetadataCachePath(const String& persistentMetadataCachePath)
{
    WebCore::persistentPluginMetadataCachePath() = persistentMetadataCachePath;
}
#endif
}
