/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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
#import "ProcessLauncher.h"

#import "Logging.h"
#import "ReasonSPI.h"
#import "WebPreferencesDefaultValues.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <mach/mach_error.h>
#import <mach/machine.h>
#import <pal/spi/cocoa/ServersSPI.h>
#import <spawn.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <wtf/FileSystem.h>
#import <wtf/MachSendRight.h>
#import <wtf/RunLoop.h>
#import <wtf/SoftLinking.h>
#import <wtf/Threading.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/spi/cf/CFBundleSPI.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(MAC)
#import "CodeSigning.h"
#endif

namespace WebKit {

static const char* webContentServiceName(bool nonValidInjectedCodeAllowed, ProcessLauncher::Client* client)
{
    if (client && client->shouldEnableCaptivePortalMode())
        return "com.apple.WebKit.WebContent.CaptivePortal";

    return nonValidInjectedCodeAllowed ? "com.apple.WebKit.WebContent.Development" : "com.apple.WebKit.WebContent";
}

static const char* serviceName(const ProcessLauncher::LaunchOptions& launchOptions, ProcessLauncher::Client* client)
{
    switch (launchOptions.processType) {
    case ProcessLauncher::ProcessType::Web:
        return webContentServiceName(launchOptions.nonValidInjectedCodeAllowed, client);
    case ProcessLauncher::ProcessType::Network:
        return "com.apple.WebKit.Networking";
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        return "com.apple.WebKit.GPU";
#endif
    }
}

static bool shouldLeakBoost(const ProcessLauncher::LaunchOptions& launchOptions)
{
#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(launchOptions);
    // On iOS, we don't need to leak a boost message because RunningBoard process assertions give us the
    // right priorities.
    return false;
#else
    // On Mac, leak a boost onto the NetworkProcess and GPUProcess.
#if ENABLE(GPU_PROCESS)
    if (launchOptions.processType == ProcessLauncher::ProcessType::GPU)
        return true;
#endif
    return launchOptions.processType == ProcessLauncher::ProcessType::Network;
#endif
}

void ProcessLauncher::launchProcess()
{
    ASSERT(!m_xpcConnection);

    const char* name;
    if (!m_launchOptions.customWebContentServiceBundleIdentifier.isNull())
        name = m_launchOptions.customWebContentServiceBundleIdentifier.data();
    else
        name = serviceName(m_launchOptions, m_client);

    m_xpcConnection = adoptOSObject(xpc_connection_create(name, nullptr));

    uuid_t uuid;
    uuid_generate(uuid);
    xpc_connection_set_oneshot_instance(m_xpcConnection.get(), uuid);

    // Inherit UI process localization. It can be different from child process default localization:
    // 1. When the application and system frameworks simply have different localized resources available, we should match the application.
    // 1.1. An important case is WebKitTestRunner, where we should use English localizations for all system frameworks.
    // 2. When AppleLanguages is passed as command line argument for UI process, or set in its preferences, we should respect it in child processes.
    auto initializationMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    _CFBundleSetupXPCBootstrap(initializationMessage.get());
#if PLATFORM(IOS_FAMILY)
    // Clients that set these environment variables explicitly do not have the values automatically forwarded by libxpc.
    auto containerEnvironmentVariables = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    if (const char* environmentHOME = getenv("HOME"))
        xpc_dictionary_set_string(containerEnvironmentVariables.get(), "HOME", environmentHOME);
    if (const char* environmentCFFIXED_USER_HOME = getenv("CFFIXED_USER_HOME"))
        xpc_dictionary_set_string(containerEnvironmentVariables.get(), "CFFIXED_USER_HOME", environmentCFFIXED_USER_HOME);
    if (const char* environmentTMPDIR = getenv("TMPDIR"))
        xpc_dictionary_set_string(containerEnvironmentVariables.get(), "TMPDIR", environmentTMPDIR);
    xpc_dictionary_set_value(initializationMessage.get(), "ContainerEnvironmentVariables", containerEnvironmentVariables.get());
#endif

    auto languagesIterator = m_launchOptions.extraInitializationData.find<HashTranslatorASCIILiteral>("OverrideLanguages"_s);
    if (languagesIterator != m_launchOptions.extraInitializationData.end()) {
        LOG_WITH_STREAM(Language, stream << "Process Launcher is copying OverrideLanguages into initialization message: " << languagesIterator->value);
        auto languages = adoptOSObject(xpc_array_create(nullptr, 0));
        for (auto language : StringView(languagesIterator->value).split(','))
            xpc_array_set_string(languages.get(), XPC_ARRAY_APPEND, language.utf8().data());
        xpc_dictionary_set_value(initializationMessage.get(), "OverrideLanguages", languages.get());
    }

#if PLATFORM(MAC)
    xpc_dictionary_set_string(initializationMessage.get(), "WebKitBundleVersion", [[NSBundle bundleWithIdentifier:@"com.apple.WebKit"].infoDictionary[(__bridge NSString *)kCFBundleVersionKey] UTF8String]);
#endif
    xpc_connection_set_bootstrap(m_xpcConnection.get(), initializationMessage.get());

    if (shouldLeakBoost(m_launchOptions)) {
        auto preBootstrapMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(preBootstrapMessage.get(), "message-name", "pre-bootstrap");
        xpc_connection_send_message(m_xpcConnection.get(), preBootstrapMessage.get());
    }

    // Create the listening port.
    mach_port_t listeningPort = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    if (kr != KERN_SUCCESS) {
        LOG_ERROR("Could not allocate mach port, error %x: %s", kr, mach_error_string(kr));
        CRASH();
    }

    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    mach_port_t previousNotificationPort = MACH_PORT_NULL;
    auto mc = mach_port_request_notification(mach_task_self(), listeningPort, MACH_NOTIFY_NO_SENDERS, 0, listeningPort, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previousNotificationPort);
    ASSERT(!previousNotificationPort);
    ASSERT(mc == KERN_SUCCESS);
    if (mc != KERN_SUCCESS) {
        // If mach_port_request_notification fails, 'previousNotificationPort' will be uninitialized.
        LOG_ERROR("mach_port_request_notification failed: (%x) %s", mc, mach_error_string(mc));
    }

    String clientIdentifier;
#if PLATFORM(MAC)
    clientIdentifier = codeSigningIdentifierForCurrentProcess();
#endif
    if (clientIdentifier.isNull())
        clientIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    // FIXME: Switch to xpc_connection_set_bootstrap once it's available everywhere we need.
    auto bootstrapMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));

    if (m_client) {
        if (m_client->shouldConfigureJSCForTesting())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "configure-jsc-for-testing", true);
        if (!m_client->isJITEnabled())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "disable-jit", true);
        if (m_client->shouldEnableSharedArrayBuffer())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "enable-shared-array-buffer", true);
        if (m_client->shouldEnableCaptivePortalMode())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "enable-captive-portal-mode", true);
    }

    xpc_dictionary_set_string(bootstrapMessage.get(), "message-name", "bootstrap");

    xpc_dictionary_set_mach_send(bootstrapMessage.get(), "server-port", listeningPort);

    xpc_dictionary_set_string(bootstrapMessage.get(), "client-identifier", !clientIdentifier.isEmpty() ? clientIdentifier.utf8().data() : *_NSGetProgname());
    xpc_dictionary_set_string(bootstrapMessage.get(), "client-bundle-identifier", WebCore::applicationBundleIdentifier().utf8().data());
    xpc_dictionary_set_string(bootstrapMessage.get(), "process-identifier", String::number(m_launchOptions.processIdentifier.toUInt64()).utf8().data());
    xpc_dictionary_set_string(bootstrapMessage.get(), "ui-process-name", [[[NSProcessInfo processInfo] processName] UTF8String]);
    xpc_dictionary_set_string(bootstrapMessage.get(), "service-name", name);

    bool isWebKitDevelopmentBuild = ![[[[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] bundlePath] stringByDeletingLastPathComponent] hasPrefix:FileSystem::systemDirectoryPath()];
    if (isWebKitDevelopmentBuild) {
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stdout", STDOUT_FILENO);
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stderr", STDERR_FILENO);
    }
    
    auto sdkBehaviors = sdkAlignedBehaviors();
    xpc_dictionary_set_data(bootstrapMessage.get(), "client-sdk-aligned-behaviors", sdkBehaviors.storage(), sdkBehaviors.storageLengthInBytes());

    auto extraInitializationData = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));

    for (const auto& keyValuePair : m_launchOptions.extraInitializationData)
        xpc_dictionary_set_string(extraInitializationData.get(), keyValuePair.key.utf8().data(), keyValuePair.value.utf8().data());

    xpc_dictionary_set_value(bootstrapMessage.get(), "extra-initialization-data", extraInitializationData.get());

    auto errorHandlerImpl = [weakProcessLauncher = WeakPtr { *this }, listeningPort, logName = CString(name)] (xpc_object_t event) {
        ASSERT(!event || xpc_get_type(event) == XPC_TYPE_ERROR);

        auto processLauncher = weakProcessLauncher.get();
        if (!processLauncher)
            return;

        if (!processLauncher->isLaunching())
            return;

#if ERROR_DISABLED
        UNUSED_PARAM(logName);
#endif

        if (event)
            LOG_ERROR("Error while launching %s: %s", logName.data(), xpc_dictionary_get_string(event, XPC_ERROR_KEY_DESCRIPTION));
        else
            LOG_ERROR("Error while launching %s: No xpc_object_t event available.", logName.data());

#if ASSERT_ENABLED
        mach_port_urefs_t sendRightCount = 0;
        mach_port_get_refs(mach_task_self(), listeningPort, MACH_PORT_RIGHT_SEND, &sendRightCount);
        ASSERT(sendRightCount >= 1);
#endif

        // We failed to launch. Release the send right.
        deallocateSendRightSafely(listeningPort);

        // And the receive right.
        mach_port_mod_refs(mach_task_self(), listeningPort, MACH_PORT_RIGHT_RECEIVE, -1);

        if (processLauncher->m_xpcConnection)
            xpc_connection_cancel(processLauncher->m_xpcConnection.get());
        processLauncher->m_xpcConnection = nullptr;

        processLauncher->didFinishLaunchingProcess(0, IPC::Connection::Identifier());
    };

    auto eventHandler = [errorHandlerImpl = WTFMove(errorHandlerImpl), eventHandler = m_client->xpcEventHandler()] (xpc_object_t event) mutable {

        if (!event || xpc_get_type(event) == XPC_TYPE_ERROR) {
            RunLoop::main().dispatch([errorHandlerImpl = WTFMove(errorHandlerImpl), event = OSObjectPtr(event)] {
                errorHandlerImpl(event.get());
            });
            return;
        }

        if (eventHandler) {
            RunLoop::main().dispatch([eventHandler = eventHandler, event = OSObjectPtr(event)] {
                eventHandler->handleXPCEvent(event.get());
            });
        }
    };

    xpc_connection_set_event_handler(m_xpcConnection.get(), eventHandler);

    xpc_connection_resume(m_xpcConnection.get());

    if (UNLIKELY(m_launchOptions.shouldMakeProcessLaunchFailForTesting)) {
        eventHandler(nullptr);
        return;
    }

    ref();
    xpc_connection_send_message_with_reply(m_xpcConnection.get(), bootstrapMessage.get(), dispatch_get_main_queue(), ^(xpc_object_t reply) {
        // Errors are handled in the event handler.
        // It is possible for this block to be called after the error event handler, in which case we're no longer
        // launching and we already took care of cleaning things up.
        if (isLaunching() && xpc_get_type(reply) != XPC_TYPE_ERROR) {
            ASSERT(xpc_get_type(reply) == XPC_TYPE_DICTIONARY);
            ASSERT(!strcmp(xpc_dictionary_get_string(reply, "message-name"), "process-finished-launching"));

#if ASSERT_ENABLED
            mach_port_urefs_t sendRightCount = 0;
            mach_port_get_refs(mach_task_self(), listeningPort, MACH_PORT_RIGHT_SEND, &sendRightCount);
            ASSERT(sendRightCount >= 1);
#endif

            deallocateSendRightSafely(listeningPort);

            if (!m_xpcConnection) {
                // The process was terminated.
                didFinishLaunchingProcess(0, IPC::Connection::Identifier());
                return;
            }

            // The process has finished launching, grab the pid from the connection.
            pid_t processIdentifier = xpc_connection_get_pid(m_xpcConnection.get());

            didFinishLaunchingProcess(processIdentifier, IPC::Connection::Identifier(listeningPort, m_xpcConnection));
            m_xpcConnection = nullptr;
        }

        deref();
    });
}

void ProcessLauncher::terminateProcess()
{
    if (m_isLaunching) {
        terminateXPCConnection();
        return;
    }

    if (!m_processIdentifier)
        return;

    kill(m_processIdentifier, SIGKILL);
    m_processIdentifier = 0;
}

void ProcessLauncher::platformInvalidate()
{
    terminateXPCConnection();
}

void ProcessLauncher::terminateXPCConnection()
{
    if (!m_xpcConnection)
        return;

    xpc_connection_cancel(m_xpcConnection.get());
    terminateWithReason(m_xpcConnection.get(), WebKit::ReasonCode::Invalidation, "ProcessLauncher::platformInvalidate");
    m_xpcConnection = nullptr;
}

void terminateWithReason(xpc_connection_t connection, ReasonCode, const char*)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    xpc_connection_kill(connection, SIGKILL);
    ALLOW_DEPRECATED_DECLARATIONS_END
}

} // namespace WebKit
