/*
 * Copyright (C) 2015-2017 Igalia S.L.
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
#include "WebsiteDataStore.h"

#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

static String programName()
{
    if (auto* prgname = g_get_prgname())
        return String::fromUTF8(prgname);

#if PLATFORM(GTK)
    return "webkitgtk"_s;
#elif PLATFORM(WPE)
    return "wpe"_s;
#else
    return "WebKit"_s;
#endif
}

const String& WebsiteDataStore::defaultBaseCacheDirectory()
{
    static NeverDestroyed<String> baseCacheDirectory;
    static std::once_flag once;
    std::call_once(once, [] {

        baseCacheDirectory.get() = FileSystem::pathByAppendingComponent(FileSystem::userCacheDirectory(), programName());
    });
    return baseCacheDirectory;
}

const String& WebsiteDataStore::defaultBaseDataDirectory()
{
    static NeverDestroyed<String> baseDataDirectory;
    static std::once_flag once;
    std::call_once(once, [] {
        baseDataDirectory.get() = FileSystem::pathByAppendingComponent(FileSystem::userDataDirectory(), programName());
    });
    return baseDataDirectory;
}

String WebsiteDataStore::cacheDirectoryFileSystemRepresentation(const String& directoryName, const String& baseCacheDirectory, ShouldCreateDirectory)
{
    return FileSystem::pathByAppendingComponent(baseCacheDirectory.isNull() ? defaultBaseCacheDirectory() : baseCacheDirectory, directoryName);
}

String WebsiteDataStore::websiteDataDirectoryFileSystemRepresentation(const String& directoryName, const String& baseDataDirectory, ShouldCreateDirectory)
{
    return FileSystem::pathByAppendingComponent(baseDataDirectory.isNull() ? defaultBaseDataDirectory() : baseDataDirectory, directoryName);
}

UnifiedOriginStorageLevel WebsiteDataStore::defaultUnifiedOriginStorageLevel()
{
#if ENABLE(2022_GLIB_API)
    return UnifiedOriginStorageLevel::Basic;
#else
    return UnifiedOriginStorageLevel::None;
#endif
}

} // namespace API
