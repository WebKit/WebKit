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
#import "WebPushToolMain.h"

#import "PushMessageForTesting.h"
#import "WebPushToolConnection.h"
#import <optional>
#import <wtf/MainThread.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/WTFProcess.h>
#import <wtf/cocoa/NSStringExtras.h>

#if HAVE(OS_LAUNCHD_JOB) && (PLATFORM(MAC) || PLATFORM(IOS))

#if USE(APPLE_INTERNAL_SDK)
// AppServerSupport cannot be safely imported within modules, so avoid putting
// this SPI declaration in headers.
#import <AppServerSupport/OSLaunchdJob.h>
#else
#import <Foundation/NSError.h>
@interface OSLaunchdJob : NSObject
- (instancetype)initWithPlist:(xpc_object_t)plist;
- (BOOL)submit:(NSError **)errorOut;
@end
#endif

using WebKit::WebPushD::PushMessageForTesting;

__attribute__((__noreturn__))
static void printUsageAndTerminate(NSString *message)
{
    SAFE_FPRINTF(stderr, "%s\n\n", message);

    SAFE_FPRINTF(stderr, "Usage: webpushtool [options] verb [verb_args]\n");
    SAFE_FPRINTF(stderr, "\n");
    SAFE_FPRINTF(stderr, "options is one or more of:\n");
    SAFE_FPRINTF(stderr, "  --development\n");
    SAFE_FPRINTF(stderr, "    Connects to mach service \"org.webkit.webpushtestdaemon.service\" (Default)\n");
    SAFE_FPRINTF(stderr, "  --production\n");
    SAFE_FPRINTF(stderr, "    Connects to mach service \"com.apple.webkit.webpushd.service\"\n");
    SAFE_FPRINTF(stderr, "  --bundleIdentifier <bundleIdentifier>\n");
    SAFE_FPRINTF(stderr, "    Sets connection config to use bundle identifier <bundleIdentifier>.\n");
    SAFE_FPRINTF(stderr, "  --pushPartition <partition>\n");
    SAFE_FPRINTF(stderr, "    Sets connection config to use push partition <partition>.\n");
    SAFE_FPRINTF(stderr, "\n");
    SAFE_FPRINTF(stderr, "verb is one of:\n");
#if HAVE(OS_LAUNCHD_JOB)
    SAFE_FPRINTF(stderr, "  host\n");
    SAFE_FPRINTF(stderr, "    Dynamically registers the service with launchd so it is visible to other applications\n");
    SAFE_FPRINTF(stderr, "    The service name of the registration depends on either the --development or --production option chosen\n");
#endif
    SAFE_FPRINTF(stderr, "  streamDebugMessages\n");
    SAFE_FPRINTF(stderr, "    Stream debug messages from webpushd\n");
    SAFE_FPRINTF(stderr, "  injectPushMessage <scope URL> <message>\n");
    SAFE_FPRINTF(stderr, "    Inject a test push message <message> to the provided --bundleIdentifier and --pushPartition with service worker scope <scope URL>\n");
    SAFE_FPRINTF(stderr, "  getPushPermissionState <scope URL>\n");
    SAFE_FPRINTF(stderr, "    Gets the permission state for the given service worker scope.\n");
    SAFE_FPRINTF(stderr, "  requestPushPermission <scope URL>\n");
    SAFE_FPRINTF(stderr, "    Requests permission state for the given service worker scope.\n");
    SAFE_FPRINTF(stderr, "\n");

    exitProcess(-1);
}

static std::unique_ptr<PushMessageForTesting> pushMessageFromArguments(NSEnumerator<NSString *> *enumerator)
{
    RetainPtr<NSString> registrationString = [enumerator nextObject];
    if (!registrationString)
        return nullptr;

    RetainPtr<NSURL> registrationURL = [NSURL URLWithString:registrationString.get()];
    if (!registrationURL)
        return nullptr;

    RetainPtr<NSString> message = [enumerator nextObject];
    if (!message)
        return nullptr;

#if ENABLE(DECLARATIVE_WEB_PUSH)
    PushMessageForTesting pushMessage = { { }, { }, registrationURL.get(), message.get(), WebKit::WebPushD::PushMessageDisposition::Legacy, std::nullopt };
#else
    PushMessageForTesting pushMessage = { { }, { }, registrationURL.get(), message.get(), WebKit::WebPushD::PushMessageDisposition::Legacy };
#endif

    return makeUniqueWithoutFastMallocCheck<PushMessageForTesting>(WTF::move(pushMessage));
}

