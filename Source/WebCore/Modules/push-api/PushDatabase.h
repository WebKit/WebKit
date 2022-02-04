/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(SERVICE_WORKER)

#include "EpochTimeStamp.h"
#include "PushSubscriptionIdentifier.h"
#include "SQLiteDatabase.h"
#include "SQLiteStatement.h"
#include "SQLiteStatementAutoResetScope.h"
#include <wtf/CompletionHandler.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

namespace WebCore {

// Wake state that applies for devices that race-to-sleep.
enum class PushWakeState : uint8_t {
    Waking, // All pushes will wake device.
    Opportunistic, // Low priority pushes may not wake device.
    NonWaking, // No pushes will wake device.
    NumberOfStates
};

struct PushRecord {
    PushSubscriptionIdentifier identifier;
    String bundleID;
    String securityOrigin;
    String scope;
    String endpoint;
    String topic;
    Vector<uint8_t> serverVAPIDPublicKey;
    Vector<uint8_t> clientPublicKey;
    Vector<uint8_t> clientPrivateKey;
    Vector<uint8_t> sharedAuthSecret;
    std::optional<EpochTimeStamp> expirationTime { };
    PushWakeState wakeState { PushWakeState::Waking };

    WEBCORE_EXPORT PushRecord isolatedCopy() const &;
    WEBCORE_EXPORT PushRecord isolatedCopy() &&;
};

class PushDatabase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using CreationHandler = CompletionHandler<void(std::unique_ptr<PushDatabase>&&)>;

    WEBCORE_EXPORT static void create(const String& path, CreationHandler&&);
    WEBCORE_EXPORT ~PushDatabase();

    WEBCORE_EXPORT void insertRecord(const PushRecord&, CompletionHandler<void(std::optional<PushRecord>&&)>&&);
    WEBCORE_EXPORT void removeRecordByIdentifier(PushSubscriptionIdentifier, CompletionHandler<void(bool)>&&);
    WEBCORE_EXPORT void getRecordByTopic(const String& topic, CompletionHandler<void(std::optional<PushRecord>&&)>&&);
    WEBCORE_EXPORT void getRecordByBundleIdentifierAndScope(const String& bundleID, const String& scope, CompletionHandler<void(std::optional<PushRecord>&&)>&&);
    WEBCORE_EXPORT void getIdentifiers(CompletionHandler<void(HashSet<PushSubscriptionIdentifier>&&)>&&);

    using PushWakeStateToTopicMap = HashMap<PushWakeState, Vector<String>, WTF::IntHash<PushWakeState>, WTF::StrongEnumHashTraits<PushWakeState>>;
    WEBCORE_EXPORT void getTopicsByWakeState(CompletionHandler<void(PushWakeStateToTopicMap&&)>&&);

private:
    PushDatabase(Ref<WorkQueue>&&, UniqueRef<WebCore::SQLiteDatabase>&&);
    WebCore::SQLiteStatementAutoResetScope cachedStatementOnQueue(ASCIILiteral query);
    void dispatchOnWorkQueue(Function<void()>&&);

    Ref<WorkQueue> m_queue;
    UniqueRef<WebCore::SQLiteDatabase> m_db;
    HashMap<const char*, UniqueRef<WebCore::SQLiteStatement>> m_statements;
};

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
