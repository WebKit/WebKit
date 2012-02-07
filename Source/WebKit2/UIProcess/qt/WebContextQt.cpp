/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 University of Szeged
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
#include "WebContext.h"

#include "ApplicationCacheStorage.h"
#include "FileSystem.h"
#include "WKSharedAPICast.h"
#if ENABLE(GEOLOCATION)
#include "WebGeolocationProviderQt.h"
#endif
#include "WebProcessCreationParameters.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QProcess>

namespace WebKit {

static QString defaultDataLocation()
{
    static QString s_dataLocation;

    if (!s_dataLocation.isEmpty())
        return s_dataLocation;

    QString dataLocation = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    if (dataLocation.isEmpty())
        dataLocation = WebCore::pathByAppendingComponent(QDir::homePath(), QCoreApplication::applicationName());
    s_dataLocation = WebCore::pathByAppendingComponent(dataLocation, ".QtWebKit/");
    WebCore::makeAllDirectories(s_dataLocation);
    return s_dataLocation;
}

static String defaultDiskCacheDirectory()
{
    static String s_defaultDiskCacheDirectory;

    if (!s_defaultDiskCacheDirectory.isEmpty())
        return s_defaultDiskCacheDirectory;

    s_defaultDiskCacheDirectory = WebCore::pathByAppendingComponent(defaultDataLocation(), "cache/");
    WebCore::makeAllDirectories(s_defaultDiskCacheDirectory);
    return s_defaultDiskCacheDirectory;
}

static QString s_defaultDatabaseDirectory;
static QString s_defaultLocalStorageDirectory;

String WebContext::applicationCacheDirectory()
{
    return WebCore::cacheStorage().cacheDirectory();
}

void WebContext::platformInitializeWebProcess(WebProcessCreationParameters& parameters)
{
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    parameters.cookieStorageDirectory = defaultDataLocation();
    parameters.diskCacheDirectory = defaultDiskCacheDirectory();

#if ENABLE(GEOLOCATION)
    static WebGeolocationProviderQt* location = WebGeolocationProviderQt::create(toAPI(geolocationManagerProxy()));
    WKGeolocationManagerSetProvider(toAPI(geolocationManagerProxy()), WebGeolocationProviderQt::provider(location));
#endif
}

void WebContext::platformInvalidateContext()
{
}

String WebContext::platformDefaultDatabaseDirectory() const
{
    if (!s_defaultDatabaseDirectory.isEmpty())
        return s_defaultDatabaseDirectory;

    s_defaultDatabaseDirectory = defaultDataLocation() + QLatin1String("Databases");
    QDir().mkpath(s_defaultDatabaseDirectory);
    return s_defaultDatabaseDirectory;
}

String WebContext::platformDefaultIconDatabasePath() const
{
    return defaultDataLocation() + QLatin1String("WebpageIcons.db");
}

String WebContext::platformDefaultLocalStorageDirectory() const
{
    if (!s_defaultLocalStorageDirectory.isEmpty())
        return s_defaultLocalStorageDirectory;

    s_defaultLocalStorageDirectory = defaultDataLocation() + QLatin1String("LocalStorage");
    QDir().mkpath(s_defaultLocalStorageDirectory);
    return s_defaultLocalStorageDirectory;
}

} // namespace WebKit
