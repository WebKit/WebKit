/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#import "DaemonDecoder.h"
#import "DaemonUtilities.h"
#import "PCMDaemonConnectionSet.h"
#import "PrivateClickMeasurementConnection.h"
#import "PrivateClickMeasurementManagerInterface.h"
#import "PrivateClickMeasurementXPCUtilities.h"
#import <Foundation/Foundation.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <wtf/CompletionHandler.h>
#import <wtf/FileSystem.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RunLoop.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/spi/darwin/XPCSPI.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

// FIXME: Add daemon plist to repository.

namespace WebKit {

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
        NSLog(@"Received request that was not the current protocol version");
        return;
    }

    auto messageType { static_cast<PCM::MessageType>(xpc_dictionary_get_uint64(request, PCM::protocolMessageTypeKey)) };
    auto encodedMessage = xpc_dictionary_get_data_span(request, PCM::protocolEncodedMessageKey);
    decodeMessageAndSendToManager(Daemon::Connection::create(xpc_dictionary_get_remote_connection(request)), messageType, encodedMessage, replySender(messageType, request));
}

static void registerScheduledActivityHandler()
{
    NSLog(@"Registering XPC activity");

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    xpc_activity_register("com.apple.webkit.adattributiond.activity", XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
        if (xpc_activity_get_state(activity) == XPC_ACTIVITY_STATE_CHECK_IN) {
            NSLog(@"Activity checking in");
            auto criteria = adoptOSObject(xpc_activity_copy_criteria(activity));

            // These values should align with values from com.apple.webkit.adattributiond.plist
            constexpr auto oneHourSeconds = 3600;
            constexpr auto oneDaySeconds = 24 * oneHourSeconds;
            xpc_dictionary_set_uint64(criteria.get(), XPC_ACTIVITY_INTERVAL, oneDaySeconds);
            xpc_dictionary_set_uint64(criteria.get(), XPC_ACTIVITY_GRACE_PERIOD, oneHourSeconds);
            xpc_dictionary_set_string(criteria.get(), XPC_ACTIVITY_PRIORITY, XPC_ACTIVITY_PRIORITY_MAINTENANCE);
            xpc_dictionary_set_bool(criteria.get(), XPC_ACTIVITY_ALLOW_BATTERY, true);
            xpc_dictionary_set_uint64(criteria.get(), XPC_ACTIVITY_RANDOM_INITIAL_DELAY, oneDaySeconds);
            xpc_dictionary_set_bool(criteria.get(), XPC_ACTIVITY_REQUIRE_NETWORK_CONNECTIVITY, true);
            xpc_dictionary_set_bool(criteria.get(), XPC_ACTIVITY_REPEATING, true);

            xpc_activity_set_criteria(activity, criteria.get());
            return;
        }
ALLOW_DEPRECATED_DECLARATIONS_END

        dispatch_async(dispatch_get_main_queue(), ^{
            NSLog(@"XPC activity happening");
            PCM::doDailyActivityInManager();
        });
    });
}

static void enterSandbox()
{
#if PLATFORM(MAC)
    // FIXME: Enter a sandbox here. We should only need read/write access to our database and network access and nothing else.
#endif
}

static void connectionAdded(xpc_connection_t connection)
{
    PCM::DaemonConnectionSet::singleton().add(connection);
}

static void connectionRemoved(xpc_connection_t connection)
{
    PCM::DaemonConnectionSet::singleton().remove(connection);
}

int PCMDaemonMain(int argc, const char** argv)
{
    if (argc < 5 || strcmp(argv[1], "--machServiceName") || strcmp(argv[3], "--storageLocation")) {
        NSLog(@"Usage: %s --machServiceName <name> --storageLocation <location> [--startActivity]", argv[0]);
        return -1;
    }
    const char* machServiceName = argv[2];
    const char* storageLocation = argv[4];
    bool startActivity = argc > 5 && !strcmp(argv[5], "--startActivity");

    @autoreleasepool {
#if ENABLE(CFPREFS_DIRECT_MODE)
        _CFPrefsSetDirectModeEnabled(YES);
#endif
#if HAVE(CF_PREFS_SET_READ_ONLY)
        _CFPrefsSetReadOnly(YES);
#endif
        enterSandbox();
        startListeningForMachServiceConnections(machServiceName, "com.apple.private.webkit.adattributiond"_s, connectionAdded, connectionRemoved, connectionEventHandler);
        if (startActivity)
            registerScheduledActivityHandler();
        WTF::initializeMainThread();
        PCM::initializePCMStorageInDirectory(FileSystem::stringFromFileSystemRepresentation(storageLocation));
    }
    CFRunLoopRun();
    return 0;
}

} // namespace WebKit

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
