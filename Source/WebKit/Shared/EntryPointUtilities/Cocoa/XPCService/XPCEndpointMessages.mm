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
#import "XPCEndpointMessages.h"

#import "GPUConnectionToWebProcess.h"
#import "GPUProcess.h"
#import "LaunchServicesDatabaseManager.h"
#import "LaunchServicesDatabaseXPCConstants.h"
#import "RemoteMediaPlayerManagerProxy.h"
#import "VideoReceiverEndpointMessage.h"
#import "XPCEndpoint.h"
#import <wtf/RunLoop.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

#if HAVE(LSDATABASECONTEXT)
static void handleLaunchServiceDatabaseMessage(xpc_object_t message)
{
    RetainPtr xpcEndPoint = xpc_dictionary_get_value(message, LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointNameKey);
    if (!xpcEndPoint || xpc_get_type(xpcEndPoint.get()) != XPC_TYPE_ENDPOINT)
        return;

    LaunchServicesDatabaseManager::singleton().setEndpoint(xpcEndPoint.get());
}
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
static void handleVideoReceiverEndpointMessage(xpc_object_t message)
{
    ASSERT(isMainRunLoop());
    RELEASE_ASSERT(isInGPUProcess());

    auto endpointMessage = VideoReceiverEndpointMessage::decode(message);
    if (!endpointMessage.processIdentifier())
        return;

    if (RefPtr webProcessConnection = GPUProcess::singleton().webProcessConnection(*endpointMessage.processIdentifier()))
        webProcessConnection->remoteMediaPlayerManagerProxy().handleVideoReceiverEndpointMessage(endpointMessage);
}

static void handleVideoReceiverSwapEndpointsMessage(xpc_object_t message)
{
    ASSERT(isMainRunLoop());
    RELEASE_ASSERT(isInGPUProcess());

    auto endpointMessage = VideoReceiverSwapEndpointsMessage::decode(message);
    if (!endpointMessage.processIdentifier())
        return;

    if (RefPtr webProcessConnection = GPUProcess::singleton().webProcessConnection(*endpointMessage.processIdentifier()))
        webProcessConnection->remoteMediaPlayerManagerProxy().handleVideoReceiverSwapEndpointsMessage(endpointMessage);
}
#endif

void handleXPCEndpointMessage(xpc_object_t message, const String& messageName)
{
    ASSERT_UNUSED(messageName, messageName);
    RELEASE_ASSERT(xpc_get_type(message) == XPC_TYPE_DICTIONARY);

#if HAVE(LSDATABASECONTEXT)
    if (messageName == LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointMessageName) {
        handleLaunchServiceDatabaseMessage(message);
        return;
    }
#endif

#if ENABLE(LINEAR_MEDIA_PLAYER)
    if (messageName == VideoReceiverEndpointMessage::messageName()) {
        RunLoop::mainSingleton().dispatch([message = OSObjectPtr(message)] {
            handleVideoReceiverEndpointMessage(message.get());
        });
        return;
    }

    if (messageName == VideoReceiverSwapEndpointsMessage::messageName()) {
        RunLoop::mainSingleton().dispatch([message = OSObjectPtr(message)] {
            handleVideoReceiverSwapEndpointsMessage(message.get());
        });
        return;
    }
#endif

}

} // namespace WebKit
