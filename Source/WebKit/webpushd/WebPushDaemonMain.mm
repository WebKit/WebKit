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
#import "DaemonConnection.h"
#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "WebPushDaemon.h"
#import <Foundation/Foundation.h>
#import <wtf/MainThread.h>
#import <wtf/spi/darwin/XPCSPI.h>

using WebKit::Daemon::EncodedMessage;
using WebPushD::Daemon;

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

int main(int argc, const char** argv)
{
    if (argc != 3 || strcmp(argv[1], "--machServiceName")) {
        NSLog(@"usage: webpushd --machServiceName <name>");
        return -1;
    }
    const char* machServiceName = argv[2];

    @autoreleasepool {
        // FIXME: Add a sandbox.
        // FIXME: Add an entitlement check.
        WebKit::startListeningForMachServiceConnections(machServiceName, nullptr, WebPushD::connectionAdded, WebPushD::connectionRemoved, WebPushD::connectionEventHandler);
        WTF::initializeMainThread();
    }
    CFRunLoopRun();
    return 0;
}
