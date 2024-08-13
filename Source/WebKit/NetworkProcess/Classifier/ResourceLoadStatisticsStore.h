/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "DatabaseUtilities.h"
#include "ResourceLoadStatisticsClassifier.h"
#include "WebResourceLoadStatisticsStore.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/FrameIdentifier.h>
#include <pal/SessionID.h>
#include <wtf/Forward.h>
#include <wtf/StdSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if HAVE(CORE_PREDICTION)
#include "ResourceLoadStatisticsClassifierCocoa.h"
#endif

namespace WebKit {
class ResourceLoadStatisticsStore;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebKit::ResourceLoadStatisticsStore> : std::true_type { };
}

namespace WebCore {
class KeyedDecoder;
class KeyedEncoder;
class SQLiteStatement;
enum class StorageAccessPromptWasShown : bool;
enum class StorageAccessWasGranted : uint8_t;
struct ResourceLoadStatistics;
}

namespace WebKit {

enum class DidFilterKnownLinkDecoration : bool;

class OperatingDate {
public:
    OperatingDate() = default;
    
    static OperatingDate fromWallTime(WallTime);
    static OperatingDate today(Seconds timeAdvanceForTesting);
    Seconds secondsSinceEpoch() const;
    int year() const { return m_year; }
    int month() const { return m_month; }
    int monthDay() const { return m_monthDay; }

    friend bool operator==(const OperatingDate&, const OperatingDate&) = default;
    bool operator<(const OperatingDate& other) const;
    bool operator<=(const OperatingDate& other) const;
    
    OperatingDate(int year, int month, int monthDay)
        : m_year(year)
        , m_month(month)
        , m_monthDay(monthDay)
    { }

private:
    int m_year { 0 };
    int m_month { 0 }; // [0, 11].
    int m_monthDay { 0 }; // [1, 31].
};

enum class OperatingDatesWindow : uint8_t { Long, Short, ForLiveOnTesting, ForReproTesting };
enum class CookieAccess : uint8_t { CannotRequest, BasedOnCookiePolicy, OnlyIfGranted };
enum class CanRequestStorageAccessWithoutUserInteraction : bool { No, Yes };
enum class DataRemovalFrequency : uint8_t { Never, Short, Long };

// This is always constructed / used / destroyed on the WebResourceLoadStatisticsStore's statistics queue.
class ResourceLoadStatisticsStore final : public DatabaseUtilities, public CanMakeWeakPtr<ResourceLoadStatisticsStore> {
    WTF_MAKE_TZONE_ALLOCATED(ResourceLoadStatisticsStore);
public:
    using ResourceLoadStatistics = WebCore::ResourceLoadStatistics;
    using RegistrableDomain = WebCore::RegistrableDomain;
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;
    using RedirectDomain = WebCore::RegistrableDomain;
    using RedirectedFromDomain = WebCore::RegistrableDomain;
    using RedirectedToDomain = WebCore::RegistrableDomain;
    using NavigatedFromDomain = WebCore::RegistrableDomain;
    using NavigatedToDomain = WebCore::RegistrableDomain;
    using DomainInNeedOfStorageAccess = WebCore::RegistrableDomain;
    using OpenerDomain = WebCore::RegistrableDomain;
    
    ResourceLoadStatisticsStore(WebResourceLoadStatisticsStore&, SuspendableWorkQueue&, ShouldIncludeLocalhost, const String& storageDirectoryPath, PAL::SessionID);
    virtual ~ResourceLoadStatisticsStore();

    void clear(CompletionHandler<void()>&&);
    bool isEmpty() const;
    Vector<ITPThirdPartyData> aggregatedThirdPartyData() const;
    void updateCookieBlocking(CompletionHandler<void()>&&);
    void processStatisticsAndDataRecords();
    void cancelPendingStatisticsProcessingRequest();
    void mergeStatistics(Vector<ResourceLoadStatistics>&&);
    void runIncrementalVacuumCommand();
    void dumpResourceLoadStatistics(CompletionHandler<void(String&&)>&&);
    bool isNewResourceLoadStatisticsDatabaseFile() const { return m_isNewResourceLoadStatisticsDatabaseFile; }
    void setIsNewResourceLoadStatisticsDatabaseFile(bool isNewResourceLoadStatisticsDatabaseFile) { m_isNewResourceLoadStatisticsDatabaseFile = isNewResourceLoadStatisticsDatabaseFile; }

