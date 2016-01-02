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
#include "NetworkCacheFileSystem.h"
#include "NetworkProcess.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/DiagnosticLoggingResultType.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SQLiteDatabaseTracker.h>
#include <WebCore/SQLiteStatement.h>
#include <WebCore/SQLiteTransaction.h>
#include <wtf/RunLoop.h>

namespace WebKit {
namespace NetworkCache {

static const char* StatisticsDatabaseName = "WebKitCacheStatistics.db";
static const std::chrono::milliseconds mininumWriteInterval = std::chrono::milliseconds(10000);

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

    if (statement.step() != SQLITE_DONE) {
        LOG_ERROR("Network cache statistics: failed to execute statement \"%s\" error \"%s\"", statement.query().utf8().data(), statement.database().lastErrorMsg());
        return false;
    }

    return true;
}

std::unique_ptr<Statistics> Statistics::open(const String& cachePath)
{
    ASSERT(RunLoop::isMain());

    String databasePath = WebCore::pathByAppendingComponent(cachePath, StatisticsDatabaseName);
    return std::unique_ptr<Statistics>(new Statistics(databasePath));
}

Statistics::Statistics(const String& databasePath)
    : m_serialBackgroundIOQueue(WorkQueue::create("com.apple.WebKit.Cache.Statistics.Background", WorkQueue::Type::Serial, WorkQueue::QOS::Background))
    , m_writeTimer(*this, &Statistics::writeTimerFired)
{
    initialize(databasePath);
}

void Statistics::initialize(const String& databasePath)
{
    ASSERT(RunLoop::isMain());

    auto startTime = std::chrono::system_clock::now();

    StringCapture databasePathCapture(databasePath);
    StringCapture networkCachePathCapture(singleton().recordsPath());
    serialBackgroundIOQueue().dispatch([this, databasePathCapture, networkCachePathCapture, startTime] {
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
        if (statement.prepareAndStep() != SQLITE_ROW) {
            LOG_ERROR("Network cache statistics: Failed to count the number of rows in AlreadyRequested table");
            return;
        }

        m_approximateEntryCount = statement.getColumnInt(0);

#if !LOG_DISABLED
        auto elapsedMS = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count());
#endif
        LOG(NetworkCache, "(NetworkProcess) Network cache statistics database load complete, entries=%lu time=%" PRIi64 "ms", static_cast<size_t>(m_approximateEntryCount), elapsedMS);

        if (!m_approximateEntryCount) {
            bootstrapFromNetworkCache(networkCachePathCapture.string());
#if !LOG_DISABLED
            elapsedMS = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count());
#endif
            LOG(NetworkCache, "(NetworkProcess) Network cache statistics database bootstrapping complete, entries=%lu time=%" PRIi64 "ms", static_cast<size_t>(m_approximateEntryCount), elapsedMS);
        }
    });
}

void Statistics::bootstrapFromNetworkCache(const String& networkCachePath)
{
    ASSERT(!RunLoop::isMain());

    LOG(NetworkCache, "(NetworkProcess) Bootstrapping the network cache statistics database from the network cache...");

    Vector<StringCapture> hashes;
    traverseRecordsFiles(networkCachePath, ASCIILiteral("resource"), [&hashes](const String& fileName, const String& hashString, const String& type, bool isBodyBlob, const String& recordDirectoryPath) {
        if (isBodyBlob)
            return;

        Key::HashType hash;
        if (!Key::stringToHash(hashString, hash))
            return;

        hashes.append(hashString);
    });

    WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
    WebCore::SQLiteTransaction writeTransaction(m_database);
    writeTransaction.begin();

    addHashesToDatabase(hashes);

    writeTransaction.commit();
}

void Statistics::shrinkIfNeeded()
{
    ASSERT(RunLoop::isMain());
    const size_t maxEntries = 100000;

    if (m_approximateEntryCount < maxEntries)
        return;

    LOG(NetworkCache, "(NetworkProcess) shrinking statistics cache m_approximateEntryCount=%lu, maxEntries=%lu", static_cast<size_t>(m_approximateEntryCount), maxEntries);

    clear();

    StringCapture networkCachePathCapture(singleton().recordsPath());
    serialBackgroundIOQueue().dispatch([this, networkCachePathCapture] {
        bootstrapFromNetworkCache(networkCachePathCapture.string());
        LOG(NetworkCache, "(NetworkProcess) statistics cache shrink completed m_approximateEntryCount=%lu", static_cast<size_t>(m_approximateEntryCount));
    });
}

