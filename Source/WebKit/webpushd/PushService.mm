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
#import <notify.h>
#import <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WorkQueue.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/Base64.h>
#import <wtf/text/MakeString.h>

#if HAVE(MOBILE_KEY_BAG)
#import <pal/spi/ios/MobileKeyBagSPI.h>
#endif

#if PLATFORM(IOS) || PLATFORM(VISION)
#import "UIKitSPI.h"
#endif

namespace WebPushD {
using namespace WebKit;
using namespace WebCore;

// The system has a limited number of push topics available. We don't want to monopolize all topics
// for ourselves, so let's limit ourselves to a reasonable amount of topics.
static constexpr size_t maxTopicCount = 64;

#if HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

#if !HAVE(MOBILE_KEY_BAG)

static void performAfterFirstUnlock(Function<void()>&& function)
{
    function();
}

#else

static bool hasUnlockedAtLeastOnce()
{
    // This function returns -1 on error, 0 on if the device has never unlocked, or 1 if the device has unlocked at least once.
    return MKBDeviceUnlockedSinceBoot() == 1;
}

static void performAfterFirstUnlock(Function<void()>&& function)
{
    static NeverDestroyed<Vector<Function<void()>>> functions;
    static int notifyToken = NOTIFY_TOKEN_INVALID;

    RELEASE_ASSERT(RunLoop::isMain());

    auto runFunctions = []() {
        RELEASE_LOG(Push, "Device has unlocked. Running initialization.");

        for (auto& function : functions.get())
            WorkQueue::main().dispatch(WTFMove(function));
        functions->clear();

        if (notifyToken != NOTIFY_TOKEN_INVALID) {
            notify_cancel(notifyToken);
            notifyToken = NOTIFY_TOKEN_INVALID;
        }
    };

    if (hasUnlockedAtLeastOnce()) {
        functions->append(WTFMove(function));
        runFunctions();
        return;
    }

    RELEASE_LOG(Push, "Device is locked. Delaying init until it unlocks for the first time.");

    if (notifyToken == NOTIFY_TOKEN_INVALID) {
        notify_register_dispatch(kMobileKeyBagLockStatusNotifyToken, &notifyToken, dispatch_get_main_queue(), ^(int token) {
            if (!notify_is_valid_token(token) || !hasUnlockedAtLeastOnce())
                return;
            runFunctions();
        });
    }

    functions->append(WTFMove(function));

    // Re-check the lock state after registering the notification. This covers the case where the
    // device unlocked in the time between the initial check and notification registration.
    if (hasUnlockedAtLeastOnce())
        runFunctions();
}

#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(PushService);

void PushService::create(const String& incomingPushServiceName, const String& databasePath, IncomingPushMessageHandler&& messageHandler, CompletionHandler<void(RefPtr<PushService>&&)>&& creationHandler)
{
    auto transaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.push-service-init"));

    // Create the connection ASAP so that we bootstrap_check_in to the service in a timely manner.
    auto connection = ApplePushServiceConnection::create(incomingPushServiceName);

    performAfterFirstUnlock([databasePath, transaction = WTFMove(transaction), connection = WTFMove(connection), messageHandler = WTFMove(messageHandler), creationHandler = WTFMove(creationHandler)]() mutable {
        PushDatabase::create(databasePath, [transaction, connection = WTFMove(connection), messageHandler = WTFMove(messageHandler), creationHandler = WTFMove(creationHandler)](auto&& databaseResult) mutable {
            if (!databaseResult) {
                RELEASE_LOG_ERROR(Push, "Push service initialization failed with database error");
                creationHandler(nullptr);
                return;
            }

            Ref service = adoptRef(*new PushService(WTFMove(connection), databaseResult.releaseNonNull(), WTFMove(messageHandler)));

            // Only provide the service object back to the caller after we've synced the topic lists in
            // the database with the PushServiceConnection/APSConnection. This ensures that we won't
            // service any calls to subscribe/unsubscribe/etc. until after the topic lists are up to
            // date, which APSConnection cares about.
            auto& serviceRef = service.get();
            serviceRef.updateTopicLists([transaction, service = WTFMove(service), creationHandler = WTFMove(creationHandler)]() mutable {
                creationHandler(WTFMove(service));
            });
        });
    });
}

#else

void PushService::create(const String&, const String&, IncomingPushMessageHandler&&, CompletionHandler<void(RefPtr<PushService>&&)>&& creationHandler)
{
    creationHandler(nullptr);
}

#endif // HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

void PushService::createMockService(IncomingPushMessageHandler&& messageHandler, CompletionHandler<void(RefPtr<PushService>&&)>&& creationHandler)
{
    PushDatabase::create(SQLiteDatabase::inMemoryPath(), [messageHandler = WTFMove(messageHandler), creationHandler = WTFMove(creationHandler)](auto&& databaseResult) mutable {
        if (!databaseResult) {
            creationHandler(nullptr);
            return;
        }

        auto connection = MockPushServiceConnection::create();
        creationHandler(adoptRef(*new PushService(WTFMove(connection), databaseResult.releaseNonNull(), WTFMove(messageHandler))));
    });
}

PushService::PushService(Ref<PushServiceConnection>&& pushServiceConnection, Ref<PushDatabase>&& pushDatabase, IncomingPushMessageHandler&& incomingPushMessageHandler)
    : m_connection(WTFMove(pushServiceConnection))
    , m_database(WTFMove(pushDatabase))
    , m_incomingPushMessageHandler(WTFMove(incomingPushMessageHandler))
{
    RELEASE_ASSERT(m_incomingPushMessageHandler);
    relaxAdoptionRequirement();

    Ref connection = m_connection;
    connection->startListeningForPublicToken([weakThis = WeakPtr { *this }](auto&& token) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didReceivePublicToken(WTFMove(token));
    });

