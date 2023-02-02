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

#import "config.h"
#import "PushService.h"

#import "ApplePushServiceConnection.h"
#import "MockPushServiceConnection.h"
#import "Logging.h"
#import "WebPushDaemonConstants.h"
#import <Foundation/Foundation.h>
#import <WebCore/PushMessageCrypto.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/WorkQueue.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/Base64.h>

using namespace WebKit;
using namespace WebCore;

namespace WebPushD {

static void updateTopicLists(PushServiceConnection& connection, PushDatabase& database, CompletionHandler<void()> completionHandler)
{
    database.getTopics([&connection, completionHandler = WTFMove(completionHandler)](auto&& topics) mutable {
        PushServiceConnection::TopicLists topicLists;
        topicLists.enabledTopics = WTFMove(topics.enabledTopics);
        topicLists.ignoredTopics = WTFMove(topics.ignoredTopics);
        connection.setTopicLists(WTFMove(topicLists));
        completionHandler();
    });
}

#if HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

void PushService::create(const String& incomingPushServiceName, const String& databasePath, IncomingPushMessageHandler&& messageHandler, CompletionHandler<void(std::unique_ptr<PushService>&&)>&& creationHandler)
{
    auto transaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.push-service-init"));

    // Create the connection ASAP so that we bootstrap_check_in to the service in a timely manner.
    auto connection = makeUniqueRef<ApplePushServiceConnection>(incomingPushServiceName);

    PushDatabase::create(databasePath, [transaction = WTFMove(transaction), connection = WTFMove(connection), messageHandler = WTFMove(messageHandler), creationHandler = WTFMove(creationHandler)](auto&& databaseResult) mutable {
        if (!databaseResult) {
            RELEASE_LOG_ERROR(Push, "Push service initialization failed with database error");
            creationHandler(std::unique_ptr<PushService>());
            return;
        }

        auto database = makeUniqueRefFromNonNullUniquePtr(WTFMove(databaseResult));
        UniqueRef<PushService> service(*new PushService(WTFMove(connection), WTFMove(database), WTFMove(messageHandler)));

        auto& connectionRef = service->connection();
        auto& databaseRef = service->database();

        // Only provide the service object back to the caller after we've synced the topic lists in
        // the database with the PushServiceConnection/APSConnection. This ensures that we won't
        // service any calls to subscribe/unsubscribe/etc. until after the topic lists are up to
        // date, which APSConnection cares about.
        updateTopicLists(connectionRef, databaseRef, [transaction = WTFMove(transaction), service = WTFMove(service), creationHandler = WTFMove(creationHandler)]() mutable {
            creationHandler(service.moveToUniquePtr());
        });
    });
}

#else

void PushService::create(const String&, const String&, IncomingPushMessageHandler&&, CompletionHandler<void(std::unique_ptr<PushService>&&)>&& creationHandler)
{
    creationHandler(std::unique_ptr<PushService>());
}

#endif // HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

void PushService::createMockService(IncomingPushMessageHandler&& messageHandler, CompletionHandler<void(std::unique_ptr<PushService>&&)>&& creationHandler)
{
    PushDatabase::create(SQLiteDatabase::inMemoryPath(), [messageHandler = WTFMove(messageHandler), creationHandler = WTFMove(creationHandler)](auto&& databaseResult) mutable {
        if (!databaseResult) {
            creationHandler(std::unique_ptr<PushService>());
            return;
        }

        auto connection = makeUniqueRef<MockPushServiceConnection>();
        auto database = makeUniqueRefFromNonNullUniquePtr(WTFMove(databaseResult));
        creationHandler(std::unique_ptr<PushService>(new PushService(WTFMove(connection), WTFMove(database), WTFMove(messageHandler))));
    });
}

PushService::PushService(UniqueRef<PushServiceConnection>&& pushServiceConnection, UniqueRef<PushDatabase>&& pushDatabase, IncomingPushMessageHandler&& incomingPushMessageHandler)
    : m_connection(WTFMove(pushServiceConnection))
    , m_database(WTFMove(pushDatabase))
    , m_incomingPushMessageHandler(WTFMove(incomingPushMessageHandler))
{
    RELEASE_ASSERT(m_incomingPushMessageHandler);

    m_connection->startListeningForPublicToken([this](auto&& token) {
        didReceivePublicToken(WTFMove(token));
    });

    m_connection->startListeningForPushMessages([this](NSString *topic, NSDictionary *userInfo) {
        didReceivePushMessage(topic, userInfo);
    });
}

PushService::~PushService() = default;

static PushSubscriptionData makePushSubscriptionFromRecord(PushRecord&& record)
{
    return PushSubscriptionData {
        record.identifier,
        WTFMove(record.endpoint),
        WTFMove(record.expirationTime),
        WTFMove(record.serverVAPIDPublicKey),
        WTFMove(record.clientPublicKey),
        WTFMove(record.sharedAuthSecret)
    };
}

class PushServiceRequest {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~PushServiceRequest() = default;

