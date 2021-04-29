/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#import "HandleXPCEndpointMessages.h"
#import "Logging.h"
#import "WKCrashReporter.h"
#import "XPCServiceEntryPoint.h"
#import <CoreFoundation/CoreFoundation.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <sys/sysctl.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/spi/darwin/XPCSPI.h>

namespace WebKit {

static void setAppleLanguagesPreference()
{
    auto bootstrap = adoptOSObject(xpc_copy_bootstrap());
    if (!bootstrap)
        return;

    if (xpc_object_t languages = xpc_dictionary_get_value(bootstrap.get(), "OverrideLanguages")) {
        @autoreleasepool {
            NSDictionary *existingArguments = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
            auto newArguments = adoptNS([existingArguments mutableCopy]);
            RetainPtr<NSMutableArray> newLanguages = adoptNS([[NSMutableArray alloc] init]);
            xpc_array_apply(languages, ^(size_t index, xpc_object_t value) {
                [newLanguages addObject:[NSString stringWithCString:xpc_string_get_string_ptr(value) encoding:NSUTF8StringEncoding]];
                return true;
            });
            [newArguments setValue:newLanguages.get() forKey:@"AppleLanguages"];
            [[NSUserDefaults standardUserDefaults] setVolatileDomain:newArguments.get() forName:NSArgumentDomain];
        }
    }
}

static void XPCServiceEventHandler(xpc_connection_t peer)
{
    static NeverDestroyed<OSObjectPtr<xpc_object_t>> priorityBoostMessage;

    OSObjectPtr<xpc_connection_t> retainedPeerConnection(peer);

    xpc_connection_set_target_queue(peer, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        xpc_type_t type = xpc_get_type(event);
        if (type == XPC_TYPE_ERROR) {
            if (event == XPC_ERROR_CONNECTION_INVALID || event == XPC_ERROR_TERMINATION_IMMINENT) {
                RELEASE_LOG_FAULT(IPC, "Exiting: Received XPC event type: %s", event == XPC_ERROR_CONNECTION_INVALID ? "XPC_ERROR_CONNECTION_INVALID" : "XPC_ERROR_TERMINATION_IMMINENT");
                // FIXME: Handle this case more gracefully.
                [[NSRunLoop mainRunLoop] performBlock:^{
                    exit(EXIT_FAILURE);
                }];
            }
        } else {
            assert(type == XPC_TYPE_DICTIONARY);

            if (!strcmp(xpc_dictionary_get_string(event, "message-name"), "bootstrap")) {
                CFBundleRef webKitBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit"));

                const char* serviceName = xpc_dictionary_get_string(event, "service-name");
                CFStringRef entryPointFunctionName = nullptr;
                if (strstr(serviceName, "com.apple.WebKit.WebContent") == serviceName)
                    entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(WEBCONTENT_SERVICE_INITIALIZER));
                else if (!strcmp(serviceName, "com.apple.WebKit.Networking"))
                    entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(NETWORK_SERVICE_INITIALIZER));
                else if (!strcmp(serviceName, "com.apple.WebKit.Plugin.64"))
                    entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(PLUGIN_SERVICE_INITIALIZER));
                else if (!strcmp(serviceName, "com.apple.WebKit.GPU"))
                    entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(GPU_SERVICE_INITIALIZER));
                else if (!strcmp(serviceName, "com.apple.WebKit.WebAuthn"))
                    entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(WEBAUTHN_SERVICE_INITIALIZER));
                else
                    RELEASE_ASSERT_NOT_REACHED();

                typedef void (*InitializerFunction)(xpc_connection_t, xpc_object_t, xpc_object_t);
                InitializerFunction initializerFunctionPtr = reinterpret_cast<InitializerFunction>(CFBundleGetFunctionPointerForName(webKitBundle, entryPointFunctionName));
                if (!initializerFunctionPtr) {
                    RELEASE_LOG_FAULT(IPC, "Exiting: Unable to find entry point in WebKit.framework with name: %s", [(__bridge NSString *)entryPointFunctionName UTF8String]);
                    [[NSRunLoop mainRunLoop] performBlock:^{
                        exit(EXIT_FAILURE);
                    }];
                }

                auto reply = adoptOSObject(xpc_dictionary_create_reply(event));
                xpc_dictionary_set_string(reply.get(), "message-name", "process-finished-launching");
                xpc_connection_send_message(xpc_dictionary_get_remote_connection(event), reply.get());

                int fd = xpc_dictionary_dup_fd(event, "stdout");
                if (fd != -1)
                    dup2(fd, STDOUT_FILENO);

                fd = xpc_dictionary_dup_fd(event, "stderr");
                if (fd != -1)
                    dup2(fd, STDERR_FILENO);

                WorkQueue::main().dispatchSync([initializerFunctionPtr, event = OSObjectPtr<xpc_object_t>(event), retainedPeerConnection] {
                    initializerFunctionPtr(retainedPeerConnection.get(), event.get(), priorityBoostMessage.get().get());

                    setAppleLanguagesPreference();
                });

                priorityBoostMessage.get() = nullptr;
            }

            // Leak a boost onto the NetworkProcess.
            if (!strcmp(xpc_dictionary_get_string(event, "message-name"), "pre-bootstrap")) {
                assert(!priorityBoostMessage.get());
                priorityBoostMessage.get() = event;
            }

            handleXPCEndpointMessages(event);
        }
    });

    xpc_connection_resume(peer);
}

