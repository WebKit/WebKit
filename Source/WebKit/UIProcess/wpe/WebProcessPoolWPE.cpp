/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2012 Samsung Electronics Ltd. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS AS IS''
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
#include "WebProcessPool.h"

#include "APIProcessPoolConfiguration.h"
#include "Logging.h"
#include "NetworkProcessMessages.h"
#include "WebCookieManagerProxy.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include <JavaScriptCore/RemoteInspectorServer.h>
#include <WebCore/FileSystem.h>
#include <WebCore/GStreamerCommon.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SchemeRegistry.h>
#include <cstdlib>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

#if ENABLE(REMOTE_INSPECTOR)
static bool initializeRemoteInspectorServer(const char* address)
{
    if (Inspector::RemoteInspectorServer::singleton().isRunning())
        return true;

    if (!address[0])
        return false;

    GUniquePtr<char> inspectorAddress(g_strdup(address));
    char* portPtr = g_strrstr(inspectorAddress.get(), ":");
    if (!portPtr)
        return false;

    *portPtr = '\0';
    portPtr++;
    guint64 port = g_ascii_strtoull(portPtr, nullptr, 10);
    if (!port)
        return false;

    return Inspector::RemoteInspectorServer::singleton().start(inspectorAddress.get(), port);
}
#endif

void WebProcessPool::platformInitialize()
{
#if ENABLE(REMOTE_INSPECTOR)
    if (const char* address = g_getenv("WEBKIT_INSPECTOR_SERVER")) {
        if (!initializeRemoteInspectorServer(address))
            g_unsetenv("WEBKIT_INSPECTOR_SERVER");
    }
#endif
}

WTF::String WebProcessPool::legacyPlatformDefaultApplicationCacheDirectory()
{
    GUniquePtr<gchar> cacheDirectory(g_build_filename(g_get_user_cache_dir(), "wpe", "appcache", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(cacheDirectory.get());
}

WTF::String WebProcessPool::legacyPlatformDefaultMediaCacheDirectory()
{
    GUniquePtr<gchar> cacheDirectory(g_build_filename(g_get_user_cache_dir(), "wpe", "mediacache", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(cacheDirectory.get());
}

void WebProcessPool::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    parameters.memoryCacheDisabled = m_memoryCacheDisabled || cacheModel() == CacheModelDocumentViewer;
#if USE(GSTREAMER)
    parameters.gstreamerOptions = WebCore::extractGStreamerOptionsFromCommandLine();
#endif

}

void WebProcessPool::platformInvalidateContext()
{
    notImplemented();
}

String WebProcessPool::legacyPlatformDefaultWebSQLDatabaseDirectory()
{
    GUniquePtr<gchar> databaseDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "databases", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(databaseDirectory.get());
}

String WebProcessPool::legacyPlatformDefaultIndexedDBDatabaseDirectory()
{
    GUniquePtr<gchar> indexedDBDatabaseDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "databases", "indexeddb", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(indexedDBDatabaseDirectory.get());
}

String WebProcessPool::legacyPlatformDefaultLocalStorageDirectory()
{
    GUniquePtr<gchar> storageDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "localstorage", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(storageDirectory.get());
}

String WebProcessPool::legacyPlatformDefaultMediaKeysStorageDirectory()
{
    GUniquePtr<gchar> mediaKeysStorageDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "mediakeys", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(mediaKeysStorageDirectory.get());
}

String WebProcessPool::legacyPlatformDefaultNetworkCacheDirectory()
{
    GUniquePtr<char> diskCacheDirectory(g_build_filename(g_get_user_cache_dir(), "wpe", "cache", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(diskCacheDirectory.get());
}

String WebProcessPool::legacyPlatformDefaultJavaScriptConfigurationDirectory()
{
    GUniquePtr<gchar> javaScriptCoreConfigDirectory(g_build_filename(g_get_user_data_dir(), "wpe", "JavaScriptCoreDebug", nullptr));
    return WebCore::FileSystem::stringFromFileSystemRepresentation(javaScriptCoreConfigDirectory.get());
}

void WebProcessPool::platformResolvePathsForSandboxExtensions()
{
}

} // namespace WebKit