    virtual ASCIILiteral description() const = 0;

    const PushSubscriptionSetIdentifier& subscriptionSetIdentifier() const { return m_identifier; }
    const String& scope() const { return m_scope; };

    virtual void start() = 0;

    String key() const { return makePushTopic(m_identifier, m_scope); }

protected:
    PushServiceRequest(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope)
        : m_service(service)
        , m_connection(service.connection())
        , m_database(service.database())
        , m_identifier(identifier)
        , m_scope(scope)
    {
    }

    virtual void finish() = 0;

    PushService& m_service;
    PushServiceConnection& m_connection;
    PushDatabase& m_database;
    PushSubscriptionSetIdentifier m_identifier;
    String m_scope;
    CompletionHandler<void(PushServiceRequest&)> m_completionHandler;
};

template<typename ResultType>
class PushServiceRequestImpl : public PushServiceRequest {
public:
    void start() final
    {
        if (m_identifier.bundleIdentifier.isEmpty() || m_scope.isEmpty()) {
            reject(WebCore::ExceptionData { WebCore::AbortError, "Invalid sender"_s });
            return;
        }
        
        String transactionDescription = makeString("com.apple.webkit.webpushd:", description(), ":"_s, m_identifier.debugDescription(), ":"_s, m_scope);
        m_transaction = adoptOSObject(os_transaction_create(transactionDescription.utf8().data()));

        RELEASE_LOG(Push, "Started pushServiceRequest %{public}s (%p) for %{public}s, scope = %{sensitive}s", description().characters(), this, m_identifier.debugDescription().utf8().data(), m_scope.utf8().data());
        startInternal();
    }

protected:
    using ResultHandler = CompletionHandler<void(const Expected<ResultType, WebCore::ExceptionData>&)>;

    PushServiceRequestImpl(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, ResultHandler&& resultHandler)
        : PushServiceRequest(service, identifier, scope)
        , m_resultHandler(WTFMove(resultHandler))
    {
    }
    virtual ~PushServiceRequestImpl() = default;

    virtual void startInternal() = 0;

    void fulfill(ResultType result)
    {
        bool hasResult = true;
        if constexpr (std::is_constructible_v<bool, ResultType>)
            hasResult = static_cast<bool>(result);

        RELEASE_LOG(Push, "Finished pushServiceRequest %{public}s (%p) with result (hasResult: %d) for %{public}s, scope = %{sensitive}s", description().characters(), this, hasResult, m_identifier.debugDescription().utf8().data(), m_scope.utf8().data());

        m_resultHandler(WTFMove(result));
        finish();
    }