#if PLATFORM(MAC)

NEVER_INLINE NO_RETURN_DUE_TO_CRASH static void crashDueWebKitFrameworkVersionMismatch()
{
    CRASH();
}

#endif // PLATFORM(MAC)

#if ENABLE(CFPREFS_DIRECT_MODE)
static bool shouldEnableCFPrefsDirectMode(int argc, const char** argv)
{
    if (argc <= 0 || !argv[0])
        return false;
    if (strstr(argv[0], "com.apple.WebKit.WebContent"))
        return true;
    if (strstr(argv[0], "com.apple.WebKit.GPU"))
        return true;
    return false;
}
#endif

int XPCServiceMain(int argc, const char** argv)
{
    ASSERT(argc >= 1);
    ASSERT(argv[0]);
#if ENABLE(CFPREFS_DIRECT_MODE)
    if (shouldEnableCFPrefsDirectMode(argc, argv)) {
        // Enable CFPrefs direct mode to avoid unsuccessfully attempting to connect to the daemon and getting blocked by the sandbox.
        _CFPrefsSetDirectModeEnabled(YES);
#if HAVE(CF_PREFS_SET_READ_ONLY)
        _CFPrefsSetReadOnly(YES);
#endif
    }
#else
    UNUSED_PARAM(argc);
    UNUSED_PARAM(argv);
#endif

    WTF::initializeMainThread();

    auto bootstrap = adoptOSObject(xpc_copy_bootstrap());

    if (bootstrap) {
#if PLATFORM(IOS_FAMILY)
        auto containerEnvironmentVariables = xpc_dictionary_get_value(bootstrap.get(), "ContainerEnvironmentVariables");
        xpc_dictionary_apply(containerEnvironmentVariables, ^(const char *key, xpc_object_t value) {
            setenv(key, xpc_string_get_string_ptr(value), 1);
            return true;
        });
#endif
#if PLATFORM(MAC)
#if ASAN_ENABLED
        // EXC_RESOURCE on ASAN builds freezes the process for several minutes: rdar://65027596
        if (char *disableFreezingOnExcResource = getenv("DISABLE_FREEZING_ON_EXC_RESOURCE")) {
            if (!strcasecmp(disableFreezingOnExcResource, "yes") || !strcasecmp(disableFreezingOnExcResource, "true") || !strcasecmp(disableFreezingOnExcResource, "1")) {
                int val = 1;
                int rc = sysctlbyname("debug.toggle_address_reuse", nullptr, 0, &val, sizeof(val));
                if (rc < 0)
                    WTFLogAlways("failed to set debug.toggle_address_reuse: %d\n", rc);
                else
                    WTFLogAlways("debug.toggle_address_reuse is now 1.\n");
            }
        }
#endif
        String webKitBundleVersion = xpc_dictionary_get_string(bootstrap.get(), "WebKitBundleVersion");
        String expectedBundleVersion = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"].infoDictionary[(__bridge NSString *)kCFBundleVersionKey];
        if (!webKitBundleVersion.isNull() && !expectedBundleVersion.isNull() && webKitBundleVersion != expectedBundleVersion) {
            auto errorMessage = makeString("WebKit framework version mismatch: ", webKitBundleVersion, " != ", expectedBundleVersion);
            logAndSetCrashLogMessage(errorMessage.utf8().data());
            crashDueWebKitFrameworkVersionMismatch();
        }
#endif
    }

#if PLATFORM(MAC)
    // Don't allow Apple Events in WebKit processes. This can be removed when <rdar://problem/14012823> is fixed.
    setenv("__APPLEEVENTSSERVICENAME", "", 1);
#endif

    xpc_main(XPCServiceEventHandler);
    return 0;
}

} // namespace WebKit
