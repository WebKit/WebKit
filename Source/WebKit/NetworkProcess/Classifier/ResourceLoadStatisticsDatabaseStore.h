/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(TRACKING_PREVENTION)

#include "DatabaseUtilities.h"
#include "ResourceLoadStatisticsStore.h"
#include "WebResourceLoadStatisticsStore.h"
#include <WebCore/SQLiteStatement.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/StdSet.h>
#include <wtf/Vector.h>

namespace WebCore {
enum class StorageAccessPromptWasShown : bool;
enum class StorageAccessWasGranted : bool;
struct ResourceLoadStatistics;
}

namespace WebKit {

static constexpr size_t numberOfBucketsPerStatistic = 5;
static constexpr size_t numberOfStatistics = 7;
static constexpr std::array<unsigned, numberOfBucketsPerStatistic> bucketSizes {{ 1, 3, 10, 50, 100 }};

// FIXME: ResourceLoadStatisticsDatabaseStore is the only subclass of the abstract class ResourceLoadStatisticsStore.
// There is no longer any reason to have them be separate classes.

// This is always constructed / used / destroyed on the WebResourceLoadStatisticsStore's statistics queue.
class ResourceLoadStatisticsDatabaseStore final : public ResourceLoadStatisticsStore, public DatabaseUtilities {
public:
    ResourceLoadStatisticsDatabaseStore(WebResourceLoadStatisticsStore&, SuspendableWorkQueue&, ShouldIncludeLocalhost, const String& storageDirectoryPath, PAL::SessionID);
    ~ResourceLoadStatisticsDatabaseStore();

    void mergeStatistics(Vector<ResourceLoadStatistics>&&) override;
    void clear(CompletionHandler<void()>&&) override;
    bool isEmpty() const override;

    Vector<WebResourceLoadStatisticsStore::ThirdPartyData> aggregatedThirdPartyData() const override;
    void updateCookieBlocking(CompletionHandler<void()>&&) override;

    void classifyPrevalentResources() override;
    void runIncrementalVacuumCommand();

    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, WebCore::PageIdentifier openerID, OpenerDomain&&) override;

    void grandfatherDataForDomains(const HashSet<RegistrableDomain>&) override;

    bool isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&) const override;
    bool isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&) const override;
    bool isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&) const override;

    void clearPrevalentResource(const RegistrableDomain&) override;
    void dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&&) final;
    bool isPrevalentResource(const RegistrableDomain&) const override;
    bool isVeryPrevalentResource(const RegistrableDomain&) const override;
    void setPrevalentResource(const RegistrableDomain&) override;
    void setVeryPrevalentResource(const RegistrableDomain&) override;

    void setGrandfathered(const RegistrableDomain&, bool value) override;
    bool isGrandfathered(const RegistrableDomain&) const override;

    void setMostRecentWebPushInteractionTime(const RegistrableDomain&) override;
    WallTime mostRecentWebPushInteractionTime(const RegistrableDomain&) const;
    void setIsScheduledForAllScriptWrittenStorageRemoval(const RegistrableDomain&, bool value);
    void setSubframeUnderTopFrameDomain(const SubFrameDomain&, const TopFrameDomain&) override;
    void setSubresourceUnderTopFrameDomain(const SubResourceDomain&, const TopFrameDomain&) override;
    void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectDomain&) override;
    void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectDomain&) override;
    void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectDomain&) override;
    void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectDomain&) override;

    void hasStorageAccess(SubFrameDomain&&, TopFrameDomain&&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, CompletionHandler<void(bool)>&&) override;
    void requestStorageAccess(SubFrameDomain&&, TopFrameDomain&&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::StorageAccessScope, CompletionHandler<void(StorageAccessStatus)>&&) override;
    void grantStorageAccess(SubFrameDomain&&, TopFrameDomain&&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::StorageAccessPromptWasShown, WebCore::StorageAccessScope, CompletionHandler<void(WebCore::StorageAccessWasGranted)>&&) override;

    void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser) override;
    void logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain&, const NavigatedToDomain&) override;
    void clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement(const NavigatedToDomain&, CompletionHandler<void()>&&);

    void logUserInteraction(const TopFrameDomain&, CompletionHandler<void()>&&) override;
    void clearUserInteraction(const RegistrableDomain&, CompletionHandler<void()>&&) override;
    bool hasHadUserInteraction(const RegistrableDomain&, OperatingDatesWindow) override;

    void setTimeAdvanceForTesting(Seconds) final;

    void setLastSeen(const RegistrableDomain&, Seconds) override;
    bool isCorrectSubStatisticsCount(const RegistrableDomain&, const TopFrameDomain&);
    void resourceToString(StringBuilder&, const String&) const;
    Seconds getMostRecentlyUpdatedTimestamp(const RegistrableDomain&, const TopFrameDomain&) const;
    bool isNewResourceLoadStatisticsDatabaseFile() const { return m_isNewResourceLoadStatisticsDatabaseFile; }
    void setIsNewResourceLoadStatisticsDatabaseFile(bool isNewResourceLoadStatisticsDatabaseFile) { m_isNewResourceLoadStatisticsDatabaseFile = isNewResourceLoadStatisticsDatabaseFile; }
    void removeDataForDomain(const RegistrableDomain&) override;
    Vector<RegistrableDomain> allDomains() const final;
    bool domainIDExistsInDatabase(int);
    std::optional<Vector<String>> checkForMissingTablesInSchema();
    void insertExpiredStatisticForTesting(const RegistrableDomain&, unsigned numberOfOperatingDaysPassed, bool hasUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool isPrevalent) override;
    static void interruptAllDatabases();