    void reject(WebCore::ExceptionData&& data)
    {
        RELEASE_LOG(Push, "Finished pushServiceRequest %{public}s (%p) with exception for %{public}s, scope = %{sensitive}s", description().characters(), this, m_identifier.debugDescription().utf8().data(), m_scope.utf8().data());

        m_resultHandler(makeUnexpected(WTFMove(data)));
        finish();
    }

private:
    ResultHandler m_resultHandler;
    OSObjectPtr<os_transaction_t> m_transaction;
};

class GetSubscriptionRequest : public PushServiceRequestImpl<std::optional<WebCore::PushSubscriptionData>> {
public:
    GetSubscriptionRequest(PushService&, const PushSubscriptionSetIdentifier&, const String& scope, ResultHandler&&);
    virtual ~GetSubscriptionRequest() = default;

protected:
    ASCIILiteral description() const final { return "GetSubscriptionRequest"_s; }
    void startInternal() final;
    void finish() final { m_service.didCompleteGetSubscriptionRequest(*this); }
};

GetSubscriptionRequest::GetSubscriptionRequest(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, ResultHandler&& resultHandler)
    : PushServiceRequestImpl(service, identifier, scope, WTFMove(resultHandler))
{
}

// Implements the webpushd side of PushManager.getSubscription.
void GetSubscriptionRequest::startInternal()
{
    m_database.getRecordBySubscriptionSetAndScope(m_identifier, m_scope, [this](auto&& result) mutable {
        if (!result) {
            fulfill(std::optional<WebCore::PushSubscriptionData> { });
            return;
        }

        fulfill(makePushSubscriptionFromRecord(WTFMove(*result)));
    });
}

class SubscribeRequest : public PushServiceRequestImpl<WebCore::PushSubscriptionData> {
public:
    SubscribeRequest(PushService&, const PushSubscriptionSetIdentifier&, const String& scope, const Vector<uint8_t>& vapidPublicKey, ResultHandler&&);
    virtual ~SubscribeRequest() = default;

protected:
    ASCIILiteral description() const final { return "SubscribeRequest"_s; }
    void startInternal() final { startImpl(IsRetry::No); }
    void finish() final { m_service.didCompleteSubscribeRequest(*this); }

private:
    enum class IsRetry { No, Yes };
    void startImpl(IsRetry);
    void attemptToRecoverFromTopicAlreadyInFilterError(String&&);
    Vector<uint8_t> m_vapidPublicKey;
};

SubscribeRequest::SubscribeRequest(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, const Vector<uint8_t>& vapidPublicKey, ResultHandler&& resultHandler)
    : PushServiceRequestImpl(service, identifier, scope, WTFMove(resultHandler))
    , m_vapidPublicKey(vapidPublicKey)
{
}

// Implements the webpushd side of PushManager.subscribe().
void SubscribeRequest::startImpl(IsRetry isRetry)
{
    m_database.getRecordBySubscriptionSetAndScope(m_identifier, m_scope, [this, isRetry](auto&& result) mutable {
        if (result) {
            if (m_vapidPublicKey != result->serverVAPIDPublicKey)
                reject(WebCore::ExceptionData { WebCore::InvalidStateError, "Provided applicationServerKey does not match the key in the existing subscription."_s });
            else
                fulfill(makePushSubscriptionFromRecord(WTFMove(*result)));
            return;
        }

        auto topic = makePushTopic(m_identifier, m_scope);
        m_connection.subscribe(topic, m_vapidPublicKey, [this, isRetry, topic](NSString *endpoint, NSError *error) mutable {
            if (error) {
#if !HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)
                UNUSED_PARAM(isRetry);
#else
                // FIXME: use pointer comparison once APSURLTokenErrorDomain is externed.
                if (isRetry == IsRetry::No && [error.domain isEqualToString:@"APSURLTokenErrorDomain"] && error.code == APSURLTokenErrorCodeTopicAlreadyInFilter) {
                    attemptToRecoverFromTopicAlreadyInFilterError(WTFMove(topic));
                    return;
                }
#endif

                RELEASE_LOG_ERROR(Push, "PushManager.subscribe(%{public}s, scope: %{sensitive}s) failed with domain: %{public}s code: %lld)", m_identifier.debugDescription().utf8().data(), m_scope.utf8().data(), error.domain.UTF8String, static_cast<int64_t>(error.code));
                reject(WebCore::ExceptionData { WebCore::AbortError, "Failed due to internal service error"_s });
                return;
            }

            auto clientKeys = m_service.connection().generateClientKeys();
            PushRecord record {
                .subscriptionSetIdentifier = m_identifier,
                .securityOrigin = SecurityOrigin::createFromString(m_scope)->data().toString(),
                .scope = m_scope,
                .endpoint = endpoint,
                .topic = WTFMove(topic),
                .serverVAPIDPublicKey = m_vapidPublicKey,
                .clientPublicKey = WTFMove(clientKeys.clientP256DHKeyPair.publicKey),
                .clientPrivateKey = WTFMove(clientKeys.clientP256DHKeyPair.privateKey),
                .sharedAuthSecret = WTFMove(clientKeys.sharedAuthSecret)
            };

            m_database.insertRecord(record, [this](auto&& result) mutable {
                if (!result) {
                    RELEASE_LOG_ERROR(Push, "PushManager.subscribe(%{public}s, scope: %{sensitive}s) failed with database error", m_identifier.debugDescription().utf8().data(), m_scope.utf8().data());
                    reject(WebCore::ExceptionData { WebCore::AbortError, "Failed due to internal database error"_s });
                    return;
                }

                // FIXME: support partial topic list updates.
                updateTopicLists(m_connection, m_database, [this, record = WTFMove(*result)]() mutable {
                    fulfill(makePushSubscriptionFromRecord(WTFMove(record)));
                });
            });
        });
    });
}

// Tries to recover from a topic being moved to the global paused filter (rdar://88139330).
void SubscribeRequest::attemptToRecoverFromTopicAlreadyInFilterError(String&& topic)
{
#if !HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)
    UNUSED_PARAM(topic);
#else
    WorkQueue::main().dispatch([this, topic = WTFMove(topic)]() mutable {
        // This takes ownership of the paused topic and tells apsd to forget about the topic.
        auto originalTopics = m_connection.ignoredTopics();
        auto augmentedTopics = originalTopics;
        augmentedTopics.append(topic);
        m_connection.setIgnoredTopics(WTFMove(augmentedTopics));
        m_connection.setIgnoredTopics(WTFMove(originalTopics));

        WorkQueue::main().dispatch([this]() mutable {
            startImpl(IsRetry::Yes);
        });
    });
#endif
}

class UnsubscribeRequest : public PushServiceRequestImpl<bool> {
public:
    UnsubscribeRequest(PushService&, const PushSubscriptionSetIdentifier&, const String& scope, std::optional<PushSubscriptionIdentifier>, ResultHandler&&);
    virtual ~UnsubscribeRequest() = default;

protected:
    ASCIILiteral description() const final { return "UnsubscribeRequest"_s; }
    void startInternal() final;
    void finish() final { m_service.didCompleteUnsubscribeRequest(*this); }

private:
    std::optional<PushSubscriptionIdentifier> m_subscriptionIdentifier;
};

UnsubscribeRequest::UnsubscribeRequest(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, std::optional<PushSubscriptionIdentifier> subscriptionIdentifier, ResultHandler&& resultHandler)
    : PushServiceRequestImpl(service, identifier, scope, WTFMove(resultHandler))
    , m_subscriptionIdentifier(subscriptionIdentifier)
{
}

// Implements the webpushd side of PushSubscription.unsubscribe.
void UnsubscribeRequest::startInternal()
{
    m_database.getRecordBySubscriptionSetAndScope(m_identifier, m_scope, [this](auto&& result) mutable {
        if (!result || (m_subscriptionIdentifier && *m_subscriptionIdentifier != result->identifier)) {
            fulfill(false);
            return;
        }
        
        m_database.removeRecordByIdentifier(result->identifier, [this, serverVAPIDPublicKey = result->serverVAPIDPublicKey](bool removed) mutable {
            if (!removed) {
                fulfill(false);
                return;
            }

            // FIXME: support partial topic list updates.
            updateTopicLists(m_connection, m_database, [this]() mutable {
                fulfill(true);
            });

            auto topic = makePushTopic(m_identifier, m_scope);
            m_connection.unsubscribe(topic, serverVAPIDPublicKey, [this](bool unsubscribed, NSError *error) mutable {
                RELEASE_LOG_ERROR_IF(!unsubscribed, Push, "PushSubscription.unsubscribe(%{public}s scope: %{sensitive}s) failed with domain: %{public}s code: %lld)", m_identifier.debugDescription().utf8().data(), m_scope.utf8().data(), error.domain.UTF8String ?: "none", static_cast<int64_t>(error.code));
            });
        });
    });
}

// Only allow one request per (bundleIdentifier, dataStoreIdentifier, scope) to proceed at once. For
// instance, if a given page calls PushManager.subscribe() twice in a row, the second subscribe call
// won't start until the first one completes.
void PushService::enqueuePushServiceRequest(PushServiceRequestMap& map, std::unique_ptr<PushServiceRequest>&& request)
{
    auto addResult = map.ensure(request->key(), []() {
        return Deque<std::unique_ptr<PushServiceRequest>> { };
    });

    auto addedRequest = request.get();
    auto& queue = addResult.iterator->value;

    RELEASE_LOG(Push, "Enqueuing PushServiceRequest %p (current queue size: %zu)", request.get(), queue.size());
    queue.append(WTFMove(request));

    if (addResult.isNewEntry)
        addedRequest->start();
}

void PushService::finishedPushServiceRequest(PushServiceRequestMap& map, PushServiceRequest& request)
{
    auto requestQueueIt = map.find(request.key());

    RELEASE_ASSERT(requestQueueIt != map.end());
    auto& requestQueue = requestQueueIt->value;
    RELEASE_ASSERT(requestQueue.size() > 0);
    auto currentRequest = requestQueue.takeFirst();
    RELEASE_ASSERT(currentRequest.get() == &request);

    PushServiceRequest* nextRequest = nullptr;
    if (!requestQueue.size())
        map.remove(requestQueueIt);
    else
        nextRequest = requestQueue.first().get();

    // Even if there's no next request to start, hold on to currentRequest until the next turn of the run loop since we're in the middle of executing the finish() member function of currentRequest.
    WorkQueue::main().dispatch([currentRequest = WTFMove(currentRequest), nextRequest] {
        if (nextRequest)
            nextRequest->start();
    });
}

void PushService::getSubscription(const PushSubscriptionSetIdentifier& identifier, const String& scope, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& completionHandler)
{
    if (identifier.bundleIdentifier.isEmpty() || scope.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring getSubscription request with bundleIdentifier (empty = %d) and scope (empty = %d)", identifier.bundleIdentifier.isEmpty(), scope.isEmpty());
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::AbortError, "Invalid sender"_s }));
        return;
    }

    enqueuePushServiceRequest(m_getSubscriptionRequests, makeUnique<GetSubscriptionRequest>(*this, identifier, scope, WTFMove(completionHandler)));
}

