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

#include "config.h"
#include "Utilities.h"
#include <WebCore/PushDatabase.h>
#include <WebCore/SQLiteDatabase.h>
#include <wtf/FileSystem.h>

#if ENABLE(SERVICE_WORKER)

using namespace WebCore;

namespace TestWebKitAPI {

static String makeTemporaryDatabasePath()
{
    FileSystem::PlatformFileHandle handle;
    auto path = FileSystem::openTemporaryFile("PushDatabase", handle, ".db");
    FileSystem::closeFile(handle);
    return path;
}

static Vector<String> getTopicsSync(PushDatabase& database)
{
    bool done = false;
    Vector<String> topics;

    database.getTopics([&done, &topics](auto&& result) {
        topics = WTFMove(result);
        done = true;
    });
    Util::run(&done);

    return topics;
}

class PushDatabaseTest : public testing::Test {
public:
    std::unique_ptr<PushDatabase> db;

    PushRecord record1 {
        PushSubscriptionIdentifier(),
        "com.apple.webapp"_s,
        "https://www.apple.com"_s,
        "https://www.apple.com/iphone"_s,
        "https://pushEndpoint2"_s,
        "topic1"_s,
        { 5, 6 },
        { 6, 7 },
        { 7, 8 },
        { 8, 9 }
    };
    PushRecord record2 {
        PushSubscriptionIdentifier(),
        "com.apple.Safari"_s,
        "https://www.webkit.org"_s,
        "https://www.webkit.org/blog"_s,
        "https://pushEndpoint4"_s,
        "topic4"_s,
        { 14, 15 },
        { 16, 17 },
        { 18, 19 },
        { 20, 21 }
    };
    PushRecord record3 {
        PushSubscriptionIdentifier(),
        "com.apple.Safari"_s,
        "https://www.apple.com"_s,
        "https://www.apple.com/mac"_s,
        "https://pushEndpoint1"_s,
        "topic2"_s,
        { 0, 1 },
        { 1, 2 },
        { 2, 3 },
        { 4, 5 },
        convertSecondsToEpochTimeStamp(1643350000),
    };
    PushRecord record4 {
        PushSubscriptionIdentifier(),
        "com.apple.Safari"_s,
        "https://www.apple.com"_s,
        "https://www.apple.com/iphone"_s,
        "https://pushEndpoint3"_s,
        "topic3"_s,
        { 9, 10 },
        { 10, 11 },
        { 11, 12 },
        { 12, 13 }
    };

    std::optional<PushRecord> insertResult1;
    std::optional<PushRecord> insertResult2;
    std::optional<PushRecord> insertResult3;
    std::optional<PushRecord> insertResult4;

    std::optional<PushRecord> insertRecord(const PushRecord& record)
    {
        bool done = false;
        std::optional<PushRecord> insertResult;

        db->insertRecord(record, [&done, &insertResult](std::optional<PushRecord>&& result) {
            insertResult = WTFMove(result);
            done = true;
        });
        Util::run(&done);

        return insertResult;
    }

    bool removeRecordByRowIdentifier(uint64_t rowIdentifier)
    {
        bool done = false;
        bool removeResult = false;

        db->removeRecordByIdentifier(makeObjectIdentifier<PushSubscriptionIdentifierType>(rowIdentifier), [&done, &removeResult](bool result) {
            removeResult = result;
            done = true;
        });
        Util::run(&done);

        return removeResult;
    }

    std::optional<PushRecord> getRecordByTopic(const String& topic)
    {
        bool done = false;
        std::optional<PushRecord> getResult;

        db->getRecordByTopic(topic, [&done, &getResult](std::optional<PushRecord>&& result) {
            getResult = WTFMove(result);
            done = true;
        });
        Util::run(&done);

        return getResult;
    }

    std::optional<PushRecord> getRecordByBundleIdentifierAndScope(const String& bundleID, const String& scope)
    {
        bool done = false;
        std::optional<PushRecord> getResult;

        db->getRecordByBundleIdentifierAndScope(bundleID, scope, [&done, &getResult](std::optional<PushRecord>&& result) {
            getResult = WTFMove(result);
            done = true;
        });
        Util::run(&done);

        return getResult;
    }

