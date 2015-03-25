/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "APIWebsiteDataRecord.h"
#include "StorageManager.h"
#include "WebProcessPool.h"
#include "WebsiteData.h"
#include <WebCore/DatabaseTracker.h>
#include <WebCore/OriginLock.h>
#include <wtf/RunLoop.h>

namespace WebKit {

static WebCore::SessionID generateNonPersistentSessionID()
{
    // FIXME: We count backwards here to not conflict with API::Session.
    static uint64_t sessionID = std::numeric_limits<uint64_t>::max();

    return WebCore::SessionID(--sessionID);
}

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

RefPtr<WebsiteDataStore> WebsiteDataStore::createNonPersistent()
{
    return adoptRef(new WebsiteDataStore(generateNonPersistentSessionID()));
}

RefPtr<WebsiteDataStore> WebsiteDataStore::create(Configuration configuration)
{
    return adoptRef(new WebsiteDataStore(WTF::move(configuration)));
}

WebsiteDataStore::WebsiteDataStore(Configuration configuration)
    : m_identifier(generateIdentifier())
    , m_sessionID(WebCore::SessionID::defaultSessionID())
    , m_networkCacheDirectory(WTF::move(configuration.networkCacheDirectory))
    , m_applicationCacheDirectory(WTF::move(configuration.applicationCacheDirectory))
    , m_webSQLDatabaseDirectory(WTF::move(configuration.webSQLDatabaseDirectory))
    , m_storageManager(StorageManager::create(WTF::move(configuration.localStorageDirectory)))
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    platformInitialize();
}

WebsiteDataStore::WebsiteDataStore(WebCore::SessionID sessionID)
    : m_identifier(generateIdentifier())
    , m_sessionID(sessionID)
    , m_queue(WorkQueue::create("com.apple.WebKit.WebsiteDataStore"))
{
    platformInitialize();
}

WebsiteDataStore::~WebsiteDataStore()
{
    platformDestroy();
}

void WebsiteDataStore::cloneSessionData(WebPageProxy& sourcePage, WebPageProxy& newPage)
{
    auto& sourceDataStore = sourcePage.websiteDataStore();
    auto& newDataStore = newPage.websiteDataStore();

    // FIXME: Handle this.
    if (&sourceDataStore != &newDataStore)
        return;

    if (!sourceDataStore.m_storageManager)
        return;

    sourceDataStore.m_storageManager->cloneSessionStorageNamespace(sourcePage.pageID(), newPage.pageID());
}

enum class ProcessAccessType {
    None,
    OnlyIfLaunched,
    Launch,
};

static ProcessAccessType computeNetworkProcessAccessTypeForDataFetch(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeCookies) {
        if (isNonPersistentStore)
            processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
        else
            processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    }

    if (dataTypes & WebsiteDataTypeDiskCache && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataFetch(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeMemoryCache)
        return ProcessAccessType::OnlyIfLaunched;

    return processAccessType;
}

