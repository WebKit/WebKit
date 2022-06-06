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
#import "DaemonUtilities.h"

#import <wtf/RetainPtr.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/ASCIILiteral.h>

namespace WebKit {

void startListeningForMachServiceConnections(const char* serviceName, ASCIILiteral entitlement, void(*connectionAdded)(xpc_connection_t), void(*connectionRemoved)(xpc_connection_t), void(*eventHandler)(xpc_object_t))
{
    static NeverDestroyed<RetainPtr<xpc_connection_t>> listener = xpc_connection_create_mach_service(serviceName, dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
    xpc_connection_set_event_handler(listener.get().get(), ^(xpc_object_t peer) {
        if (xpc_get_type(peer) != XPC_TYPE_CONNECTION)
            return;

#if USE(APPLE_INTERNAL_SDK)
        if (!entitlement.isNull() && !WTF::hasEntitlement(peer, entitlement)) {
            NSLog(@"Connection attempted without required entitlement");
            xpc_connection_cancel(peer);
            return;
        }
#endif

        xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
            if (event == XPC_ERROR_CONNECTION_INVALID) {
#if HAVE(XPC_CONNECTION_COPY_INVALIDATION_REASON)
                auto reason = std::unique_ptr<char[]>(xpc_connection_copy_invalidation_reason(peer));
                NSLog(@"Failed to start listening for connections to mach service %s, reason: %s", serviceName, reason.get());
#else
                NSLog(@"Failed to start listening for connections to mach service %s, likely because it is not registered with launchd", serviceName);
#endif
                NSLog(@"Removing peer connection %p", peer);
                connectionRemoved(peer);
                return;
            }
            if (event == XPC_ERROR_CONNECTION_INTERRUPTED) {
                NSLog(@"Removing peer connection %p", peer);
                connectionRemoved(peer);
                return;
            }
            eventHandler(event);
        });
        xpc_connection_set_target_queue(peer, dispatch_get_main_queue());
        xpc_connection_activate(peer);

        NSLog(@"Adding peer connection %p", peer);
        connectionAdded(peer);
    });
    xpc_connection_activate(listener.get().get());
}

RetainPtr<xpc_object_t> vectorToXPCData(Vector<uint8_t>&& vector)
{
    auto bufferSize = vector.size();
    auto rawPointer = vector.releaseBuffer().leakPtr();
    auto dispatchData = adoptNS(dispatch_data_create(rawPointer, bufferSize, dispatch_get_main_queue(), ^{
        fastFree(rawPointer);
    }));
    return adoptNS(xpc_data_create_with_dispatch_data(dispatchData.get()));
}

} // namespace WebKit