    HashSet<uint64_t> getRowIdentifiers()
    {
        bool done = false;
        HashSet<uint64_t> rowIdentifiers;

        db->getIdentifiers([&done, &rowIdentifiers](HashSet<PushSubscriptionIdentifier>&& identifiers) {
            for (auto identifier : identifiers)
                rowIdentifiers.add(identifier.toUInt64());
            done = true;
        });
        Util::run(&done);

        return rowIdentifiers;
    }

    Vector<String> getTopics()
    {
        return getTopicsSync(*db);
    }

    Vector<RemovedPushRecord> removeRecordsByBundleIdentifier(const String& bundleID)
    {
        bool done = false;
        Vector<RemovedPushRecord> removedRecords;

        db->removeRecordsByBundleIdentifier(bundleID, [&done, &removedRecords](auto&& result) {
            removedRecords = WTFMove(result);
            done = true;
        });
        Util::run(&done);

        return removedRecords;
    }

    Vector<RemovedPushRecord> removeRecordsByBundleIdentifierAndSecurityOrigin(const String& bundleID, const String& securityOrigin)
    {
        bool done = false;
        Vector<RemovedPushRecord> removedRecords;

        db->removeRecordsByBundleIdentifierAndSecurityOrigin(bundleID, securityOrigin, [&done, &removedRecords](auto&& result) {
            removedRecords = WTFMove(result);
            done = true;
        });
        Util::run(&done);

        return removedRecords;
    }

    unsigned incrementSilentPushCount(const String& bundleID, const String& securityOrigin)
    {
        bool done = false;
        unsigned count = 0;

        db->incrementSilentPushCount(bundleID, securityOrigin, [&done, &count](int result) {
            count = result;
            done = true;
        });
        Util::run(&done);

        return count;
    }

