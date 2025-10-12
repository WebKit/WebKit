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

#import "config.h"
#import "ApplePushServiceConnection.h"

#if HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)

#import <wtf/BlockPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/darwin/DispatchExtras.h>

@interface _WKAPSConnectionDelegate : NSObject<APSConnectionDelegate> {
    WeakPtr<WebPushD::ApplePushServiceConnection> _connection;
}
@end

@implementation _WKAPSConnectionDelegate

- (instancetype)initWithConnection:(WebPushD::ApplePushServiceConnection *)connection
{
    if ((self = [super init]))
        _connection = connection;
    return self;
}

- (void)connection:(APSConnection *)connection didReceivePublicToken:(NSData *)publicToken
{
    UNUSED_PARAM(connection);
    ASSERT(isMainRunLoop());

    if (RefPtr connection = _connection.get(); connection && publicToken.length)
        connection->didReceivePublicToken(makeVector(publicToken));
}

- (void)connection:(APSConnection *)connection didReceiveIncomingMessage:(APSIncomingMessage *)message
{
    UNUSED_PARAM(connection);
    ASSERT(isMainRunLoop());

    if (RefPtr connection = _connection.get())
        connection->didReceivePushMessage(message.topic, message.userInfo);
}

@end

namespace WebPushD {

ApplePushServiceConnection::ApplePushServiceConnection(const String& incomingPushServiceName)
{
    m_connection = adoptNS([[APSConnection alloc] initWithEnvironmentName:APSEnvironmentProduction namedDelegatePort:incomingPushServiceName.createNSString().get() queue:mainDispatchQueueSingleton()]);
    m_delegate = adoptNS([[_WKAPSConnectionDelegate alloc] initWithConnection:this]);
    [m_connection setDelegate:m_delegate.get()];
}

ApplePushServiceConnection::~ApplePushServiceConnection()
{
    [m_connection setDelegate:nil];
}

static RetainPtr<APSURLTokenInfo> makeTokenInfo(const String& topic, const Vector<uint8_t>& vapidPublicKey)
{
    return adoptNS([[APSURLTokenInfo alloc] initWithTopic:topic.createNSString().get() vapidPublicKey:toNSData(vapidPublicKey).get()]);
}

void ApplePushServiceConnection::subscribe(const String& topic, const Vector<uint8_t>& vapidPublicKey, SubscribeHandler&& subscribeHandler)
{
    ASSERT(isMainRunLoop());

    // Stash the completion handler away and look it up by id so that we can ensure it gets destructed on the main thread. If we move the handler and capture it in the Obj-C block, it might get destructed on a secondary thread since this completion block moves between different dispatch queues in the APS implementation.
    auto identifier = ++m_handlerIdentifier;
    m_subscribeHandlers.add(identifier, WTFMove(subscribeHandler));

    [m_connection requestURLTokenForInfo:makeTokenInfo(topic, vapidPublicKey).get() completion:makeBlockPtr([weakThis = WeakPtr { *this }, identifier] (APSURLToken *token, NSError *error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto handler = protectedThis->m_subscribeHandlers.take(identifier);
        handler(token.tokenURL, error);
    }).get()];
}

void ApplePushServiceConnection::unsubscribe(const String& topic, const Vector<uint8_t>& vapidPublicKey, UnsubscribeHandler&& unsubscribeHandler)
{
    ASSERT(isMainRunLoop());

    // See subscribe for why we stash the handler into a map.
    auto identifier = ++m_handlerIdentifier;
    m_unsubscribeHandlers.add(identifier, WTFMove(unsubscribeHandler));

    [m_connection invalidateURLTokenForInfo:makeTokenInfo(topic, vapidPublicKey).get() completion:makeBlockPtr([weakThis = WeakPtr { *this }, identifier] (BOOL success, NSError *error) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        auto handler = protectedThis->m_unsubscribeHandlers.take(identifier);
        handler(success, error);
    }).get()];
}

Vector<String> ApplePushServiceConnection::enabledTopics()
{
    return makeVector<String>([m_connection enabledTopics]);
}

Vector<String> ApplePushServiceConnection::ignoredTopics()
{
    return makeVector<String>([m_connection ignoredTopics]);
}

Vector<String> ApplePushServiceConnection::opportunisticTopics()
{
    return makeVector<String>([m_connection opportunisticTopics]);
}

Vector<String> ApplePushServiceConnection::nonWakingTopics()
{
    return makeVector<String>([m_connection nonWakingTopics]);
}

void ApplePushServiceConnection::setEnabledTopics(Vector<String>&& topics)
{
    [m_connection _setEnabledTopics:createNSArray(WTFMove(topics)).get()];
}

void ApplePushServiceConnection::setIgnoredTopics(Vector<String>&& topics)
{
    [m_connection _setIgnoredTopics:createNSArray(WTFMove(topics)).get()];
}

void ApplePushServiceConnection::setOpportunisticTopics(Vector<String>&& topics)
{
    [m_connection _setOpportunisticTopics:createNSArray(WTFMove(topics)).get()];
}

void ApplePushServiceConnection::setNonWakingTopics(Vector<String>&& topics)
{
    [m_connection _setNonWakingTopics:createNSArray(WTFMove(topics)).get()];
}

void ApplePushServiceConnection::setTopicLists(TopicLists&& topicLists)
{
    [m_connection setEnabledTopics:createNSArray(WTFMove(topicLists.enabledTopics)).get() ignoredTopics:createNSArray(WTFMove(topicLists.ignoredTopics)).get() opportunisticTopics:createNSArray(WTFMove(topicLists.opportunisticTopics)).get() nonWakingTopics:createNSArray(WTFMove(topicLists.nonWakingTopics)).get()];
}

} // namespace WebPushD

#endif // HAVE(APPLE_PUSH_SERVICE_URL_TOKEN_SUPPORT)
