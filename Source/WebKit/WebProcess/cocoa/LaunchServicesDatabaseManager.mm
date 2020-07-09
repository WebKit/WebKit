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
#import "XPCEndpoint.h"
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

LaunchServicesDatabaseManager& LaunchServicesDatabaseManager::singleton()
{
    static NeverDestroyed<LaunchServicesDatabaseManager> manager;
    return manager.get();
}

void LaunchServicesDatabaseManager::handleEvent(xpc_object_t message)
{
    String messageName = xpc_dictionary_get_string(message, XPCEndpoint::xpcMessageNameKey);
    if (messageName.isEmpty())
        return;
    if (messageName == LaunchServicesDatabaseXPCConstants::xpcUpdateLaunchServicesDatabaseMessageName) {
#if HAVE(LSDATABASECONTEXT)
        auto database = xpc_dictionary_get_value(message, LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseKey);

        if (database) {
            auto context = [NSClassFromString(@"LSDatabaseContext") sharedDatabaseContext];
            if (![context respondsToSelector:@selector(observeDatabaseChange4WebKit:)])
                return;
            [context observeDatabaseChange4WebKit:database];
        }
#endif
        m_semaphore.signal();
        m_hasReceivedLaunchServicesDatabase = true;
    }
}

void LaunchServicesDatabaseManager::didConnect()
{
    auto message = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(message.get(), XPCEndpoint::xpcMessageNameKey, LaunchServicesDatabaseXPCConstants::xpcRequestLaunchServicesDatabaseUpdateMessageName);

    xpc_connection_send_message(connection().get(), message.get());
}

bool LaunchServicesDatabaseManager::waitForDatabaseUpdate(Seconds timeout)
{
    if (m_hasReceivedLaunchServicesDatabase)
        return true;
    return m_semaphore.waitFor(timeout);
}

}