    void requestStorageAccessUnderOpener(DomainInNeedOfStorageAccess&&, WebCore::PageIdentifier openerID, OpenerDomain&&, CanRequestStorageAccessWithoutUserInteraction);
    void removeAllStorageAccess(CompletionHandler<void()>&&);

    void grandfatherExistingWebsiteData(CompletionHandler<void()>&&);
    void setGrandfathered(const RegistrableDomain&, bool value);
    bool isGrandfathered(const RegistrableDomain&) const;
    void setGrandfatheringTime(Seconds);

    bool isRegisteredAsSubresourceUnder(const SubResourceDomain&, const TopFrameDomain&) const;
    bool isRegisteredAsSubFrameUnder(const SubFrameDomain&, const TopFrameDomain&) const;
    bool isRegisteredAsRedirectingTo(const RedirectedFromDomain&, const RedirectedToDomain&) const;

    void clearPrevalentResource(const RegistrableDomain&);
    bool isPrevalentResource(const RegistrableDomain&) const;
    bool isVeryPrevalentResource(const RegistrableDomain&) const;
    void setPrevalentResource(const RegistrableDomain&);
    void setVeryPrevalentResource(const RegistrableDomain&);
    void setMostRecentWebPushInteractionTime(const RegistrableDomain&);

    void setSubframeUnderTopFrameDomain(const SubFrameDomain&, const TopFrameDomain&);
    void setSubresourceUnderTopFrameDomain(const SubResourceDomain&, const TopFrameDomain&);
    void setSubresourceUniqueRedirectTo(const SubResourceDomain&, const RedirectDomain&);
    void setSubresourceUniqueRedirectFrom(const SubResourceDomain&, const RedirectDomain&);
    void setTopFrameUniqueRedirectTo(const TopFrameDomain&, const RedirectDomain&);
    void setTopFrameUniqueRedirectFrom(const TopFrameDomain&, const RedirectDomain&);

    void setIsRunningTest(bool);
    void logTestingEvent(String&&);
    void setTimeAdvanceForTesting(Seconds);

    void setMaxStatisticsEntries(size_t maximumEntryCount);
    void setPruneEntriesDownTo(size_t pruneTargetCount);
    void resetParametersToDefaultValues();

    void setNotifyPagesWhenDataRecordsWereScanned(bool);
    bool shouldSkip(const RegistrableDomain&) const;
    void setShouldClassifyResourcesBeforeDataRecordsRemoval(bool);
    void setTimeToLiveUserInteraction(Seconds);
    void setMinimumTimeBetweenDataRecordsRemoval(Seconds);
    void setResourceLoadStatisticsDebugMode(bool);
    bool isDebugModeEnabled() const { return m_debugModeEnabled; };
    void setPrevalentResourceForDebugMode(const RegistrableDomain&);
    void setThirdPartyCookieBlockingMode(WebCore::ThirdPartyCookieBlockingMode mode) { m_thirdPartyCookieBlockingMode = mode; };
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode() const { return m_thirdPartyCookieBlockingMode; };
    void setSameSiteStrictEnforcementEnabled(WebCore::SameSiteStrictEnforcementEnabled enabled) { m_sameSiteStrictEnforcementEnabled = enabled; };
    bool isSameSiteStrictEnforcementEnabled() const { return m_sameSiteStrictEnforcementEnabled == WebCore::SameSiteStrictEnforcementEnabled::Yes; };
    void setFirstPartyWebsiteDataRemovalMode(WebCore::FirstPartyWebsiteDataRemovalMode mode) { m_firstPartyWebsiteDataRemovalMode = mode; }
    WebCore::FirstPartyWebsiteDataRemovalMode firstPartyWebsiteDataRemovalMode() const { return m_firstPartyWebsiteDataRemovalMode; }
    void setPersistedDomains(HashSet<RegistrableDomain>&& domains) { m_persistedDomains = WTFMove(domains); }
    void setStandaloneApplicationDomain(RegistrableDomain&& domain) { m_standaloneApplicationDomain = WTFMove(domain); }
#if ENABLE(APP_BOUND_DOMAINS)
    void setAppBoundDomains(HashSet<RegistrableDomain>&&);
#endif
#if ENABLE(MANAGED_DOMAINS)
    void setManagedDomains(HashSet<RegistrableDomain>&&);
#endif