void PushService::didCompleteGetSubscriptionRequest(GetSubscriptionRequest& request)
{
    finishedPushServiceRequest(m_getSubscriptionRequests, request);
}

void PushService::subscribe(const PushSubscriptionSetIdentifier& identifier, const String& scope, const Vector<uint8_t>& vapidPublicKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& completionHandler)
{
    if (identifier.bundleIdentifier.isEmpty() || scope.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring subscribe request with bundleIdentifier (empty = %d) and scope (empty = %d)", identifier.bundleIdentifier.isEmpty(), scope.isEmpty());
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::AbortError, "Invalid sender"_s }));
        return;
    }

    enqueuePushServiceRequest(m_subscribeRequests, makeUnique<SubscribeRequest>(*this, identifier, scope, vapidPublicKey, WTFMove(completionHandler)));
}

void PushService::didCompleteSubscribeRequest(SubscribeRequest& request)
{
    finishedPushServiceRequest(m_subscribeRequests, request);
}

void PushService::unsubscribe(const PushSubscriptionSetIdentifier& identifier, const String& scope, std::optional<PushSubscriptionIdentifier> subscriptionIdentifier, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& completionHandler)
{
    if (identifier.bundleIdentifier.isEmpty() || scope.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring unsubscribe request with bundleIdentifier (empty = %d) and scope (empty = %d)", identifier.bundleIdentifier.isEmpty(), scope.isEmpty());
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::AbortError, "Invalid sender"_s }));
        return;
    }

    enqueuePushServiceRequest(m_unsubscribeRequests, makeUnique<UnsubscribeRequest>(*this, identifier, scope, subscriptionIdentifier, WTFMove(completionHandler)));
}

