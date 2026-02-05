/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
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

#import "LaunchLogHook.h"
#import "Logging.h"
#import "WKCrashReporter.h"
#import "WebKitServiceNames.h"
#import "XPCEndpointMessages.h"
#import "XPCServiceEntryPoint.h"
#import "XPCUtilities.h"
#import <CoreFoundation/CoreFoundation.h>
#import <mach/mach.h>
#import <pal/spi/cf/CFUtilitiesSPI.h>
#import <pal/spi/cocoa/CoreServicesSPI.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <sys/sysctl.h>
#import <wtf/BlockPtr.h>
#import <wtf/Language.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/WTFProcess.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/darwin/XPCExtras.h>
#import <wtf/spi/cocoa/OSLogSPI.h>
#import <wtf/spi/darwin/SandboxSPI.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/TextStream.h>

#if __has_include(<WebKitAdditions/DyldCallbackAdditions.h>)
#import <WebKitAdditions/DyldCallbackAdditions.h>
#endif

namespace WebKit {

static Vector<String>& overrideLanguagesFromBootstrap()
{
    static NeverDestroyed<Vector<String>> languages;
    return languages;
}

static void stageOverrideLanguagesForMainThread(Vector<String>&& languages)
{
    RELEASE_ASSERT(overrideLanguagesFromBootstrap().isEmpty());
    overrideLanguagesFromBootstrap().swap(languages);
}

static void setAppleLanguagesPreference()
{
    if (overrideLanguagesFromBootstrap().isEmpty())
        return;
    LOG_WITH_STREAM(Language, stream << "Overriding user prefered language: " << overrideLanguagesFromBootstrap());
    overrideUserPreferredLanguages(overrideLanguagesFromBootstrap());
}

static void initializeCFPrefs()
{
#if ENABLE(CFPREFS_DIRECT_MODE)
    // Enable CFPrefs direct mode to avoid unsuccessfully attempting to connect to the daemon and getting blocked by the sandbox.
    _CFPrefsSetDirectModeEnabled(YES);
    _CFPrefsSetReadOnly(YES);
#endif // ENABLE(CFPREFS_DIRECT_MODE)
}

static void initializeLogd(bool disableLogging, xpc_connection_t connection)
{
#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
    if (disableLogging) {
        os_trace_set_mode(OS_TRACE_MODE_OFF);
        LaunchLogHook::singleton().initialize(connection);
        return;
    }
#else
    UNUSED_PARAM(disableLogging);

    // Log a long message to make sure the XPC connection to the log daemon for oversized messages is opened.
    // This is needed to block launchd after the WebContent process has launched, since access to launchd is
    // required when opening new XPC connections.
    std::array<char, 1024> stringWithSpaces;
    memsetSpan(std::span { stringWithSpaces }, ' ');
    stringWithSpaces.back() = '\0';
    RELEASE_LOG(Process, "Initialized logd %s", stringWithSpaces.data());
#endif
}

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)

NEVER_INLINE NO_RETURN_DUE_TO_CRASH static void crashDueWebKitFrameworkVersionMismatch()
{
    CRASH();
}
static void checkFrameworkVersion(xpc_object_t message)
{
    auto uiProcessWebKitBundleVersion = xpcDictionaryGetString(message, "WebKitBundleVersion"_s);
    auto webkitBundleVersion = ASCIILiteral::fromLiteralUnsafe(WEBKIT_BUNDLE_VERSION);
    if (!uiProcessWebKitBundleVersion.isNull() && uiProcessWebKitBundleVersion != webkitBundleVersion) {
        auto errorMessage = makeString("WebKit framework version mismatch: "_s, uiProcessWebKitBundleVersion, " != "_s, webkitBundleVersion);
        logAndSetCrashLogMessage(errorMessage.utf8().data());
        crashDueWebKitFrameworkVersionMismatch();
    }
}
#endif // PLATFORM(MAC)

static bool s_isWebProcess = false;