static bool registerDaemonWithLaunchD(WebPushTool::PreferTestService preferTestService)
{
    // For now webpushtool only knows how to host webpushd when they're in the same directory
    // e.g. the build directory of a WebKit contributor.
    RetainPtr<NSString> currentExecutablePath = [[NSBundle mainBundle] executablePath];
    RetainPtr<NSURL> currentExecutableDirectoryURL = [[NSURL fileURLWithPath:currentExecutablePath.get() isDirectory:NO] URLByDeletingLastPathComponent];
    RetainPtr<NSURL> daemonExecutablePathURL = [currentExecutableDirectoryURL URLByAppendingPathComponent:@"webpushd"];

    if (![[NSFileManager defaultManager] fileExistsAtPath:retainPtr(daemonExecutablePathURL.get().path).get()]) {
        NSLog(@"Daemon executable does not exist at path %@", retainPtr(daemonExecutablePathURL.get().path).get());
        return false;
    }

    const char* serviceName = (preferTestService == WebPushTool::PreferTestService::Yes) ? "org.webkit.webpushtestdaemon.service" : "com.apple.webkit.webpushd.service";

    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto plist = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(plist.get(), "_ManagedBy", "webpushtool");
    xpc_dictionary_set_string(plist.get(), "Label", "org.webkit.webpushtestdaemon");
    xpc_dictionary_set_bool(plist.get(), "LaunchOnlyOnce", true);
    xpc_dictionary_set_bool(plist.get(), "RootedSimulatorPath", true);

    {
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto environmentVariables = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(environmentVariables.get(), "DYLD_FRAMEWORK_PATH", currentExecutableDirectoryURL.get().fileSystemRepresentation);
        xpc_dictionary_set_string(environmentVariables.get(), "DYLD_LIBRARY_PATH", currentExecutableDirectoryURL.get().fileSystemRepresentation);
        xpc_dictionary_set_value(plist.get(), "EnvironmentVariables", environmentVariables.get());
    }
    {
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto machServices = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_bool(machServices.get(), serviceName, true);
        xpc_dictionary_set_value(plist.get(), "MachServices", machServices.get());
    }
    {
        // FIXME: This is a false positive. <rdar://164843889>
        SUPPRESS_RETAINPTR_CTOR_ADOPT auto programArguments = adoptOSObject(xpc_array_create(nullptr, 0));
#if PLATFORM(MAC)
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, daemonExecutablePathURL.get().fileSystemRepresentation);
#else
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, daemonExecutablePathURL.get().path.fileSystemRepresentation);
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

namespace WebKit {

class WebPushToolVerb {
public:
    virtual ~WebPushToolVerb() = default;
    virtual void run(WebPushTool::Connection&) = 0;
    virtual void done() { CFRunLoopStop(retainPtr(CFRunLoopGetMain()).get()); }
};

class InjectPushMessageVerb : public WebPushToolVerb {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(InjectPushMessageVerb);
public:
    InjectPushMessageVerb(PushMessageForTesting&& message)
        : m_pushMessage(WTF::move(message)) { }

    void run(WebPushTool::Connection& connection) override
    {
        auto pushMessage = m_pushMessage;
        pushMessage.targetAppCodeSigningIdentifier = connection.bundleIdentifier();
        pushMessage.pushPartitionString = connection.pushPartition();

        connection.sendPushMessage(WTF::move(pushMessage), [this, bundleIdentifier = connection.bundleIdentifier(), webClipIdentifier = connection.pushPartition()](String error) mutable {
            if (error.isEmpty())
                SAFE_PRINTF("Successfully injected push message %s for [bundleID = %s, webClipIdentifier = %s, scope = %s]\n", m_pushMessage.payload.utf8(), bundleIdentifier.utf8(), webClipIdentifier.utf8(), m_pushMessage.registrationURL.string().utf8());
            else
                SAFE_PRINTF("Injected push message with error: %s\n", error.utf8());
            done();
        });
    }

private:
    PushMessageForTesting m_pushMessage;
};

class GetPushPermissionStateVerb : public WebPushToolVerb {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(GetPushPermissionStateVerb);
public:
    GetPushPermissionStateVerb(const String& scope)
        : m_scope(scope) { }

