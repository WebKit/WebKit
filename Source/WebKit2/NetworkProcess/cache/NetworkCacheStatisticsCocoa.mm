/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_CACHE)
#include "NetworkCacheStatistics.h"

#include "Logging.h"
#include "NetworkCache.h"
#include "NetworkCacheFileSystemPosix.h"
#include "NetworkProcess.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DiagnosticLoggingResultType.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SQLiteDatabaseTracker.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <wtf/RunLoop.h>

namespace WebKit {

static const char* networkCacheStatisticsDatabaseName = "WebKitCacheStatistics.db";

static bool executeSQLCommand(WebCore::SQLiteDatabase& database, const String& sql)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(WebCore::SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(database.isOpen());

    bool result = database.executeCommand(sql);
    if (!result)
        LOG_ERROR("Network cache statistics: failed to execute statement \"%s\" error \"%s\"", sql.utf8().data(), database.lastErrorMsg());

    return result;
}

static bool executeSQLStatement(WebCore::SQLiteStatement& statement)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(WebCore::SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(statement.database().isOpen());

    bool result = statement.executeCommand();
    if (!result)
        LOG_ERROR("Network cache statistics: failed to execute statement \"%s\" error \"%s\"", statement.query().utf8().data(), statement.database().lastErrorMsg());

    return result;
}

std::unique_ptr<NetworkCacheStatistics> NetworkCacheStatistics::open(const String& cachePath)
{
    ASSERT(RunLoop::isMain());

    String databasePath = WebCore::pathByAppendingComponent(cachePath, networkCacheStatisticsDatabaseName);
    return std::unique_ptr<NetworkCacheStatistics>(new NetworkCacheStatistics(databasePath));
}

NetworkCacheStatistics::NetworkCacheStatistics(const String& databasePath)
    : m_backgroundIOQueue(adoptDispatch(dispatch_queue_create("com.apple.WebKit.Cache.Statistics.Background", DISPATCH_QUEUE_SERIAL)))
{
    dispatch_set_target_queue(m_backgroundIOQueue.get(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));

    initialize(databasePath);
}

void NetworkCacheStatistics::initialize(const String& databasePath)
{
    ASSERT(RunLoop::isMain());

    auto startTime = std::chrono::system_clock::now();

    StringCapture databasePathCapture(databasePath);
    StringCapture networkCachePathCapture(NetworkCache::singleton().storagePath());
    dispatch_async(m_backgroundIOQueue.get(), [this, databasePathCapture, networkCachePathCapture, startTime] {
        WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;

        String databasePath = databasePathCapture.string();
        if (!WebCore::makeAllDirectories(WebCore::directoryName(databasePath)))
            return;

        LOG(NetworkCache, "(NetworkProcess) Opening network cache statistics database at %s...", databasePath.utf8().data());
        m_database.open(databasePath);
        m_database.disableThreadingChecks();
        if (!m_database.isOpen()) {
            LOG_ERROR("Network cache statistics: Failed to open / create the network cache statistics database");
            return;
        }

        executeSQLCommand(m_database, ASCIILiteral("CREATE TABLE IF NOT EXISTS AlreadyRequested (hash TEXT PRIMARY KEY)"));
        executeSQLCommand(m_database, ASCIILiteral("CREATE TABLE IF NOT EXISTS UncachedReason (hash TEXT PRIMARY KEY, reason INTEGER)"));

        WebCore::SQLiteStatement statement(m_database, ASCIILiteral("SELECT count(*) FROM AlreadyRequested"));
        if (statement.prepareAndStep() != WebCore::SQLResultRow) {
            LOG_ERROR("Network cache statistics: Failed to count the number of rows in AlreadyRequested table");
            return;
        }

        m_approximateEntryCount = statement.getColumnInt(0);

#if !LOG_DISABLED
        auto elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
#endif
        LOG(NetworkCache, "(NetworkProcess) Network cache statistics database load complete, entries=%lu time=%lldms", static_cast<size_t>(m_approximateEntryCount), elapsedMS);

        if (!m_approximateEntryCount) {
            bootstrapFromNetworkCache(networkCachePathCapture.string());
#if !LOG_DISABLED
            elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
#endif
            LOG(NetworkCache, "(NetworkProcess) Network cache statistics database bootstrapping complete, entries=%lu time=%lldms", static_cast<size_t>(m_approximateEntryCount), elapsedMS);
        }
    });
}

void NetworkCacheStatistics::bootstrapFromNetworkCache(const String& networkCachePath)
{
    ASSERT(!RunLoop::isMain());

    LOG(NetworkCache, "(NetworkProcess) Bootstrapping the network cache statistics database from the network cache...");

    WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
    WebCore::SQLiteTransaction bootstrapTransaction(m_database);
    bootstrapTransaction.begin();

    traverseCacheFiles(networkCachePath, [this](const String& hash, const String&) {
        addHashToDatabase(hash);
    });
    bootstrapTransaction.commit();
}

void NetworkCacheStatistics::shrinkIfNeeded()
{
    ASSERT(RunLoop::isMain());
    const size_t maxEntries = 100000;

    if (m_approximateEntryCount < maxEntries)
        return;

    LOG(NetworkCache, "(NetworkProcess) shrinking statistics cache m_approximateEntryCount=%lu, maxEntries=%lu", static_cast<size_t>(m_approximateEntryCount), maxEntries);

    clear();

    StringCapture networkCachePathCapture(NetworkCache::singleton().storagePath());
    dispatch_async(m_backgroundIOQueue.get(), [this, networkCachePathCapture] {
        bootstrapFromNetworkCache(networkCachePathCapture.string());
        LOG(NetworkCache, "(NetworkProcess) statistics cache shrink completed m_approximateEntryCount=%lu", static_cast<size_t>(m_approximateEntryCount));
    });
}

void NetworkCacheStatistics::recordNotCachingResponse(const NetworkCacheKey& key, NetworkCache::StoreDecision storeDecision)
{
    ASSERT(storeDecision != NetworkCache::StoreDecision::Yes);

    StringCapture hashCapture(key.hashAsString());
    dispatch_async(m_backgroundIOQueue.get(), [this, hashCapture, storeDecision] {
        if (!m_database.isOpen())
            return;
        WebCore::SQLiteStatement statement(m_database, ASCIILiteral("INSERT OR REPLACE INTO UncachedReason (hash, reason) VALUES (?, ?)"));
        if (statement.prepare() != WebCore::SQLResultOk)
            return;

        statement.bindText(1, hashCapture.string());
        statement.bindInt(2, static_cast<int>(storeDecision));
        executeSQLStatement(statement);
    });
}

static String retrieveDecisionToDiagnosticKey(NetworkCache::RetrieveDecision retrieveDecision)
{
    switch (retrieveDecision) {
    case NetworkCache::RetrieveDecision::NoDueToProtocol:
        return WebCore::DiagnosticLoggingKeys::notHTTPFamilyKey();
    case NetworkCache::RetrieveDecision::NoDueToHTTPMethod:
        return WebCore::DiagnosticLoggingKeys::unsupportedHTTPMethodKey();
    case NetworkCache::RetrieveDecision::NoDueToConditionalRequest:
        return WebCore::DiagnosticLoggingKeys::isConditionalRequestKey();
    case NetworkCache::RetrieveDecision::NoDueToReloadIgnoringCache:
        return WebCore::DiagnosticLoggingKeys::isReloadIgnoringCacheDataKey();
    case NetworkCache::RetrieveDecision::Yes:
        ASSERT_NOT_REACHED();
        break;
    }
    return emptyString();
}

void NetworkCacheStatistics::recordNotUsingCacheForRequest(uint64_t webPageID, const NetworkCacheKey& key, const WebCore::ResourceRequest& request, NetworkCache::RetrieveDecision retrieveDecision)
{
    ASSERT(retrieveDecision != NetworkCache::RetrieveDecision::Yes);

    String hash = key.hashAsString();
    WebCore::URL requestURL = request.url();
    queryWasEverRequested(hash, [this, hash, requestURL, webPageID, retrieveDecision](bool wasEverRequested, const Optional<NetworkCache::StoreDecision>&) {
        if (wasEverRequested) {
            String diagnosticKey = retrieveDecisionToDiagnosticKey(retrieveDecision);
            LOG(NetworkCache, "(NetworkProcess) webPageID %llu: %s was previously requested but we are not using the cache, reason: %s", webPageID, requestURL.string().ascii().data(), diagnosticKey.utf8().data());
            NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::unusedKey(), diagnosticKey, WebCore::ShouldSample::Yes);
        } else
            markAsRequested(hash);
    });
}

