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
#import "PushMessageForTesting.h"
#import "WebPushToolConnection.h"
#import <Foundation/Foundation.h>
#import <optional>
#import <wtf/MainThread.h>

using WebKit::WebPushD::PushMessageForTesting;

__attribute__((__noreturn__))
static void printUsageAndTerminate(NSString *message)
{
    fprintf(stderr, "%s\n\n", message.UTF8String);

    fprintf(stderr, "Usage: webpushtool [options]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  --development\n");
    fprintf(stderr, "    Connects to mach service \"org.webkit.webpushtestdaemon.service\" (Default)\n");
    fprintf(stderr, "  --production\n");
    fprintf(stderr, "    Connects to mach service \"com.apple.webkit.webpushd.service\"\n");
#if HAVE(OS_LAUNCHD_JOB)
    fprintf(stderr, "  --host\n");
    fprintf(stderr, "    Dynamically registers the service with launchd so it is visible to other applications\n");
    fprintf(stderr, "    The service name of the registration depends on either the --development or --production option chosen\n");
#endif
    fprintf(stderr, "  --streamDebugMessages\n");
    fprintf(stderr, "    Stream debug messages from webpushd\n");
    fprintf(stderr, "  --reconnect\n");
    fprintf(stderr, "    Reconnect after connection is lost\n");
    fprintf(stderr, "  --push <target app identifier> <registration URL> <message>\n");
    fprintf(stderr, "    Inject a test push messasge to the target app and registration URL\n");
    fprintf(stderr, "\n");

    exit(-1);
}

static std::unique_ptr<PushMessageForTesting> pushMessageFromArguments(NSEnumerator<NSString *> *enumerator)
{
    NSString *appIdentifier = [enumerator nextObject];
    if (!appIdentifier)
        return nullptr;

    NSString *registrationString = [enumerator nextObject];
    if (!registrationString)
        return nullptr;

    NSURL *registrationURL = [NSURL URLWithString:registrationString];
    if (!registrationURL)
        return nullptr;

    NSString *message = [enumerator nextObject];
    if (!message)
        return nullptr;

    PushMessageForTesting pushMessage = { appIdentifier, registrationString, registrationURL, message };
    return makeUniqueWithoutFastMallocCheck<PushMessageForTesting>(WTFMove(pushMessage));
}

#if HAVE(OS_LAUNCHD_JOB)
static bool registerDaemonWithLaunchD(WebPushTool::PreferTestService preferTestService)
{
    // For now webpushtool only knows how to host webpushd when they're in the same directory
    // e.g. the build directory of a WebKit contributor.
    NSString *currentExecutablePath = [[NSBundle mainBundle] executablePath];
    NSURL *currentExecutableDirectoryURL = [[NSURL fileURLWithPath:currentExecutablePath isDirectory:NO] URLByDeletingLastPathComponent];
    NSURL *daemonExecutablePathURL = [currentExecutableDirectoryURL URLByAppendingPathComponent:@"webpushd"];

    if (![[NSFileManager defaultManager] fileExistsAtPath:daemonExecutablePathURL.path]) {
        NSLog(@"Daemon executable does not exist at path %@", daemonExecutablePathURL.path);
        return false;
    }

    const char* serviceName = (preferTestService == WebPushTool::PreferTestService::Yes) ? "org.webkit.webpushtestdaemon.service" : "com.apple.webkit.webpushd.service";

    auto plist = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(plist.get(), "_ManagedBy", "webpushtool");
    xpc_dictionary_set_string(plist.get(), "Label", "org.webkit.webpushtestdaemon");
    xpc_dictionary_set_bool(plist.get(), "LaunchOnlyOnce", true);
    xpc_dictionary_set_bool(plist.get(), "RootedSimulatorPath", true);

    {
        auto environmentVariables = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(environmentVariables.get(), "DYLD_FRAMEWORK_PATH", currentExecutableDirectoryURL.fileSystemRepresentation);
        xpc_dictionary_set_string(environmentVariables.get(), "DYLD_LIBRARY_PATH", currentExecutableDirectoryURL.fileSystemRepresentation);
        xpc_dictionary_set_value(plist.get(), "EnvironmentVariables", environmentVariables.get());
    }
    {
        auto machServices = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_bool(machServices.get(), serviceName, true);
        xpc_dictionary_set_value(plist.get(), "MachServices", machServices.get());
    }
    {
        auto programArguments = adoptNS(xpc_array_create(nullptr, 0));
#if PLATFORM(MAC)
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, daemonExecutablePathURL.fileSystemRepresentation);
#else
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, daemonExecutablePathURL.path.fileSystemRepresentation);
#endif
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, "--machServiceName");
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, serviceName);
        xpc_dictionary_set_value(plist.get(), "ProgramArguments", programArguments.get());
    }

    auto job = adoptNS([[OSLaunchdJob alloc] initWithPlist:plist.get()]);
    NSError *error = nil;
    [job submit:&error];

    if (error) {
        NSLog(@"Error setting up service: %@", error);
        return false;
    }

    return true;
}
#endif // #if HAVE(OS_LAUNCHD_JOB)

int main(int, const char **)
{
    WTF::initializeMainThread();

    auto preferTestService = WebPushTool::PreferTestService::Yes;
    auto reconnect = WebPushTool::Reconnect::No;
    bool host = false;
    std::optional<WebPushTool::Action> action;
    std::unique_ptr<PushMessageForTesting> pushMessage;

    @autoreleasepool {
        NSArray *arguments = [[NSProcessInfo processInfo] arguments];
        if (arguments.count == 1)
            printUsageAndTerminate(@"No arguments provided");

        NSEnumerator<NSString *> *enumerator = [[arguments subarrayWithRange:NSMakeRange(1, arguments.count - 1)] objectEnumerator];
        NSString *argument = [enumerator nextObject];
        while (argument) {
            if ([argument isEqualToString:@"--production"])
                preferTestService = WebPushTool::PreferTestService::No;
            else if ([argument isEqualToString:@"--development"])
                preferTestService = WebPushTool::PreferTestService::Yes;
            else if ([argument isEqualToString:@"--streamDebugMessages"])
                action = WebPushTool::Action::StreamDebugMessages;
            else if ([argument isEqualToString:@"--reconnect"])
                reconnect = WebPushTool::Reconnect::Yes;
#if HAVE(OS_LAUNCHD_JOB)
            else if ([argument isEqualToString:@"--host"])
                host = true;
#endif
            else if ([argument isEqualToString:@"--push"]) {
                pushMessage = pushMessageFromArguments(enumerator);
                if (!pushMessage)
                    printUsageAndTerminate([NSString stringWithFormat:@"Invalid push arguments specified"]);
            } else
                printUsageAndTerminate([NSString stringWithFormat:@"Invalid option provided: %@", argument]);

            argument = [enumerator nextObject];
        }
    }

    if (!action && !pushMessage)
        printUsageAndTerminate(@"No action provided");

#if HAVE(OS_LAUNCHD_JOB)
    if (host && !registerDaemonWithLaunchD(preferTestService))
        printUsageAndTerminate(@"Unable to install plist to host the service");
#endif

    auto connection = WebPushTool::Connection::create(action, preferTestService, reconnect);
    if (pushMessage)
        connection->setPushMessage(WTFMove(pushMessage));

    connection->connectToService(host ? WebPushTool::WaitForServiceToExist::No : WebPushTool::WaitForServiceToExist::Yes);

    CFRunLoopRun();
    return 0;
}