void Statistics::recordRetrievalRequest(uint64_t webPageID)
{
    NetworkProcess::singleton().logDiagnosticMessage(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::retrievalRequestKey(), WebCore::ShouldSample::Yes);
}

void Statistics::recordNotCachingResponse(const Key& key, StoreDecision storeDecision)
{
    ASSERT(storeDecision != StoreDecision::Yes);

    m_storeDecisionsToAdd.set(key.hashAsString(), storeDecision);
    if (!m_writeTimer.isActive())
        m_writeTimer.startOneShot(mininumWriteInterval);
}

static String retrieveDecisionToDiagnosticKey(RetrieveDecision retrieveDecision)
{
    switch (retrieveDecision) {
    case RetrieveDecision::NoDueToHTTPMethod:
        return WebCore::DiagnosticLoggingKeys::unsupportedHTTPMethodKey();
    case RetrieveDecision::NoDueToConditionalRequest:
        return WebCore::DiagnosticLoggingKeys::isConditionalRequestKey();
    case RetrieveDecision::NoDueToReloadIgnoringCache:
        return WebCore::DiagnosticLoggingKeys::isReloadIgnoringCacheDataKey();
    case RetrieveDecision::Yes:
        ASSERT_NOT_REACHED();
        break;
    }
    return emptyString();
}

void Statistics::recordNotUsingCacheForRequest(uint64_t webPageID, const Key& key, const WebCore::ResourceRequest& request, RetrieveDecision retrieveDecision)
{
    ASSERT(retrieveDecision != RetrieveDecision::Yes);

    String hash = key.hashAsString();
    WebCore::URL requestURL = request.url();
    queryWasEverRequested(hash, NeedUncachedReason::No, [this, hash, requestURL, webPageID, retrieveDecision](bool wasEverRequested, const Optional<StoreDecision>&) {
        if (wasEverRequested) {
            String diagnosticKey = retrieveDecisionToDiagnosticKey(retrieveDecision);
            LOG(NetworkCache, "(NetworkProcess) webPageID %" PRIu64 ": %s was previously requested but we are not using the cache, reason: %s", webPageID, requestURL.string().ascii().data(), diagnosticKey.utf8().data());
            NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::unusedKey(), diagnosticKey, WebCore::ShouldSample::Yes);
        } else {
            NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::requestKey(), WebCore::DiagnosticLoggingKeys::neverSeenBeforeKey(), WebCore::ShouldSample::Yes);
            markAsRequested(hash);
        }
    });
}

static String storeDecisionToDiagnosticKey(StoreDecision storeDecision)
{
    switch (storeDecision) {
    case StoreDecision::NoDueToProtocol:
        return WebCore::DiagnosticLoggingKeys::notHTTPFamilyKey();
    case StoreDecision::NoDueToHTTPMethod:
        return WebCore::DiagnosticLoggingKeys::unsupportedHTTPMethodKey();
    case StoreDecision::NoDueToAttachmentResponse:
        return WebCore::DiagnosticLoggingKeys::isAttachmentKey();
    case StoreDecision::NoDueToNoStoreResponse:
    case StoreDecision::NoDueToNoStoreRequest:
        return WebCore::DiagnosticLoggingKeys::cacheControlNoStoreKey();
    case StoreDecision::NoDueToHTTPStatusCode:
        return WebCore::DiagnosticLoggingKeys::uncacheableStatusCodeKey();
    case StoreDecision::NoDueToUnlikelyToReuse:
        return WebCore::DiagnosticLoggingKeys::unlikelyToReuseKey();
    case StoreDecision::NoDueToStreamingMedia:
        return WebCore::DiagnosticLoggingKeys::streamingMedia();
    case StoreDecision::Yes:
        // It was stored but could not be retrieved so it must have been pruned from the cache.
        return WebCore::DiagnosticLoggingKeys::noLongerInCacheKey();
    }
    return String();
}

