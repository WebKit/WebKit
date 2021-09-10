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
#import "PCMDaemonEntryPoint.h"

#import "PrivateClickMeasurementConnection.h"
#import "PrivateClickMeasurementManagerInterface.h"
#import "PrivateClickMeasurementXPCUtilities.h"
#import <Foundation/Foundation.h>
#import <wtf/CompletionHandler.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/XPCSPI.h>

// FIXME: Add daemon plist to repository.

namespace WebKit {

static HashSet<RetainPtr<xpc_connection_t>>& peers()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<HashSet<RetainPtr<xpc_connection_t>>> set;
    return set.get();
}

static CompletionHandler<void(PCM::EncodedMessage&&)> replySender(PCM::MessageType messageType, OSObjectPtr<xpc_object_t>&& request)
{
    if (!PCM::messageTypeSendsReply(messageType))
        return nullptr;
    return [request = WTFMove(request)] (PCM::EncodedMessage&& message) {
        auto reply = adoptOSObject(xpc_dictionary_create_reply(request.get()));
        PCM::addVersionAndEncodedMessageToDictionary(WTFMove(message), reply.get());
        xpc_connection_send_message(xpc_dictionary_get_remote_connection(request.get()), reply.get());
    };
}

static void connectionEventHandler(xpc_object_t request)
{
    if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
        return;
    if (xpc_dictionary_get_uint64(request, PCM::protocolVersionKey) != PCM::protocolVersionValue) {
        NSLog(@"received request that was not the current protocol version");
        return;
    }

    auto messageType { static_cast<PCM::MessageType>(xpc_dictionary_get_uint64(request, PCM::protocolMessageTypeKey)) };
    size_t dataSize { 0 };
    const void* data = xpc_dictionary_get_data(request, PCM::protocolEncodedMessageKey, &dataSize);
    PCM::EncodedMessage encodedMessage { static_cast<const uint8_t*>(data), dataSize };
    decodeMessageAndSendToManager(messageType, WTFMove(encodedMessage), replySender(messageType, request));
}

static void startListeningForMachServiceConnections()
{
    static NeverDestroyed<OSObjectPtr<xpc_connection_t>> listener = xpc_connection_create_mach_service("com.apple.webkit.adattributiond.service", dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
    xpc_connection_set_event_handler(listener.get().get(), ^(xpc_object_t peer) {
        if (xpc_get_type(peer) != XPC_TYPE_CONNECTION)
            return;

        // FIXME: Add an entitlement check here so that only the network process can successfully connect.

        xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
            if (event == XPC_ERROR_CONNECTION_INTERRUPTED) {
                NSLog(@"removing peer connection %p", peer);
                peers().remove(peer);
                return;
            }
            connectionEventHandler(event);
        });
        xpc_connection_set_target_queue(peer, dispatch_get_main_queue());
        xpc_connection_activate(peer);

        NSLog(@"adding peer connection %p", peer);
        peers().add(peer);
    });
    xpc_connection_activate(listener.get().get());
}

static void registerScheduledActivityHandler()
{
    xpc_activity_register("com.apple.webkit.adattributiond.activity", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        if (xpc_activity_get_state(activity) == XPC_ACTIVITY_STATE_CHECK_IN) {
            xpc_object_t criteria = xpc_activity_copy_criteria(activity);

            // FIXME: set values here that align with values from the plist.

            xpc_activity_set_criteria(activity, criteria);
            return;
        }

        // FIXME: Add code here that does daily tasks of PrivateClickMeasurementManager.
    });
}

static void enterSandbox()
{
    // FIXME: Enter a sandbox here. We should only need read/write access to our database and network access and nothing else.
}

int PCMDaemonMain(int argc, const char** argv)
{
    if (argc != 2 || strcmp(argv[1], "runAsDaemon")) {
        NSLog(@"This executable is intended to be run as a daemon.");
        return -1;
    }
    @autoreleasepool {
        enterSandbox();
        startListeningForMachServiceConnections();
        registerScheduledActivityHandler();
        WTF::initializeMainThread();
    }
    CFRunLoopRun();
    return 0;
}

} // namespace WebKit
