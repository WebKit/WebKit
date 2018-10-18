/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebsiteDataStore.h"

#import "CookieStorageUtilsCF.h"
#import "StorageManager.h"
#import "WebResourceLoadStatisticsStore.h"
#import "WebsiteDataStoreParameters.h"
#import <WebCore/FileSystem.h>
#import <WebCore/SearchPopupMenuCocoa.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessPrivilege.h>

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIApplication.h>
#endif

namespace WebKit {

static id terminationObserver;

static Vector<WebsiteDataStore*>& dataStoresWithStorageManagers()
{
    static NeverDestroyed<Vector<WebsiteDataStore*>> dataStoresWithStorageManagers;

    return dataStoresWithStorageManagers;
}

WebsiteDataStoreParameters WebsiteDataStore::parameters()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    resolveDirectoriesIfNecessary();

    WebsiteDataStoreParameters parameters;
    parameters.networkSessionParameters = {
        m_sessionID,
        m_boundInterfaceIdentifier,
        m_allowsCellularAccess,
        m_proxyConfiguration,
        m_configuration.sourceApplicationBundleIdentifier,
        m_configuration.sourceApplicationSecondaryIdentifier,
    };

    auto cookieFile = resolvedCookieStorageFile();

    if (m_uiProcessCookieStorageIdentifier.isEmpty()) {
        auto utf8File = cookieFile.utf8();
        auto url = adoptCF(CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)utf8File.data(), (CFIndex)utf8File.length(), true));
        m_cfCookieStorage = adoptCF(CFHTTPCookieStorageCreateFromFile(kCFAllocatorDefault, url.get(), nullptr));
        m_uiProcessCookieStorageIdentifier = identifyingDataFromCookieStorage(m_cfCookieStorage.get());
    }

    parameters.uiProcessCookieStorageIdentifier = m_uiProcessCookieStorageIdentifier;
    parameters.networkSessionParameters.sourceApplicationBundleIdentifier = m_configuration.sourceApplicationBundleIdentifier;
    parameters.networkSessionParameters.sourceApplicationSecondaryIdentifier = m_configuration.sourceApplicationSecondaryIdentifier;

    parameters.pendingCookies = copyToVector(m_pendingCookies);

    if (!cookieFile.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(WebCore::FileSystem::directoryName(cookieFile), parameters.cookieStoragePathExtensionHandle);

#if ENABLE(INDEXED_DATABASE)
    parameters.indexedDatabaseDirectory = resolvedIndexedDatabaseDirectory();
    if (!parameters.indexedDatabaseDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.indexedDatabaseDirectory, parameters.indexedDatabaseDirectoryExtensionHandle);
#endif

#if ENABLE(SERVICE_WORKER)
    parameters.serviceWorkerRegistrationDirectory = resolvedServiceWorkerRegistrationDirectory();
    if (!parameters.serviceWorkerRegistrationDirectory.isEmpty())
        SandboxExtension::createHandleForReadWriteDirectory(parameters.serviceWorkerRegistrationDirectory, parameters.serviceWorkerRegistrationDirectoryExtensionHandle);
#endif

    return parameters;
}

void WebsiteDataStore::platformInitialize()
{
    if (!m_storageManager)
        return;

    if (!terminationObserver) {
        ASSERT(dataStoresWithStorageManagers().isEmpty());

#if PLATFORM(MAC)
        NSString *notificationName = NSApplicationWillTerminateNotification;
#else
        NSString *notificationName = UIApplicationWillTerminateNotification;
#endif
        terminationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:notificationName object:nil queue:nil usingBlock:^(NSNotification *note) {
            for (auto& dataStore : dataStoresWithStorageManagers()) {
                dataStore->m_storageManager->applicationWillTerminate();
                if (dataStore->m_resourceLoadStatistics)
                    dataStore->m_resourceLoadStatistics->applicationWillTerminate();
            }
        }];
    }

    ASSERT(!dataStoresWithStorageManagers().contains(this));
    dataStoresWithStorageManagers().append(this);
}

void WebsiteDataStore::platformDestroy()
{
    if (m_resourceLoadStatistics)
        m_resourceLoadStatistics->applicationWillTerminate();

    if (!m_storageManager)
        return;

    // FIXME: Rename applicationWillTerminate to something that better indicates what StorageManager does (waits for pending writes to finish).
    m_storageManager->applicationWillTerminate();

    ASSERT(dataStoresWithStorageManagers().contains(this));
    dataStoresWithStorageManagers().removeFirst(this);

    if (dataStoresWithStorageManagers().isEmpty()) {
        [[NSNotificationCenter defaultCenter] removeObserver:terminationObserver];
        terminationObserver = nil;
    }
}

void WebsiteDataStore::platformRemoveRecentSearches(WallTime oldestTimeToRemove)
{
    WebCore::removeRecentlyModifiedRecentSearches(oldestTimeToRemove);
}

}