static String storeDecisionToDiagnosticKey(NetworkCache::StoreDecision storeDecision)
{
    switch (storeDecision) {
    case NetworkCache::StoreDecision::NoDueToProtocol:
        return WebCore::DiagnosticLoggingKeys::notHTTPFamilyKey();
    case NetworkCache::StoreDecision::NoDueToHTTPMethod:
        return WebCore::DiagnosticLoggingKeys::unsupportedHTTPMethodKey();
    case NetworkCache::StoreDecision::NoDueToAttachmentResponse:
        return WebCore::DiagnosticLoggingKeys::isAttachmentKey();
    case NetworkCache::StoreDecision::NoDueToNoStoreResponse:
        return WebCore::DiagnosticLoggingKeys::cacheControlNoStoreKey();
    case NetworkCache::StoreDecision::NoDueToHTTPStatusCode:
        return WebCore::DiagnosticLoggingKeys::uncacheableStatusCodeKey();
    case NetworkCache::StoreDecision::Yes:
        // It was stored but could not be retrieved so it must have been pruned from the cache.
        return WebCore::DiagnosticLoggingKeys::noLongerInCacheKey();
    }
    return String();
}

void NetworkCacheStatistics::recordRetrievalFailure(uint64_t webPageID, const NetworkCacheKey& key, const WebCore::ResourceRequest& request)
{
    String hash = key.hashAsString();
    WebCore::URL requestURL = request.url();
    queryWasEverRequested(hash, [this, hash, requestURL, webPageID](bool wasPreviouslyRequested, const Optional<NetworkCache::StoreDecision>& storeDecision) {
        if (wasPreviouslyRequested) {
            String diagnosticKey = storeDecisionToDiagnosticKey(storeDecision.value());
            LOG(NetworkCache, "(NetworkProcess) webPageID %llu: %s was previously request but is not in the cache, reason: %s", webPageID, requestURL.string().ascii().data(), diagnosticKey.utf8().data());
            NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::notInCacheKey(), diagnosticKey, WebCore::ShouldSample::Yes);
        } else
            markAsRequested(hash);
    });
}

