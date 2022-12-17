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

#import "AuxiliaryProcess.h"
#import "DaemonConnection.h"
#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "LogInitialization.h"
#import "WebPushDaemon.h"
#import <Foundation/Foundation.h>
#import <WebCore/LogInitialization.h>
#import <getopt.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <wtf/LogInitialization.h>
#import <wtf/MainThread.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/spi/darwin/XPCSPI.h>

using WebKit::Daemon::EncodedMessage;
using WebPushD::Daemon;

static const ASCIILiteral entitlementName = "com.apple.private.webkit.webpush"_s;
static const ASCIILiteral defaultMachServiceName = "com.apple.webkit.webpushd.service"_s;
static const ASCIILiteral defaultIncomingPushServiceName = "com.apple.aps.webkit.webpushd.incoming-push"_s;

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

static void applySandbox()
{
#if PLATFORM(MAC)
    NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    auto profilePath = makeString(String([bundle resourcePath]), "/com.apple.WebKit.webpushd.mac.sb"_s);
    if (FileSystem::fileExists(profilePath)) {
        AuxiliaryProcess::applySandboxProfileForDaemon(profilePath, "com.apple.webkit.webpushd"_s);
        return;
    }

    auto oldProfilePath = makeString(String([bundle resourcePath]), "/com.apple.WebKit.webpushd.sb"_s);
    AuxiliaryProcess::applySandboxProfileForDaemon(oldProfilePath, "com.apple.webkit.webpushd"_s);
#endif
}

int WebPushDaemonMain(int argc, char** argv)
{
    @autoreleasepool {
        WTF::initializeMainThread();
        
        auto transaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.push-service-main"));

#if ENABLE(CFPREFS_DIRECT_MODE)
        _CFPrefsSetDirectModeEnabled(YES);
#endif
        applySandbox();

#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
        WTF::logChannels().initializeLogChannelsIfNecessary();
        WebCore::logChannels().initializeLogChannelsIfNecessary();
        WebKit::logChannels().initializeLogChannelsIfNecessary();
#endif // !LOG_DISABLED || !RELEASE_LOG_DISABLED

        static struct option options[] = {
            { "machServiceName", required_argument, 0, 'm' },
            { "incomingPushServiceName", required_argument, 0, 'p' },
            { "useMockPushService", no_argument, 0, 'f' }
        };

        const char* machServiceName = defaultMachServiceName;
        const char* incomingPushServiceName = defaultIncomingPushServiceName;
        bool useMockPushService = false;

        int c;
        int optionIndex;
        while ((c = getopt_long(argc, argv, "", options, &optionIndex)) != -1) {
            switch (c) {
            case 'm':
                machServiceName = optarg;
                break;
            case 'p':
                incomingPushServiceName = optarg;
                break;
            case 'f':
                useMockPushService = true;
                break;
            default:
                fprintf(stderr, "Unknown option: %c\n", optopt);
                exit(1);
            }
        }

        WebKit::startListeningForMachServiceConnections(machServiceName, entitlementName, connectionAdded, connectionRemoved, connectionEventHandler);

        if (useMockPushService)
            ::WebPushD::Daemon::singleton().startMockPushService();
        else {
            String libraryPath = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES)[0];
            String pushDatabasePath = FileSystem::pathByAppendingComponents(libraryPath, { "WebKit"_s, "WebPush"_s, "PushDatabase.db"_s });
            ::WebPushD::Daemon::singleton().startPushService(String::fromLatin1(incomingPushServiceName), pushDatabasePath);
        }
    }
    CFRunLoopRun();
    return 0;
}

} // namespace WebKit
