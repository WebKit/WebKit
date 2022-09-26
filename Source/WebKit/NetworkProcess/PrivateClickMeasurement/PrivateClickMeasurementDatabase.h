/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <WebCore/PrivateClickMeasurement.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit::PCM {

struct DebugInfo;

// This is created, used, and destroyed on the Store's queue.
class Database : public DatabaseUtilities {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Database(const String& storageDirectory);
    virtual ~Database();
    
    using ApplicationBundleIdentifier = String;

    static void interruptAllDatabases();

    void insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&&, PrivateClickMeasurementAttributionType);
    std::pair<std::optional<WebCore::PCM::AttributionSecondsUntilSendData>, DebugInfo> attributePrivateClickMeasurement(const WebCore::PCM::SourceSite&, const WebCore::PCM::AttributionDestinationSite&, const ApplicationBundleIdentifier&, WebCore::PCM::AttributionTriggerData&&, WebCore::PrivateClickMeasurement::IsRunningLayoutTest);
    Vector<WebCore::PrivateClickMeasurement> allAttributedPrivateClickMeasurement();
    void clearPrivateClickMeasurement(std::optional<WebCore::RegistrableDomain>);
    void clearExpiredPrivateClickMeasurement();
    void clearSentAttribution(WebCore::PrivateClickMeasurement&&, WebCore::PCM::AttributionReportEndpoint);

    String privateClickMeasurementToStringForTesting() const;
    void markAllUnattributedPrivateClickMeasurementAsExpiredForTesting();
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting();

private:
    using UnattributedPrivateClickMeasurement = WebCore::PrivateClickMeasurement;
    using AttributedPrivateClickMeasurement = WebCore::PrivateClickMeasurement;
    using DomainID = unsigned;
    using SourceDomainID = unsigned;
    using DestinationDomainID = unsigned;
    using SourceEarliestTimeToSend = double;
    using DestinationEarliestTimeToSend = double;

    bool createSchema() final;
    void destroyStatements() final;
    std::pair<std::optional<UnattributedPrivateClickMeasurement>, std::optional<AttributedPrivateClickMeasurement>> findPrivateClickMeasurement(const WebCore::PCM::SourceSite&, const WebCore::PCM::AttributionDestinationSite&, const ApplicationBundleIdentifier&);
    void removeUnattributed(WebCore::PrivateClickMeasurement&);
    String attributionToStringForTesting(const WebCore::PrivateClickMeasurement&) const;
    void markReportAsSentToDestination(SourceDomainID, DestinationDomainID, const ApplicationBundleIdentifier&);
    void markReportAsSentToSource(SourceDomainID, DestinationDomainID, const ApplicationBundleIdentifier&);
    std::pair<std::optional<SourceEarliestTimeToSend>, std::optional<DestinationEarliestTimeToSend>> earliestTimesToSend(const WebCore::PrivateClickMeasurement&);
    std::optional<DomainID> ensureDomainID(const WebCore::RegistrableDomain&);
    std::optional<DomainID> domainID(const WebCore::RegistrableDomain&);
    String getDomainStringFromDomainID(DomainID) const final;

    void addDestinationTokenColumnsIfNecessary();
    bool needsUpdatedSchema() final { return false; };
    bool createUniqueIndices() final;
    const MemoryCompactLookupOnlyRobinHoodHashMap<String, TableAndIndexPair>& expectedTableAndIndexQueries() final;
    Span<const ASCIILiteral> sortedTables() final;

    using Statement = std::unique_ptr<WebCore::SQLiteStatement>;
    mutable Statement m_setUnattributedPrivateClickMeasurementAsExpiredStatement;
    mutable Statement m_findUnattributedStatement;
    mutable Statement m_findAttributedStatement;
    mutable Statement m_removeUnattributedStatement;
    mutable Statement m_allAttributedPrivateClickMeasurementStatement;
    mutable Statement m_allUnattributedPrivateClickMeasurementAttributionsStatement;
    mutable Statement m_clearAllPrivateClickMeasurementStatement;
    mutable Statement m_clearExpiredPrivateClickMeasurementStatement;
    mutable Statement m_earliestTimesToSendStatement;
    mutable Statement m_markReportAsSentToSourceStatement;
    mutable Statement m_markReportAsSentToDestinationStatement;
    mutable Statement m_domainIDFromStringStatement;
    mutable Statement m_domainStringFromDomainIDStatement;
    mutable Statement m_insertObservedDomainStatement;
};

} // namespace WebKit::PCM