    connection->startListeningForPushMessages([weakThis = WeakPtr { *this }](NSString *topic, NSDictionary *userInfo) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->didReceivePushMessage(topic, userInfo);
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

class PushServiceRequest : public AbstractRefCountedAndCanMakeWeakPtr<PushServiceRequest> {
public:
    virtual ~PushServiceRequest() = default;

    virtual ASCIILiteral description() const = 0;

    const PushSubscriptionSetIdentifier& subscriptionSetIdentifier() const { return m_identifier; }
    const String& scope() const { return m_scope; };

    virtual void start() = 0;

    String key() const { return makePushTopic(m_identifier, m_scope); }

protected:
    PushServiceRequest(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope)
        : m_identifier(identifier)
        , m_scope(scope)
        , m_service(service)
        , m_connection(service.connection())
        , m_database(service.database())
    {
    }

    PushService& service() const { return m_service.get(); }
    Ref<PushService> protectedService() const { return m_service.get(); }
    PushServiceConnection& connection() const { return m_connection.get(); }
    Ref<PushServiceConnection> protectedConnection() const { return m_connection.get(); }
    PushDatabase& database() const { return m_database.get(); }
    Ref<PushDatabase> protectedDatabase() const { return m_database.get(); }

    virtual void finish() = 0;

    PushSubscriptionSetIdentifier m_identifier;
    String m_scope;
    CompletionHandler<void(PushServiceRequest&)> m_completionHandler;

private:
    WeakRef<PushService> m_service;
    WeakRef<PushServiceConnection> m_connection;
    WeakRef<PushDatabase> m_database;
};

template<typename ResultType>
class PushServiceRequestImpl : public PushServiceRequest {
public:
    void start() final
    {
        if (m_identifier.bundleIdentifier.isEmpty() || m_scope.isEmpty()) {
            reject(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Invalid sender"_s });
            return;
        }
        
        String transactionDescription = makeString("com.apple.webkit.webpushd:"_s, description(), ':', m_identifier.debugDescription(), ':', m_scope);
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

class GetSubscriptionRequest final : public PushServiceRequestImpl<std::optional<WebCore::PushSubscriptionData>>, public RefCounted<GetSubscriptionRequest> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GetSubscriptionRequest);
public:
    static Ref<GetSubscriptionRequest> create(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, ResultHandler&& resultHandler)
    {
        return adoptRef(*new GetSubscriptionRequest(service, identifier, scope, WTFMove(resultHandler)));
    }

    virtual ~GetSubscriptionRequest() = default;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    GetSubscriptionRequest(PushService&, const PushSubscriptionSetIdentifier&, const String& scope, ResultHandler&&);

    ASCIILiteral description() const final { return "GetSubscriptionRequest"_s; }
    void startInternal() final;
    void finish() final { protectedService()->didCompleteGetSubscriptionRequest(*this); }
};

GetSubscriptionRequest::GetSubscriptionRequest(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, ResultHandler&& resultHandler)
    : PushServiceRequestImpl(service, identifier, scope, WTFMove(resultHandler))
{
}

// Implements the webpushd side of PushManager.getSubscription.
void GetSubscriptionRequest::startInternal()
{
    protectedDatabase()->getRecordBySubscriptionSetAndScope(m_identifier, m_scope, [weakThis = WeakPtr { *this }](auto&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!result) {
            protectedThis->fulfill(std::optional<WebCore::PushSubscriptionData> { });
            return;
        }

        protectedThis->fulfill(makePushSubscriptionFromRecord(WTFMove(*result)));
    });
}

class SubscribeRequest final : public PushServiceRequestImpl<WebCore::PushSubscriptionData>, public RefCounted<SubscribeRequest> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(SubscribeRequest);
public:
    static Ref<SubscribeRequest> create(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, const Vector<uint8_t>& vapidPublicKey, ResultHandler&& resultHandler)
    {
        return adoptRef(*new SubscribeRequest(service, identifier, scope, vapidPublicKey, WTFMove(resultHandler)));
    }

    virtual ~SubscribeRequest() = default;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    SubscribeRequest(PushService&, const PushSubscriptionSetIdentifier&, const String& scope, const Vector<uint8_t>& vapidPublicKey, ResultHandler&&);

    ASCIILiteral description() const final { return "SubscribeRequest"_s; }
    void startInternal() final { startImpl(IsRetry::No); }
    void finish() final { protectedService()->didCompleteSubscribeRequest(*this); }

    enum class IsRetry : bool { No, Yes };
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
    protectedDatabase()->getRecordBySubscriptionSetAndScope(m_identifier, m_scope, [weakThis = WeakPtr { *this }, isRetry](auto&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (result) {
            if (protectedThis->m_vapidPublicKey != result->serverVAPIDPublicKey)
                protectedThis->reject(WebCore::ExceptionData { WebCore::ExceptionCode::InvalidStateError, "Provided applicationServerKey does not match the key in the existing subscription."_s });
            else
                protectedThis->fulfill(makePushSubscriptionFromRecord(WTFMove(*result)));
            return;
        }

        auto topic = makePushTopic(protectedThis->m_identifier, protectedThis->m_scope);
        protectedThis->protectedConnection()->subscribe(topic, protectedThis->m_vapidPublicKey, [weakThis, isRetry, topic](NSString *endpoint, NSError *error) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (error) {
#if !HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)
                UNUSED_PARAM(isRetry);
#else
                // FIXME: use pointer comparison once APSURLTokenErrorDomain is externed.
                if (isRetry == IsRetry::No && [error.domain isEqualToString:@"APSURLTokenErrorDomain"] && error.code == APSURLTokenErrorCodeTopicAlreadyInFilter) {
                    protectedThis->attemptToRecoverFromTopicAlreadyInFilterError(WTFMove(topic));
                    return;
                }
#endif

                RELEASE_LOG_ERROR(Push, "PushManager.subscribe(%{public}s, scope: %{sensitive}s) failed with domain: %{public}s code: %lld)", protectedThis->m_identifier.debugDescription().utf8().data(), protectedThis->m_scope.utf8().data(), error.domain.UTF8String, static_cast<int64_t>(error.code));
                protectedThis->reject(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Failed due to internal service error"_s });
                return;
            }

            auto clientKeys = protectedThis->service().protectedConnection()->generateClientKeys();
            IGNORE_CLANG_WARNINGS_BEGIN("missing-designated-field-initializers")
            PushRecord record {
                .subscriptionSetIdentifier = protectedThis->m_identifier,
                .securityOrigin = SecurityOrigin::createFromString(protectedThis->m_scope)->data().toString(),
                .scope = protectedThis->m_scope,
                .endpoint = endpoint,
                .topic = WTFMove(topic),
                .serverVAPIDPublicKey = protectedThis->m_vapidPublicKey,
                .clientPublicKey = WTFMove(clientKeys.clientP256DHKeyPair.publicKey),
                .clientPrivateKey = WTFMove(clientKeys.clientP256DHKeyPair.privateKey),
                .sharedAuthSecret = WTFMove(clientKeys.sharedAuthSecret)
            };
            IGNORE_CLANG_WARNINGS_END

            protectedThis->protectedDatabase()->insertRecord(record, [weakThis](auto&& result) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                if (!result) {
                    RELEASE_LOG_ERROR(Push, "PushManager.subscribe(%{public}s, scope: %{sensitive}s) failed with database error", protectedThis->m_identifier.debugDescription().utf8().data(), protectedThis->m_scope.utf8().data());
                    protectedThis->reject(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Failed due to internal database error"_s });
                    return;
                }

                protectedThis->protectedService()->updateTopicLists([weakThis, record = WTFMove(*result)]() mutable {
                    if (RefPtr protectedThis = weakThis.get())
                        protectedThis->fulfill(makePushSubscriptionFromRecord(WTFMove(record)));
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
    WorkQueue::main().dispatch([this, weakThis = WeakPtr { *this }, topic = WTFMove(topic)]() mutable {
        if (!weakThis)
            return;

        // This takes ownership of the paused topic and tells apsd to forget about the topic.
        Ref connection = this->connection();
        auto originalTopics = connection->ignoredTopics();
        auto augmentedTopics = originalTopics;
        augmentedTopics.append(topic);
        connection->setIgnoredTopics(WTFMove(augmentedTopics));
        connection->setIgnoredTopics(WTFMove(originalTopics));

        WorkQueue::main().dispatch([weakThis = WTFMove(weakThis)]() mutable {
            if (RefPtr protectedThis = weakThis.get())
                protectedThis->startImpl(IsRetry::Yes);
        });
    });
#endif
}

class UnsubscribeRequest final : public PushServiceRequestImpl<bool>, public RefCounted<UnsubscribeRequest> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(UnsubscribeRequest);
public:
    static Ref<UnsubscribeRequest> create(PushService& service, const PushSubscriptionSetIdentifier& identifier, const String& scope, std::optional<PushSubscriptionIdentifier> subscriptionIdentifier, ResultHandler&& resultHandler)
    {
        return adoptRef(*new UnsubscribeRequest(service, identifier, scope, subscriptionIdentifier, WTFMove(resultHandler)));
    }

    virtual ~UnsubscribeRequest() = default;

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

private:
    UnsubscribeRequest(PushService&, const PushSubscriptionSetIdentifier&, const String& scope, std::optional<PushSubscriptionIdentifier>, ResultHandler&&);

    ASCIILiteral description() const final { return "UnsubscribeRequest"_s; }
    void startInternal() final;
    void finish() final { protectedService()->didCompleteUnsubscribeRequest(*this); }

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
    protectedDatabase()->getRecordBySubscriptionSetAndScope(m_identifier, m_scope, [weakThis = WeakPtr { *this }](auto&& result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (!result || (protectedThis->m_subscriptionIdentifier && *protectedThis->m_subscriptionIdentifier != result->identifier)) {
            protectedThis->fulfill(false);
            return;
        }
        
        protectedThis->protectedDatabase()->removeRecordByIdentifier(*result->identifier, [weakThis = WTFMove(weakThis), serverVAPIDPublicKey = result->serverVAPIDPublicKey](bool removed) mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;

            if (!removed) {
                protectedThis->fulfill(false);
                return;
            }

            // FIXME: support partial topic list updates.
            protectedThis->protectedService()->updateTopicLists([weakThis]() mutable {
                if (RefPtr protectedThis = weakThis.get())
                    protectedThis->fulfill(true);
            });

            auto topic = makePushTopic(protectedThis->m_identifier, protectedThis->m_scope);
            protectedThis->protectedConnection()->unsubscribe(topic, serverVAPIDPublicKey, [weakThis = WTFMove(weakThis)](bool unsubscribed, NSError *error) mutable {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                RELEASE_LOG_ERROR_IF(!unsubscribed, Push, "PushSubscription.unsubscribe(%{public}s scope: %{sensitive}s) failed with domain: %{public}s code: %lld)", protectedThis->m_identifier.debugDescription().utf8().data(), protectedThis->m_scope.utf8().data(), error.domain.UTF8String ?: "none", static_cast<int64_t>(error.code));
            });
        });
    });
}

// Only allow one request per (bundleIdentifier, dataStoreIdentifier, scope) to proceed at once. For
// instance, if a given page calls PushManager.subscribe() twice in a row, the second subscribe call
// won't start until the first one completes.
void PushService::enqueuePushServiceRequest(PushServiceRequestMap& map, Ref<PushServiceRequest>&& request)
{
    auto addResult = map.ensure(request->key(), []() {
        return Deque<Ref<PushServiceRequest>> { };
    });

    auto& queue = addResult.iterator->value;

    RELEASE_LOG(Push, "Enqueuing PushServiceRequest %p (current queue size: %zu)", request.ptr(), queue.size());
    queue.append(request);

    if (addResult.isNewEntry)
        request->start();
}

void PushService::finishedPushServiceRequest(PushServiceRequestMap& map, PushServiceRequest& request)
{
    auto requestQueueIt = map.find(request.key());

    RELEASE_ASSERT(requestQueueIt != map.end());
    auto& requestQueue = requestQueueIt->value;
    RELEASE_ASSERT(requestQueue.size() > 0);
    auto currentRequest = requestQueue.takeFirst();
    RELEASE_ASSERT(currentRequest.ptr() == &request);

    RefPtr<PushServiceRequest> nextRequest;
    if (!requestQueue.size())
        map.remove(requestQueueIt);
    else
        nextRequest = requestQueue.first().copyRef();

    // Even if there's no next request to start, hold on to currentRequest until the next turn of the run loop since we're in the middle of executing the finish() member function of currentRequest.
    WorkQueue::main().dispatch([currentRequest = WTFMove(currentRequest), nextRequest = WTFMove(nextRequest)] {
        if (nextRequest)
            nextRequest->start();
    });
}

void PushService::getSubscription(const PushSubscriptionSetIdentifier& identifier, const String& scope, CompletionHandler<void(const Expected<std::optional<WebCore::PushSubscriptionData>, WebCore::ExceptionData>&)>&& completionHandler)
{
    if (identifier.bundleIdentifier.isEmpty() || scope.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring getSubscription request with bundleIdentifier (empty = %d) and scope (empty = %d)", identifier.bundleIdentifier.isEmpty(), scope.isEmpty());
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Invalid sender"_s }));
        return;
    }

    enqueuePushServiceRequest(m_getSubscriptionRequests, GetSubscriptionRequest::create(*this, identifier, scope, WTFMove(completionHandler)));
}

void PushService::didCompleteGetSubscriptionRequest(GetSubscriptionRequest& request)
{
    finishedPushServiceRequest(m_getSubscriptionRequests, request);
}

void PushService::subscribe(const PushSubscriptionSetIdentifier& identifier, const String& scope, const Vector<uint8_t>& vapidPublicKey, CompletionHandler<void(const Expected<WebCore::PushSubscriptionData, WebCore::ExceptionData>&)>&& completionHandler)
{
    if (identifier.bundleIdentifier.isEmpty() || scope.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring subscribe request with bundleIdentifier (empty = %d) and scope (empty = %d)", identifier.bundleIdentifier.isEmpty(), scope.isEmpty());
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Invalid sender"_s }));
        return;
    }

    if (m_topicCount >= maxTopicCount) {
        RELEASE_LOG_ERROR(Push, "Subscribe request from %{public}s and scope %{sensitive}s failed: reached max push topic count", identifier.debugDescription().ascii().data(), scope.ascii().data());
        return completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotAllowedError, "Reached maximum push subscription count"_s }));
    }

    enqueuePushServiceRequest(m_subscribeRequests, SubscribeRequest::create(*this, identifier, scope, vapidPublicKey, WTFMove(completionHandler)));
}

void PushService::didCompleteSubscribeRequest(SubscribeRequest& request)
{
    finishedPushServiceRequest(m_subscribeRequests, request);
}

void PushService::unsubscribe(const PushSubscriptionSetIdentifier& identifier, const String& scope, std::optional<PushSubscriptionIdentifier> subscriptionIdentifier, CompletionHandler<void(const Expected<bool, WebCore::ExceptionData>&)>&& completionHandler)
{
    if (identifier.bundleIdentifier.isEmpty() || scope.isEmpty()) {
        RELEASE_LOG_ERROR(Push, "Ignoring unsubscribe request with bundleIdentifier (empty = %d) and scope (empty = %d)", identifier.bundleIdentifier.isEmpty(), scope.isEmpty());
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::AbortError, "Invalid sender"_s }));
        return;
    }

    enqueuePushServiceRequest(m_unsubscribeRequests, UnsubscribeRequest::create(*this, identifier, scope, subscriptionIdentifier, WTFMove(completionHandler)));
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

    protectedDatabase()->incrementSilentPushCount(identifier, securityOrigin, [weakThis = WeakPtr { *this }, identifier, securityOrigin, handler = WTFMove(handler)](unsigned silentPushCount) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return handler(0);

        if (silentPushCount < WebKit::WebPushD::maxSilentPushCount) {
            handler(silentPushCount);
            return;
        }

        RELEASE_LOG(Push, "Removing all subscriptions associated with %{public}s %{sensitive}s since it processed %u silent pushes", identifier.debugDescription().utf8().data(), securityOrigin.utf8().data(), silentPushCount);

        protectedThis->removeRecordsImpl(identifier, securityOrigin, [handler = WTFMove(handler), silentPushCount](auto&&) mutable {
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

    protectedDatabase()->setPushesEnabledForOrigin(identifier, securityOrigin, enabled, [weakThis = WeakPtr { *this }, handler = WTFMove(handler)](bool recordsChanged) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis || !recordsChanged)
            return handler();
        protectedThis->updateTopicLists(WTFMove(handler));
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

    auto removedRecordsHandler = [weakThis = WeakPtr { *this }, identifier, securityOrigin, handler = WTFMove(handler)](Vector<RemovedPushRecord>&& removedRecords) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return handler(removedRecords.size());

        Ref connection = protectedThis->connection();
        for (auto& record : removedRecords) {
            connection->unsubscribe(record.topic, record.serverVAPIDPublicKey, [topic = record.topic](bool unsubscribed, NSError* error) {
                RELEASE_LOG_ERROR_IF(!unsubscribed, Push, "removeRecordsImpl couldn't remove subscription for topic %{sensitive}s: %{public}s code: %lld)", topic.utf8().data(), error.domain.UTF8String ?: "none", static_cast<int64_t>(error.code));
            });
        }

        protectedThis->updateTopicLists([count = removedRecords.size(), handler = WTFMove(handler)]() mutable {
            handler(count);
        });
    };

    if (securityOrigin)
        protectedDatabase()->removeRecordsBySubscriptionSetAndSecurityOrigin(identifier, *securityOrigin, WTFMove(removedRecordsHandler));
    else
        protectedDatabase()->removeRecordsBySubscriptionSet(identifier, WTFMove(removedRecordsHandler));
}

void PushService::removeRecordsForBundleIdentifierAndDataStore(const String& bundleIdentifier, const std::optional<WTF::UUID>& dataStoreIdentifier, CompletionHandler<void(unsigned)>&& handler)
{
    RELEASE_LOG(Push, "Removing push subscriptions associated with %{public}s | ds: %{public}s", bundleIdentifier.utf8().data(), dataStoreIdentifier ? dataStoreIdentifier->toString().ascii().data() : "default");
    protectedDatabase()->removeRecordsByBundleIdentifierAndDataStore(bundleIdentifier, dataStoreIdentifier, [weakThis = WeakPtr { *this }, handler = WTFMove(handler)](auto&& removedRecords) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return handler(removedRecords.size());

        Ref connection = protectedThis->connection();
        for (auto& record : removedRecords) {
            connection->unsubscribe(record.topic, record.serverVAPIDPublicKey, [topic = record.topic](bool unsubscribed, NSError* error) {
                RELEASE_LOG_ERROR_IF(!unsubscribed, Push, "removeRecordsImpl couldn't remove subscription for topic %{sensitive}s: %{public}s code: %lld)", topic.utf8().data(), error.domain.UTF8String ?: "none", static_cast<int64_t>(error.code));
            });
        }

        protectedThis->updateTopicLists([count = removedRecords.size(), handler = WTFMove(handler)]() mutable {
            handler(count);
        });
    });
}

#if PLATFORM(IOS)

void PushService::updateSubscriptionSetState(const String& allowedBundleIdentifier, const HashSet<String>& installedWebClipIdentifiers, CompletionHandler<void()>&& completionHandler)
{
    protectedDatabase()->getPushSubscriptionSetRecords([weakThis = WeakPtr { *this }, allowedBundleIdentifier, installedWebClipIdentifiers, completionHandler = WTFMove(completionHandler)](auto&& records) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler();

        HashSet<PushSubscriptionSetIdentifier> identifiersToRemove;

        for (const auto& record : records) {
            auto bundleIdentifier = record.identifier.bundleIdentifier;
            auto webClipIdentifier = record.identifier.pushPartition;
            if (bundleIdentifier != allowedBundleIdentifier || !installedWebClipIdentifiers.contains(webClipIdentifier))
                identifiersToRemove.add(record.identifier);
        }

        if (identifiersToRemove.isEmpty()) {
            RELEASE_LOG(Push, "All push subscriptions are associated with existing web clips");
            return completionHandler();
        }

        Ref aggregator = MainRunLoopCallbackAggregator::create([weakThis, completionHandler = WTFMove(completionHandler)]() mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return completionHandler();

            // Set the APS topics filter again so that the topics we just unsubscribed from are no longer in the filter.
            protectedThis->updateTopicLists(WTFMove(completionHandler));
        });

        Ref database = protectedThis->m_database;
        for (const auto& identifier : identifiersToRemove) {
            RELEASE_LOG(Push, "No web clip matching push subscription set identifier %{public}s; removing", identifier.debugDescription().utf8().data());
            database->removeRecordsBySubscriptionSet(identifier, [weakThis, aggregator](auto&& records) {
                RefPtr protectedThis = weakThis.get();
                if (!protectedThis)
                    return;

                Ref connection = protectedThis->connection();
                for (auto& record : records) {
                    connection->unsubscribe(record.topic, record.serverVAPIDPublicKey, [topic = record.topic](bool unsubscribed, NSError* error) {
                        RELEASE_LOG_ERROR_IF(!unsubscribed, Push, "couldn't remove subscription for topic %{sensitive}s: %{public}s code: %lld)", topic.utf8().data(), error.domain.UTF8String ?: "none", static_cast<int64_t>(error.code));
                    });
                }
            });
        }
    });
}

