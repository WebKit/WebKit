/*
 * Copyright (C) 2017 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebSQLiteDatabaseTrackerClient.h"

#if PLATFORM(IOS_FAMILY)

#import "WebBackgroundTaskController.h"
#import <WebCore/DatabaseTracker.h>
#import <WebCore/SQLiteDatabaseTracker.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

@interface WebDatabaseTransactionBackgroundTaskController : NSObject
+ (void)startBackgroundTask;
+ (void)endBackgroundTask;
@end

namespace WebCore {

const Seconds hysteresisDuration { 2_s };

WebSQLiteDatabaseTrackerClient& WebSQLiteDatabaseTrackerClient::sharedWebSQLiteDatabaseTrackerClient()
{
    static NeverDestroyed<WebSQLiteDatabaseTrackerClient> client;
    return client;
}

WebSQLiteDatabaseTrackerClient::WebSQLiteDatabaseTrackerClient()
    : m_hysteresis([this](PAL::HysteresisState state) { hysteresisUpdated(state); }, hysteresisDuration)
{
    ASSERT(pthread_main_np());
}

WebSQLiteDatabaseTrackerClient::~WebSQLiteDatabaseTrackerClient()
{
}

void WebSQLiteDatabaseTrackerClient::willBeginFirstTransaction()
{
    dispatch_async(dispatch_get_main_queue(), [this] {
        m_hysteresis.start();
    });
}

void WebSQLiteDatabaseTrackerClient::didFinishLastTransaction()
{
    dispatch_async(dispatch_get_main_queue(), [this] {
        m_hysteresis.stop();
    });
}

void WebSQLiteDatabaseTrackerClient::hysteresisUpdated(PAL::HysteresisState state)
{
    ASSERT(pthread_main_np());
    if (state == PAL::HysteresisState::Started)
        [WebDatabaseTransactionBackgroundTaskController startBackgroundTask];
    else
        [WebDatabaseTransactionBackgroundTaskController endBackgroundTask];
}

}

static Lock transactionBackgroundTaskIdentifierLock;

static NSUInteger transactionBackgroundTaskIdentifier;

static void setTransactionBackgroundTaskIdentifier(NSUInteger identifier)
{
    transactionBackgroundTaskIdentifier = identifier;
}

static NSUInteger getTransactionBackgroundTaskIdentifier()
{
    static dispatch_once_t pred;
    dispatch_once(&pred, ^ {
        setTransactionBackgroundTaskIdentifier([[WebBackgroundTaskController sharedController] invalidBackgroundTaskIdentifier]);
    });

    return transactionBackgroundTaskIdentifier;
}

@implementation WebDatabaseTransactionBackgroundTaskController

+ (void)startBackgroundTask
{
    auto locker = holdLock(transactionBackgroundTaskIdentifierLock);

    // If there's already an existing background task going on, there's no need to start a new one.
    WebBackgroundTaskController *backgroundTaskController = [WebBackgroundTaskController sharedController];
    if (getTransactionBackgroundTaskIdentifier() != [backgroundTaskController invalidBackgroundTaskIdentifier])
        return;

    setTransactionBackgroundTaskIdentifier([backgroundTaskController startBackgroundTaskWithExpirationHandler:(^ {
        WebCore::DatabaseTracker::singleton().closeAllDatabases(WebCore::CurrentQueryBehavior::Interrupt);
        [self endBackgroundTask];
    })]);
}

+ (void)endBackgroundTask
{
    auto locker = holdLock(transactionBackgroundTaskIdentifierLock);

    // It is possible that we were unable to start the background task when the first transaction began.
    // Don't try to end the task in that case.
    // It is also possible we finally finish the last transaction right when the background task expires
    // and this will end up being called twice for the same background task. transactionBackgroundTaskIdentifier
    // will be invalid for the second caller.
    WebBackgroundTaskController *backgroundTaskController = [WebBackgroundTaskController sharedController];
    if (getTransactionBackgroundTaskIdentifier() == [backgroundTaskController invalidBackgroundTaskIdentifier])
        return;

    [backgroundTaskController endBackgroundTaskWithIdentifier:getTransactionBackgroundTaskIdentifier()];
    setTransactionBackgroundTaskIdentifier([backgroundTaskController invalidBackgroundTaskIdentifier]);
}

@end

#endif // PLATFORM(IOS_FAMILY)