static void setUserDirSuffix(ASCIILiteral suffix)
{
#if PLATFORM(IOS_FAMILY)
    if (_set_user_dir_suffix(suffix)) {
        RELEASE_LOG(IPC, "Successfully set temp dir");
        confstr(_CS_DARWIN_USER_TEMP_DIR, nullptr, 0);
        return;
    }
    RELEASE_LOG_ERROR(IPC, "Failed to set temp dir: errno = %d", errno);
#else
    UNUSED_PARAM(suffix);
#endif
}

void XPCServiceEventHandler(xpc_connection_t peer)
{
    OSObjectPtr<xpc_connection_t> retainedPeerConnection(peer);

    xpc_connection_set_target_queue(peer, globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        xpc_type_t type = xpc_get_type(event);
        if (type != XPC_TYPE_DICTIONARY) {
            RELEASE_LOG_ERROR(IPC, "XPCServiceEventHandler: Received unexpected XPC event type: %{public}s", xpc_type_get_name(type));
            if (type == XPC_TYPE_ERROR) {
                if (event == XPC_ERROR_CONNECTION_INVALID || event == XPC_ERROR_TERMINATION_IMMINENT) {
                    RELEASE_LOG_FAULT(IPC, "Exiting: Received XPC event type: %{public}s", event == XPC_ERROR_CONNECTION_INVALID ? "XPC_ERROR_CONNECTION_INVALID" : "XPC_ERROR_TERMINATION_IMMINENT");
#if ENABLE(CLOSE_WEBCONTENT_XPC_CONNECTION_POST_LAUNCH)
                    if (s_isWebProcess)
                        return;
#endif
                    // FIXME: Handle this case more gracefully.
                    [[NSRunLoop mainRunLoop] performBlock:^{
                        exitProcess(EXIT_FAILURE);
                    }];
                }
            }
            return;
        }

#if USE(EXIT_XPC_MESSAGE_WORKAROUND)
        handleXPCExitMessage(event);
#endif

        String messageName = xpcDictionaryGetString(event, "message-name"_s);
        if (!messageName) {
            RELEASE_LOG_ERROR(IPC, "XPCServiceEventHandler: 'message-name' is not present in the XPC dictionary");
            return;
        }
        if (messageName == "bootstrap"_s) {
            WTF::initialize();

            bool disableLogging = xpc_dictionary_get_bool(event, "disable-logging");
            initializeLogd(disableLogging, retainedPeerConnection.get());

            if (OSObjectPtr<xpc_object_t> languages = xpc_dictionary_get_value(event, "OverrideLanguages")) {
                Vector<String> newLanguages;
                @autoreleasepool {
                    xpc_array_apply(languages.get(), makeBlockPtr([&newLanguages](size_t index, xpc_object_t value) {
                        newLanguages.append(xpcStringGetString(value));
                        return true;
                    }).get());
                }
                LOG_WITH_STREAM(Language, stream << "Bootstrap message contains OverrideLanguages: " << newLanguages);
                stageOverrideLanguagesForMainThread(WTF::move(newLanguages));
            } else
                LOG(Language, "Bootstrap message does not contain OverrideLanguages");

#if __has_include(<WebKitAdditions/DyldCallbackAdditions.h>) && PLATFORM(IOS)
            register_for_dlsym_callbacks();
#endif

#if PLATFORM(IOS_FAMILY)
            if (RetainPtr containerEnvironmentVariables = xpc_dictionary_get_value(event, "ContainerEnvironmentVariables")) {
                xpc_dictionary_apply(containerEnvironmentVariables.get(), ^(const char *key, xpc_object_t value) {
                    setenv(key, xpc_string_get_string_ptr(value), 1);  // NOLINT
                    return true;
                });
            }
#endif
            String serviceName = xpcDictionaryGetString(event, "service-name"_s);
            if (!serviceName) {
                RELEASE_LOG_ERROR(IPC, "XPCServiceEventHandler: 'service-name' is not present in the XPC dictionary");
                return;
            }

            CFStringRef entryPointFunctionName = nullptr;
            if (serviceName.startsWith(webContentServiceName)) {
                s_isWebProcess = true;
#if !USE(EXTENSIONKIT)
                setUserDirSuffix(webContentServiceName);
#endif
                entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(WEBCONTENT_SERVICE_INITIALIZER));
            } else if (serviceName == networkingServiceName) {
                setUserDirSuffix(networkingServiceName);
                entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(NETWORK_SERVICE_INITIALIZER));
            } else if (serviceName == gpuServiceName) {
                setUserDirSuffix(gpuServiceName);
                entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(GPU_SERVICE_INITIALIZER));
            } else if (serviceName == modelServiceName)
                entryPointFunctionName = CFSTR(STRINGIZE_VALUE_OF(MODEL_SERVICE_INITIALIZER));
            else {
                RELEASE_LOG_ERROR(IPC, "XPCServiceEventHandler: Unexpected 'service-name': %{public}s", serviceName.utf8().data());
                return;
            }

            RetainPtr webKitBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit"));
            typedef void (*InitializerFunction)(xpc_connection_t, xpc_object_t);
            InitializerFunction initializerFunctionPtr = reinterpret_cast<InitializerFunction>(CFBundleGetFunctionPointerForName(webKitBundle.get(), entryPointFunctionName));
            if (!initializerFunctionPtr) {
                RELEASE_LOG_FAULT(IPC, "Exiting: Unable to find entry point in WebKit.framework with name: %s", [bridge_cast(entryPointFunctionName) UTF8String]);
                [[NSRunLoop mainRunLoop] performBlock:^{
                    exitProcess(EXIT_FAILURE);
                }];
                return;
            }

            // FIXME: This is a false positive. <rdar://164843889>
            SUPPRESS_RETAINPTR_CTOR_ADOPT auto reply = adoptOSObject(xpc_dictionary_create_reply(event));
            xpc_dictionary_set_string(reply.get(), "message-name", "process-finished-launching");
            xpc_connection_send_message(OSObjectPtr<xpc_connection_t> { xpc_dictionary_get_remote_connection(event) }.get(), reply.get());

            int fd = xpc_dictionary_dup_fd(event, "stdout");
            if (fd != -1)
                dup2(fd, STDOUT_FILENO);

            fd = xpc_dictionary_dup_fd(event, "stderr");
            if (fd != -1)
                dup2(fd, STDERR_FILENO);

            WorkQueue::mainSingleton().dispatchSync([initializerFunctionPtr, event = OSObjectPtr<xpc_object_t>(event), retainedPeerConnection] {
                WTF::initializeMainThread();

                initializeCFPrefs();
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
                checkFrameworkVersion(event.get());
#endif
                initializerFunctionPtr(retainedPeerConnection.get(), event.get());

                setAppleLanguagesPreference();
            });

            return;
        }

        handleXPCEndpointMessage(event, messageName);
    });

    xpc_connection_resume(peer);
}

int XPCServiceMain(int, const char**)
{
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto bootstrap = adoptOSObject(xpc_copy_bootstrap());

    if (bootstrap) {
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#if ASAN_ENABLED
        // EXC_RESOURCE on ASAN builds freezes the process for several minutes: rdar://65027596
        if (auto* disableFreezingOnExcResource = getenv("DISABLE_FREEZING_ON_EXC_RESOURCE")) {
            if (equalIgnoringASCIICase(disableFreezingOnExcResource, "yes"_s) || equalIgnoringASCIICase(disableFreezingOnExcResource, "true"_s) || equalIgnoringASCIICase(disableFreezingOnExcResource, "1"_s)) {
                int val = 1;
                int rc = sysctlbyname("debug.toggle_address_reuse", nullptr, 0, &val, sizeof(val));
                if (rc < 0)
                    WTFLogAlways("failed to set debug.toggle_address_reuse: %d\n", rc);
                else
                    WTFLogAlways("debug.toggle_address_reuse is now 1.\n");
            }
        }
#endif
#endif
    }

    xpc_main(XPCServiceEventHandler);
    return 0;
}

} // namespace WebKit