#endif

void PushService::updateTopicLists(CompletionHandler<void()>&& completionHandler)
{
    protectedDatabase()->getTopics([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)](auto&& topics) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler();

        PushServiceConnection::TopicLists topicLists;
        topicLists.enabledTopics = WTFMove(topics.enabledTopics);
        topicLists.ignoredTopics = WTFMove(topics.ignoredTopics);
        protectedThis->protectedConnection()->setTopicLists(WTFMove(topicLists));
        protectedThis->m_topicCount = topicLists.enabledTopics.size() + topicLists.ignoredTopics.size();
        completionHandler();
    });
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
    protectedConnection()->setPublicTokenForTesting(WTFMove(token));
}

void PushService::didReceivePublicToken(Vector<uint8_t>&& token)
{
    protectedDatabase()->updatePublicToken(token, [weakThis = WeakPtr { *this }](auto result) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        if (result == PushDatabase::PublicTokenChanged::No) {
            RELEASE_LOG(Push, "Received expected public token");
            return;
        }

        RELEASE_LOG_ERROR(Push, "Public token changed; invalidated all existing push subscriptions");
        protectedThis->updateTopicLists([]() { });
    });
}

void PushService::didReceivePushMessage(NSString* topic, NSDictionary* userInfo, CompletionHandler<void()>&& completionHandler)
{
    auto transaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.push-service.incoming-push"));

    auto messageResult = makeRawPushMessage(topic, userInfo);
    if (!messageResult)
        return;

    protectedDatabase()->getRecordByTopic(topic, [weakThis = WeakPtr { *this }, message = WTFMove(*messageResult), completionHandler = WTFMove(completionHandler), transaction = WTFMove(transaction)](auto&& recordResult) mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return completionHandler();

        if (!recordResult) {
            RELEASE_LOG_ERROR(Push, "Dropping incoming push sent to unknown topic: %{sensitive}s", message.topic.utf8().data());
            completionHandler();
            return;
        }
        auto record = WTFMove(*recordResult);

        if (message.encoding == ContentEncoding::Empty) {
            protectedThis->m_incomingPushMessageHandler(record.subscriptionSetIdentifier, WebKit::WebPushMessage { { }, record.subscriptionSetIdentifier.pushPartition, URL { record.scope }, { } });
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

        protectedThis->m_incomingPushMessageHandler(record.subscriptionSetIdentifier, WebKit::WebPushMessage { WTFMove(*decryptedPayload), record.subscriptionSetIdentifier.pushPartition, URL { record.scope }, { } });
        completionHandler();
    });
}

} // namespace WebPushD
