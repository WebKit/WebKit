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
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

namespace PCM {

struct DebugInfo;

// This is created, used, and destroyed on the Store's queue.
class Database : public DatabaseUtilities {
    WTF_MAKE_FAST_ALLOCATED;
public:
    Database(const String& storageDirectory);
    virtual ~Database();
    
    static void interruptAllDatabases();

    void insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&&, PrivateClickMeasurementAttributionType);
    std::pair<std::optional<WebCore::PrivateClickMeasurement::AttributionSecondsUntilSendData>, DebugInfo> attributePrivateClickMeasurement(const WebCore::PrivateClickMeasurement::SourceSite&, const WebCore::PrivateClickMeasurement::AttributionDestinationSite&, WebCore::PrivateClickMeasurement::AttributionTriggerData&&);
    Vector<WebCore::PrivateClickMeasurement> allAttributedPrivateClickMeasurement();
    void clearPrivateClickMeasurement(std::optional<WebCore::RegistrableDomain>);
    void clearExpiredPrivateClickMeasurement();
    void clearSentAttribution(WebCore::PrivateClickMeasurement&&, WebCore::PrivateClickMeasurement::AttributionReportEndpoint);

    String privateClickMeasurementToStringForTesting();
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
    std::pair<std::optional<UnattributedPrivateClickMeasurement>, std::optional<AttributedPrivateClickMeasurement>> findPrivateClickMeasurement(const WebCore::PrivateClickMeasurement::SourceSite&, const WebCore::PrivateClickMeasurement::AttributionDestinationSite&);
    WebCore::PrivateClickMeasurement buildPrivateClickMeasurementFromDatabase(WebCore::SQLiteStatement*, PrivateClickMeasurementAttributionType);
    void removeUnattributed(WebCore::PrivateClickMeasurement&);
    String attributionToStringForTesting(WebCore::SQLiteStatement&, PrivateClickMeasurementAttributionType);
    void markReportAsSentToDestination(SourceDomainID, DestinationDomainID);
    void markReportAsSentToSource(SourceDomainID, DestinationDomainID);
    std::pair<std::optional<SourceEarliestTimeToSend>, std::optional<DestinationEarliestTimeToSend>> earliestTimesToSend(const WebCore::PrivateClickMeasurement&);
    std::optional<DomainID> ensureDomainID(const WebCore::RegistrableDomain&);
    std::optional<DomainID> domainID(const WebCore::RegistrableDomain&);
    String getDomainStringFromDomainID(DomainID);

    std::unique_ptr<WebCore::SQLiteStatement> m_setUnattributedPrivateClickMeasurementAsExpiredStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_findUnattributedStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_findAttributedStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_removeUnattributedStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_allAttributedPrivateClickMeasurementStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_allUnattributedPrivateClickMeasurementAttributionsStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_clearAllPrivateClickMeasurementStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_clearExpiredPrivateClickMeasurementStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_earliestTimesToSendStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_markReportAsSentToSourceStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_markReportAsSentToDestinationStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_domainIDFromStringStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_domainStringFromDomainIDStatement;
    std::unique_ptr<WebCore::SQLiteStatement> m_insertObservedDomainStatement;
};

} // namespace PCM

} // namespace WebKit