void PushService::didCompleteUnsubscribeRequest(UnsubscribeRequest& request)
{
    finishedPushServiceRequest(m_unsubscribeRequests, request);
}

void PushService::incrementSilentPushCount(const PushSubscriptionSetIdentifier& identifier, const String& securityOrigin, CompletionHandler<void(unsigned)>&& handler)
{
    if (identifier.bundleIdentifier.isEmpty() || securityOrigin.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring incrementSilentPushCount request with bundleIdentifier (empty = %d) and securityOrigin (empty = %d)", identifier.bundleIdentifier.isEmpty(), securityOrigin.isEmpty());
        handler(0);
        return;
    }

    m_database->incrementSilentPushCount(identifier, securityOrigin, [this, identifier, securityOrigin, handler = WTFMove(handler)](unsigned silentPushCount) mutable {
        if (silentPushCount < WebKit::WebPushD::maxSilentPushCount) {
            handler(silentPushCount);
            return;
        }

        RELEASE_LOG(Push, "Removing all subscriptions associated with %{public}s %{sensitive}s since it processed %u silent pushes", identifier.debugDescription().utf8().data(), securityOrigin.utf8().data(), silentPushCount);

        removeRecordsImpl(identifier, securityOrigin, [handler = WTFMove(handler), silentPushCount](auto&&) mutable {
            handler(silentPushCount);
        });
    });
}

