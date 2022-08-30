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
#import "HandleXPCEndpointMessages.h"

#import "Connection.h"
#import "LaunchServicesDatabaseManager.h"
#import "LaunchServicesDatabaseXPCConstants.h"
#import "NetworkProcessEndpointClient.h"
#import "NetworkProcessEndpointXPCConstants.h"
#import "XPCEndpoint.h"

#import <wtf/text/WTFString.h>

namespace WebKit {

void handleXPCMessages(xpc_object_t event, const char* messageName)
{
    ASSERT_UNUSED(event, xpc_get_type(event) == XPC_TYPE_DICTIONARY);
    ASSERT_UNUSED(messageName, messageName);

#if ENABLE(XPC_IPC)
    if (IPC::Connection::handleXPCMessage(event))
        return;
#endif

    if (!strcmp(messageName, NetworkProcessEndpointXPCConstants::xpcNetworkProcessXPCEndpointMessageName)) {
        auto xpcEndPoint = xpc_dictionary_get_value(event, NetworkProcessEndpointXPCConstants::xpcNetworkProcessXPCEndpointNameKey);
        if (!xpcEndPoint || xpc_get_type(xpcEndPoint) != XPC_TYPE_ENDPOINT)
            return;
#if HAVE(LSDATABASECONTEXT)
        NetworkProcessEndpointClient::singleton().addObserver(WeakPtr { LaunchServicesDatabaseManager::singleton() });
#endif
        NetworkProcessEndpointClient::singleton().setEndpoint(xpcEndPoint);
        return;
    }
}

}