void WebsiteDataStore::fetchData(WebsiteDataTypes dataTypes, std::function<void (Vector<WebsiteDataRecord>)> completionHandler)
{
    struct CallbackAggregator final : public RefCounted<CallbackAggregator> {
        explicit CallbackAggregator(std::function<void (Vector<WebsiteDataRecord>)> completionHandler)
            : completionHandler(WTF::move(completionHandler))
        {
        }

        ~CallbackAggregator()
        {
            ASSERT(!pendingCallbacks);
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback(WebsiteData websiteData)
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            for (auto& entry : websiteData.entries) {
                auto displayName = WebsiteDataRecord::displayNameForOrigin(*entry.origin);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTF::move(displayName);

                record.add(entry.type, WTF::move(entry.origin));
            }

            for (auto& hostName : websiteData.hostNamesWithCookies) {
                auto displayName = WebsiteDataRecord::displayNameForCookieHostName(hostName);
                if (!displayName)
                    continue;

                auto& record = m_websiteDataRecords.add(displayName, WebsiteDataRecord { }).iterator->value;
                if (!record.displayName)
                    record.displayName = WTF::move(displayName);

                record.addCookieHostName(hostName);
            }

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (pendingCallbacks)
                return;

            RefPtr<CallbackAggregator> callbackAggregator(this);
            RunLoop::main().dispatch([callbackAggregator] {

                WTF::Vector<WebsiteDataRecord> records;
                records.reserveInitialCapacity(callbackAggregator->m_websiteDataRecords.size());

                for (auto& record : callbackAggregator->m_websiteDataRecords.values())
                    records.uncheckedAppend(WTF::move(record));

                callbackAggregator->completionHandler(WTF::move(records));
            });
        }

        unsigned pendingCallbacks = 0;
        std::function<void (Vector<WebsiteDataRecord>)> completionHandler;

        HashMap<String, WebsiteDataRecord> m_websiteDataRecords;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTF::move(completionHandler)));

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataFetch(dataTypes, isNonPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        HashSet<WebProcessPool*> processPools;
        for (auto& process : processes())
            processPools.add(&process->processPool());

        for (auto& processPool : processPools) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataFetch(dataTypes, isNonPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            process->fetchWebsiteData(m_sessionID, dataTypes, [callbackAggregator](WebsiteData websiteData) {
                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        }
    }

    if (dataTypes & WebsiteDataTypeLocalStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->getOrigins([callbackAggregator](Vector<RefPtr<WebCore::SecurityOrigin>> origins) {
            WebsiteData websiteData;
            for (auto& origin : origins)
                websiteData.entries.append(WebsiteData::Entry { WTF::move(origin), WebsiteDataTypeLocalStorage });

            callbackAggregator->removePendingCallback(WTF::move(websiteData));
        });
    }

    if (dataTypes & WebsiteDataTypeWebSQLDatabases && !isNonPersistent()) {
        StringCapture webSQLDatabaseDirectory { m_webSQLDatabaseDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory, callbackAggregator] {
            Vector<RefPtr<WebCore::SecurityOrigin>> origins;
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory.string())->origins(origins);

            RunLoop::main().dispatch([webSQLDatabaseDirectory, callbackAggregator, origins]() mutable {
                WebsiteData websiteData;
                for (auto& origin : origins)
                    websiteData.entries.append(WebsiteData::Entry { WTF::move(origin), WebsiteDataTypeWebSQLDatabases });

                callbackAggregator->removePendingCallback(WTF::move(websiteData));
            });
        });
    }

    callbackAggregator->callIfNeeded();
}

static ProcessAccessType computeNetworkProcessAccessTypeForDataRemoval(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeCookies) {
        if (isNonPersistentStore)
            processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);
        else
            processAccessType = std::max(processAccessType, ProcessAccessType::Launch);
    }

    if (dataTypes & WebsiteDataTypeDiskCache && !isNonPersistentStore)
        processAccessType = std::max(processAccessType, ProcessAccessType::Launch);

    return processAccessType;
}

static ProcessAccessType computeWebProcessAccessTypeForDataRemoval(WebsiteDataTypes dataTypes, bool isNonPersistentStore)
{
    UNUSED_PARAM(isNonPersistentStore);

    ProcessAccessType processAccessType = ProcessAccessType::None;

    if (dataTypes & WebsiteDataTypeMemoryCache)
        processAccessType = std::max(processAccessType, ProcessAccessType::OnlyIfLaunched);

    return processAccessType;
}