void PushService::setPushesEnabledForSubscriptionSetAndOrigin(const PushSubscriptionSetIdentifier& identifier, const String& securityOrigin, bool enabled, CompletionHandler<void()>&& handler)
{
    if (identifier.bundleIdentifier.isEmpty() || securityOrigin.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring setPushesEnabledForBundleIdentifierAndOrigin request with bundleIdentifier (empty = %d) and securityOrigin (empty = %d)", identifier.bundleIdentifier.isEmpty(), securityOrigin.isEmpty());
        return handler();
    }

    m_database->setPushesEnabledForOrigin(identifier, securityOrigin, enabled, [this, handler = WTFMove(handler)](bool recordsChanged) mutable {
        if (!recordsChanged)
            return handler();
        updateTopicLists(m_connection, m_database, WTFMove(handler));
    });
}

void PushService::removeRecordsForSubscriptionSet(const PushSubscriptionSetIdentifier& identifier, CompletionHandler<void(unsigned)>&& handler)
{
    RELEASE_LOG(Push, "Removing push subscriptions associated with %{public}s", identifier.debugDescription().utf8().data());
    removeRecordsImpl(identifier, std::nullopt, WTFMove(handler));
}

void PushService::removeRecordsForSubscriptionSetAndOrigin(const PushSubscriptionSetIdentifier& identifier, const String& securityOrigin, CompletionHandler<void(unsigned)>&& handler)
{
    RELEASE_LOG(Push, "Removing push subscriptions associated with %{public}s %{sensitive}s", identifier.debugDescription().utf8().data(), securityOrigin.utf8().data());
    removeRecordsImpl(identifier, securityOrigin, WTFMove(handler));
}