static String cachedEntryReuseFailureToDiagnosticKey(NetworkCache::CachedEntryReuseFailure failure)
{
    switch (failure) {
    case NetworkCache::CachedEntryReuseFailure::VaryingHeaderMismatch:
        return WebCore::DiagnosticLoggingKeys::varyingHeaderMismatchKey();
    case NetworkCache::CachedEntryReuseFailure::MissingValidatorFields:
        return WebCore::DiagnosticLoggingKeys::missingValidatorFieldsKey();
    case NetworkCache::CachedEntryReuseFailure::Other:
        return WebCore::DiagnosticLoggingKeys::otherKey();
    case NetworkCache::CachedEntryReuseFailure::None:
        ASSERT_NOT_REACHED();
        break;
    }
    return emptyString();
}

void NetworkCacheStatistics::recordRetrievedCachedEntry(uint64_t webPageID, const NetworkCacheKey& key, const WebCore::ResourceRequest& request, NetworkCache::CachedEntryReuseFailure failure)
{
    WebCore::URL requestURL = request.url();
    if (failure == NetworkCache::CachedEntryReuseFailure::None) {
        LOG(NetworkCache, "(NetworkProcess) webPageID %llu: %s is in the cache and is used", webPageID, requestURL.string().ascii().data());
        NetworkProcess::singleton().logDiagnosticMessageWithResult(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::retrievalKey(), WebCore::DiagnosticLoggingResultPass, WebCore::ShouldSample::Yes);
        return;
    }

    String diagnosticKey = cachedEntryReuseFailureToDiagnosticKey(failure);
    LOG(NetworkCache, "(NetworkProcess) webPageID %llu: %s is in the cache but wasn't used, reason: %s", webPageID, requestURL.string().ascii().data(), diagnosticKey.utf8().data());
    NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::unusableCachedEntryKey(), diagnosticKey, WebCore::ShouldSample::Yes);
}

