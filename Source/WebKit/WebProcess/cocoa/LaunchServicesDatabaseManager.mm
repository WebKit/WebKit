/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "LaunchServicesDatabaseManager.h"

#import "LaunchServicesDatabaseXPCConstants.h"
#import "Logging.h"
#import "XPCEndpoint.h"
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

LaunchServicesDatabaseManager& LaunchServicesDatabaseManager::singleton()
{
    static LazyNeverDestroyed<LaunchServicesDatabaseManager> manager;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        manager.construct();
    });
    return manager.get();
}

void LaunchServicesDatabaseManager::handleEvent(xpc_object_t message)
{
    auto* messageName = xpc_dictionary_get_string(message, XPCEndpoint::xpcMessageNameKey);
    if (LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName == messageName) {
#if HAVE(LSDATABASECONTEXT)
        auto database = xpc_dictionary_get_value(message, LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey);

        RELEASE_LOG_FORWARDABLE(Loading, RECEIVED_LAUNCH_SERVICES_DATABASE);

        if (database)
            [LSDatabaseContext.sharedDatabaseContext observeDatabaseChange4WebKit:database];
#endif
        m_semaphore.signal();
        m_hasReceivedLaunchServicesDatabase = true;
    }
}

void LaunchServicesDatabaseManager::didConnect()
{
    auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcRequestLaunchServicesDatabaseUpdateMessageName);

    auto connection = this->connection();
    if (!connection)
        return;

    xpc_connection_send_message(connection.get(), message.get());
}

bool LaunchServicesDatabaseManager::waitForDatabaseUpdate(Seconds timeout)
{
    if (m_hasReceivedLaunchServicesDatabase)
        return true;
    return m_semaphore.waitFor(timeout);
}

void LaunchServicesDatabaseManager::waitForDatabaseUpdate()
{
    auto startTime = MonotonicTime::now();
#ifdef NDEBUG
    constexpr auto waitTime = 5_s;
#else
    constexpr auto waitTime = 10_s;
#endif
    bool databaseUpdated = waitForDatabaseUpdate(waitTime);
    auto elapsedTime = MonotonicTime::now() - startTime;
    if (elapsedTime > 0.5_s)
        RELEASE_LOG_ERROR_FORWARDABLE(Loading, WAITING_FOR_LAUNCH_SERVICES_DATABASE_UPDATE_TOOK_F_SECONDS, elapsedTime.value());

    if (!databaseUpdated)
        RELEASE_LOG_FAULT_FORWARDABLE(Loading, TIMED_OUT_WAITING_FOR_LAUNCH_SERVICES_DATABASE_UPDATE);
}

}
