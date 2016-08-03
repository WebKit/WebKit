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
#include "WebCookieManagerProxy.h"
#include "WebInspectorServer.h"
#include "WebProcessCreationParameters.h"
#include "WebProcessMessages.h"
#include "WebSoupCustomProtocolRequestManager.h"
#include <WebCore/FileSystem.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/SchemeRegistry.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebKit {

static void initInspectorServer()
{
#if ENABLE(INSPECTOR_SERVER)
    static bool initialized = false;
    if (initialized)
        return;

    initialized = true;
    String serverAddress(g_getenv("WEBKIT_INSPECTOR_SERVER"));

    if (!serverAddress.isNull()) {
        String bindAddress = "127.0.0.1";
        unsigned short port = 2999;

        Vector<String> result;
        serverAddress.split(':', result);

        if (result.size() == 2) {
            bindAddress = result[0];
            bool ok = false;
            port = result[1].toInt(&ok);
            if (!ok) {
                port = 2999;
                LOG_ERROR("Couldn't parse the port. Use 2999 instead.");
            }
        } else
            LOG_ERROR("Couldn't parse %s, wrong format? Use 127.0.0.1:2999 instead.", serverAddress.utf8().data());

        if (!WebInspectorServer::singleton().listen(bindAddress, port))
            LOG_ERROR("Couldn't start listening on: IP address=%s, port=%d.", bindAddress.utf8().data(), port);
        return;
    }

    LOG(InspectorServer, "To start inspector server set WEBKIT_INSPECTOR_SERVER to 127.0.0.1:2999 for example.");
#endif
}

WTF::String WebProcessPool::legacyPlatformDefaultApplicationCacheDirectory()
{
    return API::WebsiteDataStore::defaultApplicationCacheDirectory();
}

WTF::String WebProcessPool::legacyPlatformDefaultMediaCacheDirectory()
{
    return API::WebsiteDataStore::defaultMediaCacheDirectory();
}

void WebProcessPool::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    initInspectorServer();
    parameters.memoryCacheDisabled = m_memoryCacheDisabled || cacheModel() == CacheModelDocumentViewer;
}

void WebProcessPool::platformInvalidateContext()
{
}

String WebProcessPool::legacyPlatformDefaultWebSQLDatabaseDirectory()
{
    return API::WebsiteDataStore::defaultWebSQLDatabaseDirectory();
}

String WebProcessPool::legacyPlatformDefaultIndexedDBDatabaseDirectory()
{
    return API::WebsiteDataStore::defaultIndexedDBDatabaseDirectory();
}

String WebProcessPool::platformDefaultIconDatabasePath() const
{
    GUniquePtr<gchar> databaseDirectory(g_build_filename(g_get_user_cache_dir(), "webkitgtk", "icondatabase", nullptr));
    return WebCore::filenameToString(databaseDirectory.get());
}

String WebProcessPool::legacyPlatformDefaultLocalStorageDirectory()
{
    return API::WebsiteDataStore::defaultLocalStorageDirectory();
}

String WebProcessPool::legacyPlatformDefaultMediaKeysStorageDirectory()
{
    return API::WebsiteDataStore::defaultMediaKeysStorageDirectory();
}

String WebProcessPool::legacyPlatformDefaultNetworkCacheDirectory()
{
    return API::WebsiteDataStore::defaultNetworkCacheDirectory();
}

} // namespace WebKit