    void SetUp() final
    {
        bool done = false;
        PushDatabase::create(makeTemporaryDatabasePath(), [this, &done](std::unique_ptr<PushDatabase>&& database) {
            db = WTFMove(database);
            done = true;
        });
        Util::run(&done);

        ASSERT_TRUE(db);
        ASSERT_TRUE(insertResult1 = insertRecord(record1));
        ASSERT_TRUE(insertResult2 = insertRecord(record2));
        ASSERT_TRUE(insertResult3 = insertRecord(record3));
        ASSERT_TRUE(insertResult4 = insertRecord(record4));
    }
};

static bool operator==(const PushRecord& a, const PushRecord& b)
{
    return a.identifier == b.identifier
        && a.bundleID == b.bundleID
        && a.securityOrigin == b.securityOrigin
        && a.scope == b.scope
        && a.endpoint == b.endpoint
        && a.topic == b.topic
        && a.serverVAPIDPublicKey == b.serverVAPIDPublicKey
        && a.clientPublicKey == b.clientPublicKey
        && a.clientPrivateKey == b.clientPrivateKey
        && a.sharedAuthSecret == b.sharedAuthSecret
        && a.expirationTime == b.expirationTime;
}

TEST_F(PushDatabaseTest, InsertRecord)
{
    auto expectedRecord1 = record1;
    expectedRecord1.identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(1);
    EXPECT_TRUE(expectedRecord1 == *insertResult1);

    auto expectedRecord2 = record2;
    expectedRecord2.identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(2);
    EXPECT_TRUE(expectedRecord2 == *insertResult2);

    auto expectedRecord3 = record3;
    expectedRecord3.identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(3);
    EXPECT_TRUE(expectedRecord3 == *insertResult3);

    auto expectedRecord4 = record4;
    expectedRecord4.identifier = makeObjectIdentifier<PushSubscriptionIdentifierType>(4);
    EXPECT_TRUE(expectedRecord4 == *insertResult4);

    // Inserting a record with the same (bundleID, scope) as record 1 should fail.
    PushRecord record5 = record1;
    record4.endpoint = "https://pushEndpoint5"_s;
    EXPECT_FALSE(insertRecord(WTFMove(record5)));

    EXPECT_EQ(getRowIdentifiers(), (HashSet<uint64_t> { 1, 2, 3, 4 }));
}

TEST_F(PushDatabaseTest, RemoveRecord)
{
    EXPECT_TRUE(removeRecordByRowIdentifier(1));
    EXPECT_FALSE(removeRecordByRowIdentifier(1));
    EXPECT_EQ(getRowIdentifiers(), (HashSet<uint64_t> { 2, 3, 4 }));
}

TEST_F(PushDatabaseTest, RemoveRecordsByBundleIdentifier)
{
    // record2, record3, and record4 have the same bundleID.
    auto removedRecords = removeRecordsByBundleIdentifier(record2.bundleID);
    bool containsRecord2 = removedRecords.findIf([topic = record2.topic](auto& record) {
        return topic == record.topic;
    }) != notFound;
    bool containsRecord3 = removedRecords.findIf([topic = record3.topic](auto& record) {
        return topic == record.topic;
    }) != notFound;
    bool containsRecord4 = removedRecords.findIf([topic = record4.topic](auto& record) {
        return topic == record.topic;
    }) != notFound;

    EXPECT_TRUE(containsRecord2);
    EXPECT_TRUE(containsRecord3);
    EXPECT_TRUE(containsRecord4);
    EXPECT_EQ(removedRecords.size(), 3u);
    EXPECT_EQ(getRowIdentifiers(), (HashSet<uint64_t> { 1 }));

    // Inserting a new record should produce a new identifier.
    PushRecord record5 = record3;
    auto insertResult = insertRecord(WTFMove(record5));
    EXPECT_TRUE(insertResult);
    EXPECT_EQ(insertResult->identifier, makeObjectIdentifier<PushSubscriptionIdentifierType>(5));
    EXPECT_EQ(getRowIdentifiers(), (HashSet<uint64_t> { 1, 5 }));
}

TEST_F(PushDatabaseTest, RemoveRecordsByBundleIdentifierAndSecurityOrigin)
{
    // record3 and record4 have the same bundleID and securityOrigin.
    auto removedRecords = removeRecordsByBundleIdentifierAndSecurityOrigin(record3.bundleID, record3.securityOrigin);
    bool containsRecord3 = removedRecords.findIf([topic = record3.topic](auto& record) {
        return topic == record.topic;
    }) != notFound;
    bool containsRecord4 = removedRecords.findIf([topic = record4.topic](auto& record) {
        return topic == record.topic;
    }) != notFound;

    EXPECT_TRUE(containsRecord3);
    EXPECT_TRUE(containsRecord4);
    EXPECT_EQ(removedRecords.size(), 2u);
    EXPECT_EQ(getRowIdentifiers(), (HashSet<uint64_t> { 1, 2 }));

    // Inserting a new record should produce a new identifier.
    PushRecord record5 = record3;
    auto insertResult = insertRecord(WTFMove(record5));
    EXPECT_TRUE(insertResult);
    EXPECT_EQ(insertResult->identifier, makeObjectIdentifier<PushSubscriptionIdentifierType>(5));
    EXPECT_EQ(getRowIdentifiers(), (HashSet<uint64_t> { 1, 2, 5 }));
}

TEST_F(PushDatabaseTest, GetRecordByTopic)
{
    auto result1 = getRecordByTopic(record1.topic);
    ASSERT_TRUE(result1);
    EXPECT_TRUE(*result1 == *insertResult1);

    auto result2 = getRecordByTopic("foo"_s);
    EXPECT_FALSE(result2);
}

TEST_F(PushDatabaseTest, GetRecordByBundleIdentifierAndScope)
{
    auto result1 = getRecordByBundleIdentifierAndScope(record1.bundleID, record1.scope);
    ASSERT_TRUE(result1);
    EXPECT_TRUE(*result1 == *insertResult1);

    auto result2 = getRecordByBundleIdentifierAndScope(record1.bundleID, "bar"_s);
    EXPECT_FALSE(result2);

    auto result3 = getRecordByBundleIdentifierAndScope("foo"_s, "bar"_s);
    EXPECT_FALSE(result3);
}

TEST_F(PushDatabaseTest, GetTopics)
{
    Vector<String> expected { "topic1"_s, "topic2"_s, "topic3"_s, "topic4"_s };
    EXPECT_EQ(getTopics(), expected);
}

TEST_F(PushDatabaseTest, IncrementSilentPushCount)
{
    auto count = incrementSilentPushCount(record1.bundleID, record1.securityOrigin);
    EXPECT_EQ(count, 1u);

    // record1 and record3 have different bundleID and securityOrigin.
    count = incrementSilentPushCount(record3.bundleID, record3.securityOrigin);
    EXPECT_EQ(count, 1u);

    // record3 and record4 have the same bundleID and securityOrigin.
    count = incrementSilentPushCount(record4.bundleID, record4.securityOrigin);
    EXPECT_EQ(count, 2u);

    count = incrementSilentPushCount("nonexistent"_s, "nonexistent"_s);
    EXPECT_EQ(count, 0u);
}

TEST(PushDatabase, ManyInFlightOps)
{
    auto path = makeTemporaryDatabasePath();
    constexpr unsigned recordCount = 256;

    {
        std::unique_ptr<PushDatabase> createResult;
        bool done = false;
        PushDatabase::create(path, [&createResult, &done](std::unique_ptr<PushDatabase>&& database) {
            createResult = WTFMove(database);
            done = true;
        });
        Util::run(&done);
        ASSERT_TRUE(createResult);

        auto& database = *createResult;
        char scope[64] { };
        char topic[64] { };
        PushRecord record {
            PushSubscriptionIdentifier(),
            "com.apple.Safari"_s,
            "https://www.webkit.org"_s,
            { },
            "https://pushEndpoint1"_s,
            { },
            { 0, 1 },
            { 1, 2 },
            { 2, 3 },
            { 4, 5 },
            convertSecondsToEpochTimeStamp(1643350000),
        };

        for (unsigned i = 0; i < recordCount; i++) {
            snprintf(scope, sizeof(scope), "http://www.webkit.org/test/%d", i);
            snprintf(topic, sizeof(topic), "topic_%d", i);
            record.scope = scope;
            record.topic = topic;

            database.insertRecord(record, [](auto&& result) {
                ASSERT_TRUE(result);
            });
        }
    }

    {
        std::unique_ptr<PushDatabase> createResult;
        bool done = false;
        PushDatabase::create(path, [&createResult, &done](std::unique_ptr<PushDatabase>&& database) {
            createResult = WTFMove(database);
            done = true;
        });
        Util::run(&done);
        ASSERT_TRUE(createResult);

        auto topics = getTopicsSync(*createResult);
        EXPECT_EQ(topics.size(), recordCount);
    }
}

TEST(PushDatabase, StartsFromScratchOnDowngrade)
{
    auto path = makeTemporaryDatabasePath();

    {
        SQLiteDatabase db;
        db.open(path);
        ASSERT_TRUE(db.executeCommand("PRAGMA user_version = 100000"_s));
    }

    {
        std::unique_ptr<PushDatabase> createResult;
        bool done = false;
        PushDatabase::create(path, [&createResult, &done](std::unique_ptr<PushDatabase>&& database) {
            createResult = WTFMove(database);
            done = true;
        });
        Util::run(&done);
        ASSERT_TRUE(createResult);
    }

    {
        SQLiteDatabase db;
        db.open(path);
        {
            int version = 0;
            auto sql = db.prepareStatement("PRAGMA user_version"_s);
            if (sql && sql->step() == SQLITE_ROW)
                version = sql->columnInt(0);
            EXPECT_GT(version, 0);
            EXPECT_LT(version, 100000);
        }
    }
}

} // namespace TestWebKitAPI

#endif // ENABLE(SERVICE_WORKER)

