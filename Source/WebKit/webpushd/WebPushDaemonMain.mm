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
#import "WebPushDaemonMain.h"

#import "DaemonConnection.h"
#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "WebPushDaemon.h"
#import <Foundation/Foundation.h>
#import <wtf/MainThread.h>
#import <wtf/spi/darwin/XPCSPI.h>

#if USE(APPLE_INTERNAL_SDK)
#import <servers/bootstrap.h>
#else
#import <mach/std_types.h>
extern "C" {
extern kern_return_t bootstrap_check_in(mach_port_t bootstrapPort, const char *serviceName, mach_port_t*);
}
#endif

using WebKit::Daemon::EncodedMessage;
using WebPushD::Daemon;

static const char *incomingPushServiceName = "com.apple.aps.webkit.webpushd.incoming-push";

namespace WebPushD {

static void connectionEventHandler(xpc_object_t request)
{
    Daemon::singleton().connectionEventHandler(request);
}

static void connectionAdded(xpc_connection_t connection)
{
    Daemon::singleton().connectionAdded(connection);
}

static void connectionRemoved(xpc_connection_t connection)
{
    Daemon::singleton().connectionRemoved(connection);
}

} // namespace WebPushD

using WebPushD::connectionEventHandler;
using WebPushD::connectionAdded;
using WebPushD::connectionRemoved;

namespace WebKit {

int WebPushDaemonMain(int argc, const char** argv)
{
    if (argc != 3 || strcmp(argv[1], "--machServiceName")) {
        NSLog(@"usage: webpushd --machServiceName <name>");
        return -1;
    }
    const char* machServiceName = argv[2];

    @autoreleasepool {
        WebKit::startListeningForMachServiceConnections(machServiceName, "com.apple.private.webkit.webpush", connectionAdded, connectionRemoved, connectionEventHandler);

        // TODO: remove this once we actually start using APSConnection.
        mach_port_t incomingMessagePort;
        if (bootstrap_check_in(bootstrap_port, incomingPushServiceName, &incomingMessagePort) != KERN_SUCCESS)
            NSLog(@"Couldn't register for incoming push launch port.");
        
        WTF::initializeMainThread();
    }
    CFRunLoopRun();
    return 0;
}

} // namespace WebKit