void PushService::removeRecordsImpl(const PushSubscriptionSetIdentifier& identifier, const std::optional<String>& securityOrigin, CompletionHandler<void(unsigned)>&& handler)
{
    if (identifier.bundleIdentifier.isEmpty() || (securityOrigin && securityOrigin->isEmpty())) {
        RELEASE_LOG_ERROR(Push, "Ignoring removeRecordsImpl request with bundleIdentifier (empty = %d) and securityOrigin (empty = %d)", identifier.bundleIdentifier.isEmpty(), securityOrigin && securityOrigin->isEmpty());
        handler(0);
        return;
    }

    auto removedRecordsHandler = [this, identifier, securityOrigin, handler = WTFMove(handler)](Vector<RemovedPushRecord>&& removedRecords) mutable {
        for (auto& record : removedRecords) {
            m_connection->unsubscribe(record.topic, record.serverVAPIDPublicKey, [topic = record.topic](bool unsubscribed, NSError* error) {
                RELEASE_LOG_ERROR_IF(!unsubscribed, Push, "removeRecordsImpl couldn't remove subscription for topic %{sensitive}s: %{public}s code: %lld)", topic.utf8().data(), error.domain.UTF8String ?: "none", static_cast<int64_t>(error.code));
            });
        }

        updateTopicLists(m_connection, m_database, [count = removedRecords.size(), handler = WTFMove(handler)]() mutable {
            handler(count);
        });
    };

    if (securityOrigin)
        m_database->removeRecordsBySubscriptionSetAndSecurityOrigin(identifier, *securityOrigin, WTFMove(removedRecordsHandler));
    else
        m_database->removeRecordsBySubscriptionSet(identifier, WTFMove(removedRecordsHandler));
}

enum class ContentEncoding {
    Empty,
    AESGCM,
    AES128GCM
};

struct RawPushMessage {
    String topic;
    ContentEncoding encoding;

    // Only set if encoding is not ContentEncoding::Empty.
    Vector<uint8_t> encryptedPayload;

    // Only set if encoding is ContentEncoding::AESGCM.
    Vector<uint8_t> serverPublicKey;
    Vector<uint8_t> salt;
};

static std::optional<Vector<uint8_t>> base64URLDecode(NSString *string)
{
    String coreString = string;
    return WTF::base64URLDecode(coreString);
}

static std::optional<Vector<uint8_t>> base64Decode(NSString *string)
{
    String coreString = string;
    return WTF::base64Decode(coreString);
}

static std::optional<RawPushMessage> makeRawPushMessage(NSString *topic, NSDictionary* userInfo)
{
    RawPushMessage message;

    @autoreleasepool {
        message.topic = topic;
        NSString *contentEncoding = userInfo[@"content_encoding"];
        NSString *payloadBase64 = userInfo[@"payload"];

        if (!contentEncoding.length || !payloadBase64.length) {
            message.encoding = ContentEncoding::Empty;
            return message;
        }

        if ([contentEncoding isEqualToString:@"aes128gcm"])
            message.encoding = ContentEncoding::AES128GCM;
        else if ([contentEncoding isEqualToString:@"aesgcm"]) {
            message.encoding = ContentEncoding::AESGCM;

            NSString *serverPublicKeyBase64URL = userInfo[@"as_publickey"];
            NSString *saltBase64URL = userInfo[@"as_salt"];
            if (!serverPublicKeyBase64URL || !saltBase64URL) {
                RELEASE_LOG_ERROR(Push, "Dropping aesgcm-encrypted push without required server key and salt");
                return std::nullopt;
            }

            auto serverPublicKey = base64URLDecode(serverPublicKeyBase64URL);
            auto salt = base64URLDecode(saltBase64URL);
            if (!serverPublicKey || !salt) {
                RELEASE_LOG_ERROR(Push, "Dropping aesgcm-encrypted push with improperly encoded server key and salt");
                return std::nullopt;
            }

            message.serverPublicKey = WTFMove(*serverPublicKey);
            message.salt = WTFMove(*salt);
        } else {
            RELEASE_LOG_ERROR(Push, "Dropping push with unknown content encoding: %{public}s", contentEncoding.UTF8String);
            return std::nullopt;
        }

        auto payload = base64Decode(payloadBase64);
        if (!payload) {
            RELEASE_LOG_ERROR(Push, "Dropping push with improperly encoded payload");
            return std::nullopt;
        }
        message.encryptedPayload = WTFMove(*payload);
    }

    return message;
}