private:
    const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& expectedTableAndIndexQueries() final;
    Span<const ASCIILiteral> sortedTables() final;
    void includeTodayAsOperatingDateIfNecessary() override;
    void clearOperatingDates() override { }
    bool hasStatisticsExpired(WallTime mostRecentUserInteractionTime, OperatingDatesWindow) const override;
    void updateOperatingDatesParameters();

    void openITPDatabase();
    void addMissingTablesIfNecessary();
    bool missingUniqueIndices();
    bool needsUpdatedSchema() final;
    bool missingReferenceToObservedDomains();
    void migrateDataToPCMDatabaseIfNecessary();
    bool tableExists(StringView);
    void deleteTable(StringView);

    void destroyStatements() final;

    bool hasStorageAccess(const TopFrameDomain&, const SubFrameDomain&) const;
    Vector<WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty> getThirdPartyDataForSpecificFirstPartyDomains(unsigned, const RegistrableDomain&) const;
    void openAndUpdateSchemaIfNecessary();
    String getDomainStringFromDomainID(unsigned) const final;
    ASCIILiteral getSubStatisticStatement(ASCIILiteral) const;
    void appendSubStatisticList(StringBuilder&, ASCIILiteral tableName, const String& domain) const;
    void mergeStatistic(const ResourceLoadStatistics&);
    void merge(WebCore::SQLiteStatement*, const ResourceLoadStatistics&);
    void clearDatabaseContents();
    bool insertObservedDomain(const ResourceLoadStatistics&) WARN_UNUSED_RETURN;
    void insertDomainRelationships(const ResourceLoadStatistics&);
    void insertDomainRelationshipList(const String&, const HashSet<RegistrableDomain>&, unsigned);
    bool relationshipExists(WebCore::SQLiteStatementAutoResetScope&, std::optional<unsigned> firstDomainID, const RegistrableDomain& secondDomain) const;
    std::optional<unsigned> domainID(const RegistrableDomain&) const;
    bool domainExists(const RegistrableDomain&) const;
    void updateLastSeen(const RegistrableDomain&, WallTime);
    void updateDataRecordsRemoved(const RegistrableDomain&, int);
    void setUserInteraction(const RegistrableDomain&, bool hadUserInteraction, WallTime);
    Vector<RegistrableDomain> domainsToBlockAndDeleteCookiesFor() const;
    Vector<RegistrableDomain> domainsToBlockButKeepCookiesFor() const;
    Vector<RegistrableDomain> domainsWithUserInteractionAsFirstParty() const;
    HashMap<TopFrameDomain, SubResourceDomain> domainsWithStorageAccess() const;

    struct DomainData {
        unsigned domainID;
        RegistrableDomain registrableDomain;
        WallTime mostRecentUserInteractionTime;
        WallTime mostRecentWebPushInteractionTime;
        bool hadUserInteraction;
        bool grandfathered;
        bool isScheduledForAllButCookieDataRemoval;
        unsigned topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement;
    };
    Vector<DomainData> domains() const;
    bool hasHadRecentWebPushInteraction(const DomainData&) const;
    bool hasHadUnexpiredRecentUserInteraction(const DomainData&, OperatingDatesWindow);
    void clearGrandfathering(Vector<unsigned>&&);
    WebCore::StorageAccessPromptWasShown hasUserGrantedStorageAccessThroughPrompt(unsigned domainID, const RegistrableDomain&);
    void incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&&) override;

    void reclassifyResources();
    struct NotVeryPrevalentResources {
        RegistrableDomain registrableDomain;
        ResourceLoadPrevalence prevalence;
        unsigned subresourceUnderTopFrameDomainsCount;
        unsigned subresourceUniqueRedirectsToCount;
        unsigned subframeUnderTopFrameDomainsCount;
        unsigned topFrameUniqueRedirectsToCount;
    };
    HashMap<unsigned, NotVeryPrevalentResources> findNotVeryPrevalentResources();

    bool predicateValueForDomain(WebCore::SQLiteStatementAutoResetScope&, const RegistrableDomain&) const;

    bool areAllThirdPartyCookiesBlockedUnder(const TopFrameDomain&) override;
    CookieAccess cookieAccess(const SubResourceDomain&, const TopFrameDomain&);

    void setPrevalentResource(const RegistrableDomain&, ResourceLoadPrevalence);
    unsigned recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(unsigned primaryDomainID, StdSet<unsigned>& nonPrevalentRedirectionSources, unsigned numberOfRecursiveCalls);
    void setDomainsAsPrevalent(StdSet<unsigned>&&);
    void grantStorageAccessInternal(SubFrameDomain&&, TopFrameDomain&&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, WebCore::StorageAccessPromptWasShown, WebCore::StorageAccessScope, CompletionHandler<void(WebCore::StorageAccessWasGranted)>&&);
    void markAsPrevalentIfHasRedirectedToPrevalent();
    Vector<RegistrableDomain> ensurePrevalentResourcesForDebugMode() override;
    void removeDataRecords(CompletionHandler<void()>&&);
    void pruneStatisticsIfNeeded() override;
    enum class AddedRecord { No, Yes };
    std::pair<AddedRecord, std::optional<unsigned>> ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain&) WARN_UNUSED_RETURN;
    bool shouldRemoveAllWebsiteDataFor(const DomainData&, bool shouldCheckForGrandfathering);
    bool shouldRemoveAllButCookiesFor(const DomainData&, bool shouldCheckForGrandfathering);
    bool shouldEnforceSameSiteStrictFor(DomainData&, bool shouldCheckForGrandfathering);
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor registrableDomainsToDeleteOrRestrictWebsiteDataFor() override;
    bool isDatabaseStore() const final { return true; }

    bool createUniqueIndices() final;
    bool createSchema() final;
    String ensureAndMakeDomainList(const HashSet<RegistrableDomain>&);
    std::optional<WallTime> mostRecentUserInteractionTime(const DomainData&);

    mutable std::unique_ptr<WebCore::SQLiteStatement> m_observedDomainCountStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_insertObservedDomainStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_insertTopLevelDomainStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_domainIDFromStringStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_topFrameLinkDecorationsFromExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_topFrameLoadedThirdPartyScriptsExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_subframeUnderTopFrameDomainExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_subresourceUnderTopFrameDomainExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_subresourceUniqueRedirectsToExistsStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_mostRecentUserInteractionStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updateLastSeenStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_updateDataRecordsRemovedStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updatePrevalentResourceStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_isPrevalentResourceStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updateVeryPrevalentResourceStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_isVeryPrevalentResourceStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_clearPrevalentResourceStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_hadUserInteractionStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_updateGrandfatheredStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_updateIsScheduledForAllButCookieDataRemovalStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_updateMostRecentWebPushInteractionTimeStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_mostRecentWebPushInteractionTimeStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_isGrandfatheredStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_countPrevalentResourcesStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_countPrevalentResourcesWithUserInteractionStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_countPrevalentResourcesWithoutUserInteractionStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_getResourceDataByDomainNameStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_getAllDomainsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_domainStringFromDomainIDStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_getAllSubStatisticsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_storageAccessExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_getMostRecentlyUpdatedTimestampStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_linkDecorationExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_scriptLoadExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_subFrameExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_subResourceExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_uniqueRedirectExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_observedDomainsExistsStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_removeAllDataStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_checkIfTableExistsStatement;

    PAL::SessionID m_sessionID;
    bool m_isNewResourceLoadStatisticsDatabaseFile { false };
    unsigned m_operatingDatesSize { 0 };
    std::optional<OperatingDate> m_longWindowOperatingDate;
    std::optional<OperatingDate> m_shortWindowOperatingDate;
    OperatingDate m_mostRecentOperatingDate;
};

} // namespace WebKit

// FIXME: Remove this. All ResourceLoadStatisticsStores are ResourceLoadStatisticsDatabaseStores. See FIXME above.
SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::ResourceLoadStatisticsDatabaseStore)
    static bool isType(const WebKit::ResourceLoadStatisticsStore& store) { return store.isDatabaseStore(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif
