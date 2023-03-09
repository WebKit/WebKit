/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "PrivateClickMeasurementStore.h"

namespace WebKit {

enum class PrivateClickMeasurementAttributionType : bool;

namespace PCM {

class Database;
struct DebugInfo;

class PersistentStore : public Store {
public:
    static Ref<Store> create(const String& databaseDirectory)
    {
        return adoptRef(*new PersistentStore(databaseDirectory));
    }

    ~PersistentStore();

    using ApplicationBundleIdentifier = String;

    static void prepareForProcessToSuspend(CompletionHandler<void()>&&);
    static void processDidResume();

    void insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&&, WebKit::PrivateClickMeasurementAttributionType, CompletionHandler<void()>&&) final;
    void attributePrivateClickMeasurement(WebCore::PCM::SourceSite&&, WebCore::PCM::AttributionDestinationSite&&, const ApplicationBundleIdentifier&, WebCore::PCM::AttributionTriggerData&&, WebCore::PrivateClickMeasurement::IsRunningLayoutTest, CompletionHandler<void(std::optional<WebCore::PCM::AttributionSecondsUntilSendData>&&, DebugInfo&&)>&&) final;

    void privateClickMeasurementToStringForTesting(CompletionHandler<void(String)>&&) const final;
    void markAllUnattributedPrivateClickMeasurementAsExpiredForTesting() final;
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&) final;

    void allAttributedPrivateClickMeasurement(CompletionHandler<void(Vector<WebCore::PrivateClickMeasurement>&&)>&&) final;
    void clearExpiredPrivateClickMeasurement() final;
    void clearPrivateClickMeasurement(CompletionHandler<void()>&&) final;
    void clearPrivateClickMeasurementForRegistrableDomain(WebCore::RegistrableDomain&&, CompletionHandler<void()>&&) final;
    void clearSentAttribution(WebCore::PrivateClickMeasurement&& attributionToClear, WebCore::PCM::AttributionReportEndpoint) final;

    void close(CompletionHandler<void()>&&) final;

private:
    PersistentStore(const String& databaseDirectory);

    void postTask(Function<void()>&&) const;
    void postTaskReply(Function<void()>&&) const;

    std::unique_ptr<Database> m_database;
    Ref<SuspendableWorkQueue> m_queue;
};

} // namespace PCM

} // namespace WebKit