void Statistics::recordRetrievalFailure(uint64_t webPageID, const Key& key, const WebCore::ResourceRequest& request)
{
    String hash = key.hashAsString();
    WebCore::URL requestURL = request.url();
    queryWasEverRequested(hash, NeedUncachedReason::Yes, [this, hash, requestURL, webPageID](bool wasPreviouslyRequested, const Optional<StoreDecision>& storeDecision) {
        if (wasPreviouslyRequested) {
            String diagnosticKey = storeDecisionToDiagnosticKey(storeDecision.value());
            LOG(NetworkCache, "(NetworkProcess) webPageID %" PRIu64 ": %s was previously request but is not in the cache, reason: %s", webPageID, requestURL.string().ascii().data(), diagnosticKey.utf8().data());
            NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::notInCacheKey(), diagnosticKey, WebCore::ShouldSample::Yes);
        } else {
            NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::requestKey(), WebCore::DiagnosticLoggingKeys::neverSeenBeforeKey(), WebCore::ShouldSample::Yes);
            markAsRequested(hash);
        }
    });
}

static String cachedEntryReuseFailureToDiagnosticKey(UseDecision decision)
{
    switch (decision) {
    case UseDecision::NoDueToVaryingHeaderMismatch:
        return WebCore::DiagnosticLoggingKeys::varyingHeaderMismatchKey();
    case UseDecision::NoDueToMissingValidatorFields:
        return WebCore::DiagnosticLoggingKeys::missingValidatorFieldsKey();
    case UseDecision::NoDueToDecodeFailure:
    case UseDecision::NoDueToExpiredRedirect:
        return WebCore::DiagnosticLoggingKeys::otherKey();
    case UseDecision::Use:
    case UseDecision::Validate:
        ASSERT_NOT_REACHED();
        break;
    }
    return emptyString();
}

void Statistics::recordRetrievedCachedEntry(uint64_t webPageID, const Key& key, const WebCore::ResourceRequest& request, UseDecision decision)
{
    WebCore::URL requestURL = request.url();
    if (decision == UseDecision::Use) {
        LOG(NetworkCache, "(NetworkProcess) webPageID %" PRIu64 ": %s is in the cache and is used", webPageID, requestURL.string().ascii().data());
        NetworkProcess::singleton().logDiagnosticMessageWithResult(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::retrievalKey(), WebCore::DiagnosticLoggingResultPass, WebCore::ShouldSample::Yes);
        return;
    }

    if (decision == UseDecision::Validate) {
        LOG(NetworkCache, "(NetworkProcess) webPageID %" PRIu64 ": %s is in the cache but needs revalidation", webPageID, requestURL.string().ascii().data());
        NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::retrievalKey(), WebCore::DiagnosticLoggingKeys::needsRevalidationKey(), WebCore::ShouldSample::Yes);
        return;
    }

    String diagnosticKey = cachedEntryReuseFailureToDiagnosticKey(decision);
    LOG(NetworkCache, "(NetworkProcess) webPageID %" PRIu64 ": %s is in the cache but wasn't used, reason: %s", webPageID, requestURL.string().ascii().data(), diagnosticKey.utf8().data());
    NetworkProcess::singleton().logDiagnosticMessageWithValue(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::unusableCachedEntryKey(), diagnosticKey, WebCore::ShouldSample::Yes);
}

void Statistics::recordRevalidationSuccess(uint64_t webPageID, const Key& key, const WebCore::ResourceRequest& request)
{
    WebCore::URL requestURL = request.url();
    LOG(NetworkCache, "(NetworkProcess) webPageID %" PRIu64 ": %s was successfully revalidated", webPageID, requestURL.string().ascii().data());

    NetworkProcess::singleton().logDiagnosticMessageWithResult(webPageID, WebCore::DiagnosticLoggingKeys::networkCacheKey(), WebCore::DiagnosticLoggingKeys::revalidatingKey(), WebCore::DiagnosticLoggingResultPass, WebCore::ShouldSample::Yes);
}

void Statistics::markAsRequested(const String& hash)
{
    ASSERT(RunLoop::isMain());

    m_hashesToAdd.add(hash);
    if (!m_writeTimer.isActive())
        m_writeTimer.startOneShot(mininumWriteInterval);
}

void Statistics::writeTimerFired()
{
    ASSERT(RunLoop::isMain());

    Vector<StringCapture> hashesToAdd;
    copyToVector(m_hashesToAdd, hashesToAdd);
    m_hashesToAdd.clear();

    Vector<std::pair<StringCapture, StoreDecision>> storeDecisionsToAdd;
    copyToVector(m_storeDecisionsToAdd, storeDecisionsToAdd);
    m_storeDecisionsToAdd.clear();

    shrinkIfNeeded();

    serialBackgroundIOQueue().dispatch([this, hashesToAdd, storeDecisionsToAdd] {
        if (!m_database.isOpen())
            return;

        WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
        WebCore::SQLiteTransaction writeTransaction(m_database);
        writeTransaction.begin();

        addHashesToDatabase(hashesToAdd);
        addStoreDecisionsToDatabase(storeDecisionsToAdd);

        writeTransaction.commit();
    });
}