    void run(WebPushTool::Connection& connection) override
    {
        connection.getPushPermissionState(m_scope, [this](WebCore::PushPermissionState state) mutable {
            printf("Got push permission status: %u\n", static_cast<unsigned>(state));
            done();
        });
    }

private:
    String m_scope;
};

class RequestPushPermissionVerb : public WebPushToolVerb {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(RequestPushPermissionVerb);
public:
    RequestPushPermissionVerb(const String& scope)
        : m_scope(scope) { }

    void run(WebPushTool::Connection& connection) override
    {
        connection.requestPushPermission(m_scope, [this](bool granted) mutable {
            printf("Requested push permission with result: %d\n", granted);
            done();
        });
    }

private:
    String m_scope;
};

int WebPushToolMain(int, char **)
{
    WTF::initializeMainThread();

    auto preferTestService = WebPushTool::PreferTestService::Yes;
    bool host = false;
    std::unique_ptr<WebPushToolVerb> verb;
    RetainPtr<NSString> bundleIdentifier = @"com.apple.WebKit.TestWebKitAPI";
    RetainPtr<NSString> pushPartition;

    @autoreleasepool {
        RetainPtr<NSArray> arguments = [[NSProcessInfo processInfo] arguments];
        if (arguments.get().count == 1)
            printUsageAndTerminate(@"No arguments provided");

        RetainPtr<NSEnumerator<NSString *>> enumerator = [retainPtr([arguments subarrayWithRange:NSMakeRange(1, arguments.get().count - 1)]) objectEnumerator];
        RetainPtr<NSString> argument = [enumerator nextObject];
        while (argument) {
            if ([argument isEqualToString:@"--production"])
                preferTestService = WebPushTool::PreferTestService::No;
            else if ([argument isEqualToString:@"--development"])
                preferTestService = WebPushTool::PreferTestService::Yes;
            else if ([argument isEqualToString:@"--bundleIdentifier"])
                bundleIdentifier = [enumerator nextObject];
            else if ([argument isEqualToString:@"--pushPartition"])
                pushPartition = [enumerator nextObject];
            else if ([argument isEqualToString:@"host"])
                host = true;
            else if ([argument isEqualToString:@"streamDebugMessages"])
                execl("/usr/bin/log", "log", "stream", "--debug", "--info", "--process", "webpushd");
            else if ([argument isEqualToString:@"injectPushMessage"]) {
                auto pushMessage = pushMessageFromArguments(enumerator.get());
                if (!pushMessage)
                    printUsageAndTerminate(adoptNS([[NSString alloc] initWithFormat:@"Invalid push arguments specified"]).get());
                verb = makeUnique<InjectPushMessageVerb>(WTF::move(*pushMessage));
            } else if ([argument isEqualToString:@"getPushPermissionState"]) {
                String scope { [enumerator nextObject] };
                verb = makeUnique<GetPushPermissionStateVerb>(scope);
            } else if ([argument isEqualToString:@"requestPushPermission"]) {
                String scope { [enumerator nextObject] };
                verb = makeUnique<RequestPushPermissionVerb>(scope);
            } else
                printUsageAndTerminate(adoptNS([[NSString alloc] initWithFormat:@"Invalid option provided: %@", argument.get()]).get());

            argument = [enumerator nextObject];
        }
    }

    if (host) {
        if (!registerDaemonWithLaunchD(preferTestService)) {
            NSLog(@"Failed to register webpushd with launchd");
            exit(1);
        }

        NSLog(@"Registered webpushd with launchd");
        exit(0);
    }

    if (!verb)
        printUsageAndTerminate(@"Nothing to do");

    auto connection = WebPushTool::Connection::create(preferTestService, bundleIdentifier.get(), pushPartition.get());
    connection->connectToService(host ? WebPushTool::WaitForServiceToExist::No : WebPushTool::WaitForServiceToExist::Yes);
    verb->run(connection.get());

    CFRunLoopRun();
    return 0;
}

} // namespace WebKit

#else

namespace WebKit {
int WebPushToolMain(int, char **)
{
    return -1;
}
}

#endif // #if HAVE(OS_LAUNCHD_JOB) && (PLATFORM(MAC) || PLATFORM(IOS))