void WebsiteDataStore::removeData(WebsiteDataTypes dataTypes, std::chrono::system_clock::time_point modifiedSince, std::function<void ()> completionHandler)
{
    struct CallbackAggregator : public RefCounted<CallbackAggregator> {
        explicit CallbackAggregator (std::function<void ()> completionHandler)
            : completionHandler(WTF::move(completionHandler))
        {
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (!pendingCallbacks)
                RunLoop::main().dispatch(WTF::move(completionHandler));
        }

        unsigned pendingCallbacks = 0;
        std::function<void ()> completionHandler;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTF::move(completionHandler)));

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, isNonPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        HashSet<WebProcessPool*> processPools;
        for (auto& process : processes())
            processPools.add(&process->processPool());

        for (auto& processPool : processPools) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, isNonPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();
            process->deleteWebsiteData(m_sessionID, dataTypes, modifiedSince, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes & WebsiteDataTypeLocalStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteLocalStorageOriginsModifiedSince(modifiedSince, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes & WebsiteDataTypeWebSQLDatabases && !isNonPersistent()) {
        StringCapture webSQLDatabaseDirectory { m_webSQLDatabaseDirectory };

        callbackAggregator->addPendingCallback();

        m_queue->dispatch([webSQLDatabaseDirectory, callbackAggregator, modifiedSince] {
            WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory.string())->deleteDatabasesModifiedSince(modifiedSince);

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::removeData(WebsiteDataTypes dataTypes, const Vector<WebsiteDataRecord>& dataRecords, std::function<void ()> completionHandler)
{
    Vector<RefPtr<WebCore::SecurityOrigin>> origins;

    for (const auto& dataRecord : dataRecords) {
        for (auto& origin : dataRecord.origins)
            origins.append(origin);
    }

    struct CallbackAggregator : public ThreadSafeRefCounted<CallbackAggregator> {
        explicit CallbackAggregator (std::function<void ()> completionHandler)
            : completionHandler(WTF::move(completionHandler))
        {
        }

        void addPendingCallback()
        {
            pendingCallbacks++;
        }

        void removePendingCallback()
        {
            ASSERT(pendingCallbacks);
            --pendingCallbacks;

            callIfNeeded();
        }

        void callIfNeeded()
        {
            if (!pendingCallbacks)
                RunLoop::main().dispatch(WTF::move(completionHandler));
        }

        unsigned pendingCallbacks = 0;
        std::function<void ()> completionHandler;
    };

    RefPtr<CallbackAggregator> callbackAggregator = adoptRef(new CallbackAggregator(WTF::move(completionHandler)));

    auto networkProcessAccessType = computeNetworkProcessAccessTypeForDataRemoval(dataTypes, isNonPersistent());
    if (networkProcessAccessType != ProcessAccessType::None) {
        HashSet<WebProcessPool*> processPools;
        for (auto& process : processes())
            processPools.add(&process->processPool());

        for (auto& processPool : processPools) {
            switch (networkProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!processPool->networkProcess())
                    continue;
                break;

            case ProcessAccessType::Launch:
                processPool->ensureNetworkProcess();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            Vector<String> cookieHostNames;
            for (const auto& dataRecord : dataRecords) {
                for (auto& hostName : dataRecord.cookieHostNames)
                    cookieHostNames.append(hostName);
            }

            callbackAggregator->addPendingCallback();
            processPool->networkProcess()->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, cookieHostNames, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    auto webProcessAccessType = computeWebProcessAccessTypeForDataRemoval(dataTypes, isNonPersistent());
    if (webProcessAccessType != ProcessAccessType::None) {
        for (auto& process : processes()) {
            switch (webProcessAccessType) {
            case ProcessAccessType::OnlyIfLaunched:
                if (!process->canSendMessage())
                    continue;
                break;

            case ProcessAccessType::Launch:
                // FIXME: Handle this.
                ASSERT_NOT_REACHED();
                break;

            case ProcessAccessType::None:
                ASSERT_NOT_REACHED();
            }

            callbackAggregator->addPendingCallback();

            process->deleteWebsiteDataForOrigins(m_sessionID, dataTypes, origins, [callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        }
    }

    if (dataTypes & WebsiteDataTypeLocalStorage && m_storageManager) {
        callbackAggregator->addPendingCallback();

        m_storageManager->deleteEntriesForOrigins(origins, [callbackAggregator] {
            callbackAggregator->removePendingCallback();
        });
    }

    if (dataTypes & WebsiteDataTypeWebSQLDatabases && !isNonPersistent()) {
        StringCapture webSQLDatabaseDirectory { m_webSQLDatabaseDirectory };

        HashSet<RefPtr<WebCore::SecurityOrigin>> origins;
        for (const auto& dataRecord : dataRecords) {
            for (const auto& origin : dataRecord.origins)
                origins.add(origin);
        }

        callbackAggregator->addPendingCallback();
        m_queue->dispatch([origins, callbackAggregator, webSQLDatabaseDirectory] {
            auto databaseTracker = WebCore::DatabaseTracker::trackerWithDatabasePath(webSQLDatabaseDirectory.string());

            for (const auto& origin : origins)
                databaseTracker->deleteOrigin(origin.get());

            RunLoop::main().dispatch([callbackAggregator] {
                callbackAggregator->removePendingCallback();
            });
        });
    }

    // There's a chance that we don't have any pending callbacks. If so, we want to dispatch the completion handler right away.
    callbackAggregator->callIfNeeded();
}

void WebsiteDataStore::webPageWasAdded(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->createSessionStorageNamespace(webPageProxy.pageID(), std::numeric_limits<unsigned>::max());
}

void WebsiteDataStore::webPageWasRemoved(WebPageProxy& webPageProxy)
{
    if (m_storageManager)
        m_storageManager->destroySessionStorageNamespace(webPageProxy.pageID());
}

void WebsiteDataStore::webProcessWillOpenConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processWillOpenConnection(webProcessProxy, connection);
}

void WebsiteDataStore::webPageWillOpenConnection(WebPageProxy& webPageProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->setAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), &connection);
}

void WebsiteDataStore::webPageDidCloseConnection(WebPageProxy& webPageProxy, IPC::Connection&)
{
    if (m_storageManager)
        m_storageManager->setAllowedSessionStorageNamespaceConnection(webPageProxy.pageID(), nullptr);
}

void WebsiteDataStore::webProcessDidCloseConnection(WebProcessProxy& webProcessProxy, IPC::Connection& connection)
{
    if (m_storageManager)
        m_storageManager->processDidCloseConnection(webProcessProxy, connection);
}

}
