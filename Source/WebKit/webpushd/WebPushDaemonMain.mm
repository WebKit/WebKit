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
#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#import "WebPushDaemonMain.h"

#import "AuxiliaryProcess.h"
#import "DaemonConnection.h"
#import "DaemonDecoder.h"
#import "DaemonEncoder.h"
#import "DaemonUtilities.h"
#import "LogInitialization.h"
#import "Logging.h"
#import "WebPushDaemon.h"
#import <Foundation/Foundation.h>
#import <WebCore/LogInitialization.h>
#import <WebCore/SQLiteFileSystem.h>
#import <WebKit/Logging.h>
#import <getopt.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <wtf/LogInitialization.h>
#import <wtf/MainThread.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/WTFProcess.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/MakeString.h>

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/WebPushDaemonMainAdditions.mm>)
#import <WebKitAdditions/WebPushDaemonMainAdditions.mm>
#endif

#if !defined(WEB_PUSH_DAEMON_MAIN_ADDITIONS)
#define WEB_PUSH_DAEMON_MAIN_ADDITIONS
#endif

using WebKit::Daemon::EncodedMessage;
using WebPushD::WebPushDaemon;

static const ASCIILiteral entitlementName = "com.apple.private.webkit.webpush"_s;

#if ENABLE(RELOCATABLE_WEBPUSHD)
static const ASCIILiteral defaultMachServiceName = "com.apple.webkit.webpushd.relocatable.service"_s;
static const ASCIILiteral defaultIncomingPushServiceName = "com.apple.aps.webkit.webpushd.relocatable.incoming-push"_s;
#else
static const ASCIILiteral defaultMachServiceName = "com.apple.webkit.webpushd.service"_s;
static const ASCIILiteral defaultIncomingPushServiceName = "com.apple.aps.webkit.webpushd.incoming-push"_s;
#endif

namespace WebPushD {

static void connectionEventHandler(xpc_object_t request)
{
    WebPushDaemon::singleton().connectionEventHandler(request);
}

static void connectionAdded(xpc_connection_t connection)
{
    WebPushDaemon::singleton().connectionAdded(connection);
}

static void connectionRemoved(xpc_connection_t connection)
{
    WebPushDaemon::singleton().connectionRemoved(connection);
}

} // namespace WebPushD

using WebPushD::connectionEventHandler;
using WebPushD::connectionAdded;
using WebPushD::connectionRemoved;