void Statistics::queryWasEverRequested(const String& hash, NeedUncachedReason needUncachedReason, const RequestedCompletionHandler& completionHandler)
{
    ASSERT(RunLoop::isMain());

    // Query pending writes first.
    bool wasAlreadyRequested = m_hashesToAdd.contains(hash);
    if (wasAlreadyRequested && needUncachedReason == NeedUncachedReason::No) {
        completionHandler(true, Nullopt);
        return;
    }
    if (needUncachedReason == NeedUncachedReason::Yes && m_storeDecisionsToAdd.contains(hash)) {
        completionHandler(true, m_storeDecisionsToAdd.get(hash));
        return;
    }

    // Query the database.
    auto everRequestedQuery = std::make_unique<EverRequestedQuery>(EverRequestedQuery { hash, needUncachedReason == NeedUncachedReason::Yes, completionHandler });
    auto& query = *everRequestedQuery;
    m_activeQueries.add(WTFMove(everRequestedQuery));
    serialBackgroundIOQueue().dispatch([this, wasAlreadyRequested, &query] () mutable {
        WebCore::SQLiteTransactionInProgressAutoCounter transactionCounter;
        Optional<StoreDecision> storeDecision;
        if (m_database.isOpen()) {
            if (!wasAlreadyRequested) {
                WebCore::SQLiteStatement statement(m_database, ASCIILiteral("SELECT hash FROM AlreadyRequested WHERE hash=?"));
                if (statement.prepare() == SQLITE_OK) {
                    statement.bindText(1, query.hash);
                    wasAlreadyRequested = (statement.step() == SQLITE_ROW);
                }
            }
            if (wasAlreadyRequested && query.needUncachedReason) {
                WebCore::SQLiteStatement statement(m_database, ASCIILiteral("SELECT reason FROM UncachedReason WHERE hash=?"));
                storeDecision = StoreDecision::Yes;
                if (statement.prepare() == SQLITE_OK) {
                    statement.bindText(1, query.hash);
                    if (statement.step() == SQLITE_ROW)
                        storeDecision = static_cast<StoreDecision>(statement.getColumnInt(0));
                }
            }
        }
        RunLoop::main().dispatch([this, &query, wasAlreadyRequested, storeDecision] {
            query.completionHandler(wasAlreadyRequested, storeDecision);
            m_activeQueries.remove(&query);
        });
    });
}

void Statistics::clear()
{
    ASSERT(RunLoop::isMain());

    serialBackgroundIOQueue().dispatch([this] {
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

void Statistics::addHashesToDatabase(const Vector<StringCapture>& hashes)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(WebCore::SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(m_database.isOpen());

    WebCore::SQLiteStatement statement(m_database, ASCIILiteral("INSERT OR IGNORE INTO AlreadyRequested (hash) VALUES (?)"));
    if (statement.prepare() != SQLITE_OK)
        return;

    for (auto& hash : hashes) {
        statement.bindText(1, hash.string());
        if (executeSQLStatement(statement))
            ++m_approximateEntryCount;
        statement.reset();
    }
}

void Statistics::addStoreDecisionsToDatabase(const Vector<std::pair<StringCapture, StoreDecision>>& storeDecisions)
{
    ASSERT(!RunLoop::isMain());
    ASSERT(WebCore::SQLiteDatabaseTracker::hasTransactionInProgress());
    ASSERT(m_database.isOpen());

    WebCore::SQLiteStatement statement(m_database, ASCIILiteral("INSERT OR REPLACE INTO UncachedReason (hash, reason) VALUES (?, ?)"));
    if (statement.prepare() != SQLITE_OK)
        return;

    for (auto& pair : storeDecisions) {
        statement.bindText(1, pair.first.string());
        statement.bindInt(2, static_cast<int>(pair.second));
        executeSQLStatement(statement);
        statement.reset();
    }
}

}
}

#endif // ENABLE(NETWORK_CACHE)