void NetworkCacheStatistics::markAsRequested(const String& hash)
{
    ASSERT(RunLoop::isMain());

    StringCapture hashCapture(hash);
    dispatch_async(m_backgroundIOQueue.get(), [this, hashCapture] {
        WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
        if (m_database.isOpen())
            addHashToDatabase(hashCapture.string());
    });

    shrinkIfNeeded();
}

void NetworkCacheStatistics::queryWasEverRequested(const String& hash, const RequestedCompletionHandler& completionHandler)
{
    ASSERT(RunLoop::isMain());

    auto everRequestedQuery = std::make_unique<EverRequestedQuery>(EverRequestedQuery { hash, completionHandler });
    auto& query = *everRequestedQuery;
    m_activeQueries.add(WTF::move(everRequestedQuery));
    dispatch_async(m_backgroundIOQueue.get(), [this, &query] {
        WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
        bool wasAlreadyRequested = false;
        Optional<NetworkCache::StoreDecision> storeDecision;
        if (m_database.isOpen()) {
            WebCore::SQLiteStatement statement(m_database, ASCIILiteral("SELECT AlreadyRequested.hash, reason FROM AlreadyRequested LEFT OUTER JOIN UncachedReason ON AlreadyRequested.hash=UncachedReason.hash WHERE AlreadyRequested.hash=?"));
            if (statement.prepare() == WebCore::SQLResultOk) {
                statement.bindText(1, query.hash);
                if (statement.step() == WebCore::SQLResultRow) {
                    wasAlreadyRequested = true;
                    storeDecision = statement.isColumnNull(1) ? NetworkCache::StoreDecision::Yes : static_cast<NetworkCache::StoreDecision>(statement.getColumnInt(1));
                }
            }
        }
        dispatch_async(dispatch_get_main_queue(), [this, &query, wasAlreadyRequested, storeDecision] {
            query.completionHandler(wasAlreadyRequested, storeDecision);
            m_activeQueries.remove(&query);
        });
    });
}

void NetworkCacheStatistics::clear()
{
    ASSERT(RunLoop::isMain());

    dispatch_async(m_backgroundIOQueue.get(), [this] {
        if (m_database.isOpen()) {
            WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
            WebCore::SQLiteTransaction deleteTransaction(m_database);
            deleteTransaction.begin();
            executeSQLCommand(m_database, ASCIILiteral("DELETE FROM AlreadyRequested"));
            executeSQLCommand(m_database, ASCIILiteral("DELETE FROM UncachedReason"));
            deleteTransaction.commit();
            m_approximateEntryCount = 0;
        }
    });
}

void NetworkCacheStatistics::addHashToDatabase(const String& hash)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(WebCore::SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(m_database.isOpen());

    WebCore::SQLiteStatement statement(m_database, ASCIILiteral("INSERT OR IGNORE INTO AlreadyRequested (hash) VALUES (?)"));
    if (statement.prepare() != WebCore::SQLResultOk)
        return;

    statement.bindText(1, hash);
    if (!executeSQLStatement(statement))
        return;

    ++m_approximateEntryCount;
}

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE)