namespace WebKit {

static void applySandbox()
{
#if PLATFORM(MAC)
#if ENABLE(RELOCATABLE_WEBPUSHD)
    static ASCIILiteral profileName = "/com.apple.WebKit.webpushd.relocatable.mac.sb"_s;
    static ASCIILiteral userDirectorySuffix = "com.apple.webkit.webpushd.relocatable"_s;
#else
    static ASCIILiteral profileName = "/com.apple.WebKit.webpushd.mac.sb"_s;
    static ASCIILiteral userDirectorySuffix = "com.apple.webkit.webpushd"_s;
#endif
    NSBundle *bundle = [NSBundle bundleForClass:NSClassFromString(@"WKWebView")];
    auto profilePath = makeString(String([bundle resourcePath]), profileName);
    if (FileSystem::fileExists(profilePath)) {
        AuxiliaryProcess::applySandboxProfileForDaemon(profilePath, userDirectorySuffix);
        return;
    }

    auto oldProfilePath = makeString(String([bundle resourcePath]), "/com.apple.WebKit.webpushd.sb"_s);
    AuxiliaryProcess::applySandboxProfileForDaemon(oldProfilePath, "com.apple.webkit.webpushd"_s);
#endif
}

static String getWebPushDirectoryPathWithMigrationIfNecessary()
{
    RetainPtr paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    String libraryPath = paths.get()[0];
#if PLATFORM(MAC) && !ENABLE(RELOCATABLE_WEBPUSHD)
    String oldPath = FileSystem::pathByAppendingComponents(libraryPath, std::initializer_list<StringView>({ "WebKit"_s, "WebPush"_s }));

    RetainPtr containerURL = [[NSFileManager defaultManager] containerURLForSecurityApplicationGroupIdentifier:@"group.com.apple.webkit.webpushd"];

    NSError *error = nil;
    BOOL canReadContainer = !![[NSFileManager defaultManager] contentsOfDirectoryAtURL:containerURL.get() includingPropertiesForKeys:nil options:0 error:&error];
    RELEASE_ASSERT_WITH_MESSAGE(canReadContainer, "Could not access webpushd group container: %s", error.description.UTF8String);

    String containerPath = [containerURL path];
    String newPath = FileSystem::pathByAppendingComponents(containerPath, std::initializer_list<StringView>({ "Library"_s, "WebKit"_s, "WebPush"_s }));

    String oldDatabasePath = FileSystem::pathByAppendingComponent(oldPath, "PushDatabase.db"_s);
    String newDatabasePath = FileSystem::pathByAppendingComponent(newPath, "PushDatabase.db"_s);
    if (FileSystem::fileExists(oldDatabasePath) && !FileSystem::fileExists(newDatabasePath)) {
        FileSystem::makeAllDirectories(newPath);
        bool migrated = WebCore::SQLiteFileSystem::moveDatabaseFile(oldDatabasePath, newDatabasePath);
        RELEASE_LOG(Push, "Moved push database to new container path %" PUBLIC_LOG_STRING " with result: %d", newDatabasePath.utf8().data(), migrated);
    }

    return newPath;
#else
    return FileSystem::pathByAppendingComponents(libraryPath, std::initializer_list<StringView>({ "WebKit"_s, "WebPush"_s }));
#endif
}

int WebPushDaemonMain(int argc, char** argv)
{
    @autoreleasepool {
        WTF::initializeMainThread();

        auto transaction = adoptOSObject(os_transaction_create("com.apple.webkit.webpushd.push-service-main"));
        auto peerEntitlementName = entitlementName;

#if ENABLE(CFPREFS_DIRECT_MODE)
        _CFPrefsSetDirectModeEnabled(YES);
#endif
        applySandbox();

#if PLATFORM(IOS) && !PLATFORM(IOS_SIMULATOR)
        if (!_set_user_dir_suffix("com.apple.webkit.webpushd")) {
            auto error = errno;
            auto errorMessage = strerror(error);
            os_log_error(OS_LOG_DEFAULT, "Failed to set temp dir: %{public}s (%d)", errorMessage, error);
            exit(1);
        }
        (void)NSTemporaryDirectory();
#endif

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
                exitProcess(1);
            }
        }

        WEB_PUSH_DAEMON_MAIN_ADDITIONS;

        WebKit::startListeningForMachServiceConnections(machServiceName, peerEntitlementName, connectionAdded, connectionRemoved, connectionEventHandler);

        if (useMockPushService)
            ::WebPushD::WebPushDaemon::singleton().startMockPushService();
        else {
            String webPushDirectoryPath = getWebPushDirectoryPathWithMigrationIfNecessary();

#if ENABLE(RELOCATABLE_WEBPUSHD)
            String pushDatabasePath = FileSystem::pathByAppendingComponent(webPushDirectoryPath, "PushDatabase.relocatable.db"_s);
#else
            String pushDatabasePath = FileSystem::pathByAppendingComponent(webPushDirectoryPath, "PushDatabase.db"_s);
#endif
            String webClipCachePath = FileSystem::pathByAppendingComponent(webPushDirectoryPath, "WebClipCache.plist"_s);

            ::WebPushD::WebPushDaemon::singleton().startPushService(String::fromLatin1(incomingPushServiceName), pushDatabasePath, webClipCachePath);
        }
    }
    CFRunLoopRun();
    return 0;
}

} // namespace WebKit

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)