    void hasStorageAccess(SubFrameDomain&&, TopFrameDomain&&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, CanRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(SubFrameDomain&&, TopFrameDomain&&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::StorageAccessScope, CanRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(StorageAccessStatus)>&&);
    void grantStorageAccess(SubFrameDomain&&, TopFrameDomain&&, WebCore::FrameIdentifier, WebCore::PageIdentifier, WebCore::StorageAccessPromptWasShown, WebCore::StorageAccessScope, CompletionHandler<void(WebCore::StorageAccessWasGranted)>&&);

    void logFrameNavigation(const NavigatedToDomain&, const TopFrameDomain&, const NavigatedFromDomain&, bool isRedirect, bool isMainFrame, Seconds delayAfterMainFrameDocumentLoad, bool wasPotentiallyInitiatedByUser);
    void logCrossSiteLoadWithLinkDecoration(const NavigatedFromDomain&, const NavigatedToDomain&, DidFilterKnownLinkDecoration);

    void logUserInteraction(const TopFrameDomain&, CompletionHandler<void()>&&);
    void clearUserInteraction(const RegistrableDomain&, CompletionHandler<void()>&&);
    bool hasHadUserInteraction(const RegistrableDomain&, OperatingDatesWindow);

    void setLastSeen(const RegistrableDomain& primaryDomain, Seconds);
    bool isCorrectSubStatisticsCount(const RegistrableDomain&, const TopFrameDomain&);

    void removeDataForDomain(const RegistrableDomain&);

    Vector<RegistrableDomain> allDomains() const;
    HashMap<RegistrableDomain, WallTime> allDomainsWithLastAccessedTime() const;
    HashSet<RegistrableDomain> domainsExemptFromWebsiteDataDeletion() const;

    void didCreateNetworkProcess();

    const WebResourceLoadStatisticsStore& store() const { return m_store; }

    bool domainIDExistsInDatabase(int);
    bool observedDomainNavigationWithLinkDecoration(int);
    std::optional<Vector<String>> checkForMissingTablesInSchema();

    void includeTodayAsOperatingDateIfNecessary();
    void insertExpiredStatisticForTesting(const RegistrableDomain&, unsigned numberOfOperatingDaysPassed, bool hasUserInteraction, bool isScheduledForAllButCookieDataRemoval, bool);
    static void interruptAllDatabases();

private:
    WebResourceLoadStatisticsStore& store() { return m_store; }

    struct Parameters {
        size_t pruneEntriesDownTo { 800 };
        size_t maxStatisticsEntries { 1000 };
        std::optional<Seconds> timeToLiveUserInteraction;
        Seconds minimumTimeBetweenDataRecordsRemoval { 1_h };
        Seconds grandfatheringTime { 24_h * 7 };
        Seconds cacheMaxAgeCapTime { 24_h * 7 };
        Seconds clientSideCookiesAgeCapTime { 24_h * 7 };
        Seconds clientSideCookiesForLinkDecorationTargetPageAgeCapTime { 24_h };
        Seconds minDelayAfterMainFrameDocumentLoadToNotBeARedirect { 5_s };
        size_t minimumTopFrameRedirectsForSameSiteStrictEnforcement { 10 };
        bool shouldNotifyPagesWhenDataRecordsWereScanned { false };
        bool shouldClassifyResourcesBeforeDataRecordsRemoval { true };
        bool isRunningTest { false };
    };
    const Parameters& parameters() const { return m_parameters; }
    WallTime& endOfGrandfatheringTimestamp() { return m_endOfGrandfatheringTimestamp; }
    const WallTime& endOfGrandfatheringTimestamp() const { return m_endOfGrandfatheringTimestamp; }
    void clearEndOfGrandfatheringTimeStamp() { m_endOfGrandfatheringTimestamp = { }; }
    const RegistrableDomain& debugManualPrevalentResource() const { return m_debugManualPrevalentResource; }
    const RegistrableDomain& debugStaticPrevalentResource() const { return m_debugStaticPrevalentResource; }
    void debugBroadcastConsoleMessage(MessageSource, MessageLevel, const String& message);
    void debugLogDomainsInBatches(ASCIILiteral action, const RegistrableDomainsToBlockCookiesFor&);
    bool debugLoggingEnabled() const { return m_debugLoggingEnabled; }
    bool debugModeEnabled() const { return m_debugModeEnabled; }
    bool shouldExemptFromWebsiteDataDeletion(const RegistrableDomain&) const;

    bool hasStorageAccess(const TopFrameDomain&, const SubFrameDomain&) const;
    Vector<ITPThirdPartyDataForSpecificFirstParty> getThirdPartyDataForSpecificFirstPartyDomains(unsigned, const RegistrableDomain&) const;
    String getDomainStringFromDomainID(unsigned) const final;
    void resourceToString(StringBuilder&, const String&) const;
    ASCIILiteral getSubStatisticStatement(ASCIILiteral) const;
    void appendSubStatisticList(StringBuilder&, ASCIILiteral tableName, const String& domain) const;
    void mergeStatistic(const ResourceLoadStatistics&);
    void merge(WebCore::SQLiteStatement*, const ResourceLoadStatistics&);
    void incrementRecordsDeletedCountForDomains(HashSet<RegistrableDomain>&&);
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
    HashMap<TopFrameDomain, Vector<SubResourceDomain>> domainsWithStorageAccess() const;

    struct DomainData {
        unsigned domainID;
        RegistrableDomain registrableDomain;
        WallTime mostRecentUserInteractionTime;
        WallTime mostRecentWebPushInteractionTime;
        bool hadUserInteraction;
        bool grandfathered;
        DataRemovalFrequency dataRemovalFrequency;
        unsigned topFrameUniqueRedirectsToSinceSameSiteStrictEnforcement;
    };
    Vector<DomainData> domains() const;
    bool hasHadRecentWebPushInteraction(const DomainData&) const;
    bool hasHadUnexpiredRecentUserInteraction(const DomainData&, OperatingDatesWindow);
    void clearGrandfathering(Vector<unsigned>&&);
    WebCore::StorageAccessPromptWasShown hasUserGrantedStorageAccessThroughPrompt(unsigned domainID, const RegistrableDomain&);

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

    int predicateValueForDomain(WebCore::SQLiteStatementAutoResetScope&, const RegistrableDomain&) const;
    bool predicateBoolValueForDomain(WebCore::SQLiteStatementAutoResetScope&, const RegistrableDomain&) const;

    CookieAccess cookieAccess(const SubResourceDomain&, const TopFrameDomain&, CanRequestStorageAccessWithoutUserInteraction);
    void updateCookieBlockingForDomains(RegistrableDomainsToBlockCookiesFor&&, CompletionHandler<void()>&&);

    void setPrevalentResource(const RegistrableDomain&, ResourceLoadPrevalence);
    void classifyPrevalentResources();
    unsigned recursivelyFindNonPrevalentDomainsThatRedirectedToThisDomain(unsigned primaryDomainID, StdSet<unsigned>& nonPrevalentRedirectionSources, unsigned numberOfRecursiveCalls);
    void setDomainsAsPrevalent(StdSet<unsigned>&&);
    void grantStorageAccessInternal(SubFrameDomain&&, TopFrameDomain&&, std::optional<WebCore::FrameIdentifier>, WebCore::PageIdentifier, WebCore::StorageAccessPromptWasShown, WebCore::StorageAccessScope, CanRequestStorageAccessWithoutUserInteraction, CompletionHandler<void(WebCore::StorageAccessWasGranted)>&&);
    void markAsPrevalentIfHasRedirectedToPrevalent();
    
    enum class AddedRecord : bool { No, Yes };
    // reason is used for logging purpose.
    std::pair<AddedRecord, std::optional<unsigned>> ensureResourceStatisticsForRegistrableDomain(const RegistrableDomain&, ASCIILiteral reason) WARN_UNUSED_RETURN;
    bool shouldRemoveAllWebsiteDataFor(const DomainData&, bool shouldCheckForGrandfathering);
    bool shouldRemoveAllButCookiesFor(const DomainData&, bool shouldCheckForGrandfathering);
    bool shouldEnforceSameSiteStrictFor(DomainData&, bool shouldCheckForGrandfathering);
    void setIsScheduledForAllScriptWrittenStorageRemoval(const RegistrableDomain&, DataRemovalFrequency);
    DataRemovalFrequency dataRemovalFrequency(const RegistrableDomain&) const;
    void clearTopFrameUniqueRedirectsToSinceSameSiteStrictEnforcement(const NavigatedToDomain&, CompletionHandler<void()>&&);
    bool shouldEnforceSameSiteStrictForSpecificDomain(const RegistrableDomain&) const;
    RegistrableDomainsToDeleteOrRestrictWebsiteDataFor registrableDomainsToDeleteOrRestrictWebsiteDataFor();

    bool shouldRemoveDataRecords() const;
    void setDebugLogggingEnabled(bool enabled) { m_debugLoggingEnabled  = enabled; }
    Vector<RegistrableDomain> ensurePrevalentResourcesForDebugMode();
    void setDataRecordsBeingRemoved(bool);
    void removeDataRecords(CompletionHandler<void()>&&);
    void setCacheMaxAgeCap(Seconds);
    void updateCacheMaxAgeCap();
    void updateClientSideCookiesAgeCap();
    void updateOperatingDatesParameters();
    Seconds getMostRecentlyUpdatedTimestamp(const RegistrableDomain&, const TopFrameDomain&) const;
    const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& expectedTableAndIndexQueries() final;
    std::span<const ASCIILiteral> sortedTables() final;
    String ensureAndMakeDomainList(const HashSet<RegistrableDomain>&);
    std::optional<WallTime> mostRecentUserInteractionTime(const DomainData&);
    void grandfatherDataForDomains(const HashSet<RegistrableDomain>&);
    bool areAllThirdPartyCookiesBlockedUnder(const TopFrameDomain&);
    bool hasStatisticsExpired(WallTime mostRecentUserInteractionTime, OperatingDatesWindow) const;
    void scheduleStatisticsProcessingRequestIfNecessary();
    void pruneStatisticsIfNeeded();

    void openITPDatabase();
    void clearDatabaseContents();
    bool createUniqueIndices() final;
    bool createSchema() final;
    void addMissingTablesIfNecessary();
    bool missingUniqueIndices();
    bool needsUpdatedSchema() final;
    bool missingReferenceToObservedDomains();
    void migrateDataToPCMDatabaseIfNecessary();
    void openAndUpdateSchemaIfNecessary();
    bool tableExists(StringView);
    void deleteTable(StringView);
    void destroyStatements() final;

    WebResourceLoadStatisticsStore& m_store;
    Ref<SuspendableWorkQueue> m_workQueue;
#if HAVE(CORE_PREDICTION)
    ResourceLoadStatisticsClassifierCocoa m_resourceLoadStatisticsClassifier;
#else
    ResourceLoadStatisticsClassifier m_resourceLoadStatisticsClassifier;
#endif
    Parameters m_parameters;
    WallTime m_endOfGrandfatheringTimestamp;
    RegistrableDomain m_debugManualPrevalentResource;
    MonotonicTime m_lastTimeDataRecordsWereRemoved;
    uint64_t m_lastStatisticsProcessingRequestIdentifier { 0 };
    std::optional<uint64_t> m_pendingStatisticsProcessingRequestIdentifier;
    const RegistrableDomain m_debugStaticPrevalentResource { URL { "https://3rdpartytestwebkit.org"_str } };
    RegistrableDomain m_standaloneApplicationDomain;
    HashSet<RegistrableDomain> m_appBoundDomains;
    HashSet<RegistrableDomain> m_managedDomains;
    HashSet<RegistrableDomain> m_persistedDomains;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_observedDomainCountStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_insertObservedDomainStatement;
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
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_isScheduledForAllButCookieDataRemovalStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_updateIsScheduledForAllButCookieDataRemovalStatement;
    mutable std::unique_ptr<WebCore::SQLiteStatement> m_updateMostRecentWebPushInteractionTimeStatement;
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

    Vector<CompletionHandler<void()>> m_dataRecordRemovalCompletionHandlers;
    Seconds m_timeAdvanceForTesting;

    PAL::SessionID m_sessionID;
    unsigned m_operatingDatesSize { 0 };
    std::optional<OperatingDate> m_longWindowOperatingDate;
    std::optional<OperatingDate> m_shortWindowOperatingDate;
    OperatingDate m_mostRecentOperatingDate;
    WebCore::ThirdPartyCookieBlockingMode m_thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    WebCore::SameSiteStrictEnforcementEnabled m_sameSiteStrictEnforcementEnabled { WebCore::SameSiteStrictEnforcementEnabled::No };
    WebCore::FirstPartyWebsiteDataRemovalMode m_firstPartyWebsiteDataRemovalMode { WebCore::FirstPartyWebsiteDataRemovalMode::AllButCookies };
    ShouldIncludeLocalhost m_shouldIncludeLocalhost { ShouldIncludeLocalhost::Yes };
    bool m_debugLoggingEnabled { false };
    bool m_debugModeEnabled { false };
    bool m_dataRecordsBeingRemoved { false };
    bool m_isNewResourceLoadStatisticsDatabaseFile { false };
};

} // namespace WebKit