void PushService::setPublicTokenForTesting(Vector<uint8_t>&& token)
{
    m_connection->setPublicTokenForTesting(WTFMove(token));
}

void PushService::didReceivePublicToken(Vector<uint8_t>&& token)
{
    m_database->updatePublicToken(token, [this](auto result) mutable {
        if (result == PushDatabase::PublicTokenChanged::No) {
            RELEASE_LOG(Push, "Received expected public token");
            return;
        }

        RELEASE_LOG_ERROR(Push, "Public token changed; invalidated all existing push subscriptions");
        updateTopicLists(m_connection, m_database, []() { });
    });
}

void PushService::didReceivePushMessage(NSString* topic, NSDictionary* userInfo, CompletionHandler<void()>&& completionHandler)
{
    auto transaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.push-service.incoming-push"));

    auto messageResult = makeRawPushMessage(topic, userInfo);
    if (!messageResult)
        return;

    m_database->getRecordByTopic(topic, [this, message = WTFMove(*messageResult), completionHandler = WTFMove(completionHandler), transaction = WTFMove(transaction)](auto&& recordResult) mutable {
        if (!recordResult) {
            RELEASE_LOG_ERROR(Push, "Dropping incoming push sent to unknown topic: %{sensitive}s", message.topic.utf8().data());
            completionHandler();
            return;
        }
        auto record = WTFMove(*recordResult);

        if (message.encoding == ContentEncoding::Empty) {
            m_incomingPushMessageHandler(record.subscriptionSetIdentifier, WebKit::WebPushMessage { { }, record.subscriptionSetIdentifier.pushPartition, URL { record.scope } });

            completionHandler();
            return;
        }

        PushCrypto::ClientKeys clientKeys {
            { WTFMove(record.clientPublicKey), WTFMove(record.clientPrivateKey) },
            WTFMove(record.sharedAuthSecret)
        };

        std::optional<Vector<uint8_t>> decryptedPayload;
        if (message.encoding == ContentEncoding::AES128GCM)
            decryptedPayload = decryptAES128GCMPayload(clientKeys, message.encryptedPayload);
        else if (message.encoding == ContentEncoding::AESGCM)
            decryptedPayload = decryptAESGCMPayload(clientKeys, message.serverPublicKey, message.salt, message.encryptedPayload);

        if (!decryptedPayload) {
            RELEASE_LOG_ERROR(Push, "Dropping incoming push due to decryption error for topic %{sensitive}s", message.topic.utf8().data());
            completionHandler();
            return;
        }

        RELEASE_LOG(Push, "Decoded incoming push message for %{public}s %{sensitive}s", record.subscriptionSetIdentifier.debugDescription().utf8().data(), record.scope.utf8().data());

        m_incomingPushMessageHandler(record.subscriptionSetIdentifier, WebKit::WebPushMessage { WTFMove(*decryptedPayload), record.subscriptionSetIdentifier.pushPartition, URL { record.scope } });

        completionHandler();
    });
}

} // namespace WebPushD
