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

#import "AuxiliaryProcess.h"
#import "Logging.h"
#import "WebPreferencesDefaultValues.h"
#import "XPCUtilities.h"
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <mach/mach_error.h>
#import <mach/mach_init.h>
#import <mach/mach_traps.h>
#import <mach/machine.h>
#import <pal/spi/cocoa/ServersSPI.h>
#import <spawn.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <wtf/BlockPtr.h>
#import <wtf/FileSystem.h>
#import <wtf/MachSendRight.h>
#import <wtf/RunLoop.h>
#import <wtf/RuntimeApplicationChecks.h>
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

#if USE(EXTENSIONKIT)
#import "AssertionCapability.h"
#import "ExtensionKitSPI.h"
#import <BrowserEngineKit/BENetworkingProcess.h>
#import <BrowserEngineKit/BERenderingProcess.h>
#import <BrowserEngineKit/BEWebContentProcess.h>

#if USE(LEGACY_EXTENSIONKIT_SPI)
SOFT_LINK_FRAMEWORK_OPTIONAL(ServiceExtensions);
SOFT_LINK_CLASS_OPTIONAL(ServiceExtensions, _SEServiceConfiguration);
SOFT_LINK_CLASS_OPTIONAL(ServiceExtensions, _SEServiceManager);
#endif // USE(LEGACY_EXTENSIONKIT_SPI)

#endif // USE(EXTENSIONKIT)

namespace WebKit {

#if USE(EXTENSIONKIT)
static std::pair<ASCIILiteral, RetainPtr<NSString>> serviceNameAndIdentifier(ProcessLauncher::ProcessType processType, ProcessLauncher::Client* client, bool isRetryingLaunch)
{
    switch (processType) {
    case ProcessLauncher::ProcessType::Web: {
        if (client && client->shouldEnableLockdownMode())
            return { "com.apple.WebKit.WebContent"_s, @"com.apple.WebKit.WebContent.CaptivePortal" };
        return { "com.apple.WebKit.WebContent"_s, @"com.apple.WebKit.WebContent" };
    }
    case ProcessLauncher::ProcessType::Network:
        return { "com.apple.WebKit.Networking"_s, @"com.apple.WebKit.Networking" };
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        return { "com.apple.WebKit.GPU"_s, @"com.apple.WebKit.GPU" };
#endif
    }
}

#if USE(LEGACY_EXTENSIONKIT_SPI)
static void launchWithExtensionKitFallback(ProcessLauncher& processLauncher, ProcessLauncher::ProcessType processType, ProcessLauncher::Client* client, WTF::Function<void(ThreadSafeWeakPtr<ProcessLauncher> weakProcessLauncher, ExtensionProcess&& process, ASCIILiteral name, NSError *error)>&& handler)
{
    auto [name, identifier] = serviceNameAndIdentifier(processType, client, processLauncher.isRetryingLaunch());

    RetainPtr configuration = adoptNS([alloc_SEServiceConfigurationInstance() initWithServiceIdentifier:identifier.get()]);
    _SEServiceManager* manager = [get_SEServiceManagerClass() performSelector:@selector(sharedInstance)];

    switch (processType) {
    case ProcessLauncher::ProcessType::Web: {
        auto block = makeBlockPtr([handler = WTFMove(handler), weakProcessLauncher = ThreadSafeWeakPtr { processLauncher }, name = name](_SEExtensionProcess *_Nullable process, NSError *_Nullable error) {
            handler(WTFMove(weakProcessLauncher), process, name, error);
        });
        [manager performSelector:@selector(contentProcessWithConfiguration:completion:) withObject:configuration.get() withObject:block.get()];
        break;
    }
    case ProcessLauncher::ProcessType::Network: {
        auto block = makeBlockPtr([handler = WTFMove(handler), weakProcessLauncher = ThreadSafeWeakPtr { processLauncher }, name = name](_SEExtensionProcess *_Nullable process, NSError *_Nullable error) {
            handler(WTFMove(weakProcessLauncher), process, name, error);
        });
        [manager performSelector:@selector(networkProcessWithConfiguration:completion:) withObject:configuration.get() withObject:block.get()];
        break;
    }
    case ProcessLauncher::ProcessType::GPU: {
        auto block = makeBlockPtr([handler = WTFMove(handler), weakProcessLauncher = ThreadSafeWeakPtr { processLauncher }, name = name](_SEExtensionProcess *_Nullable process, NSError *_Nullable error) {
            handler(WTFMove(weakProcessLauncher), process, name, error);
        });
        [manager performSelector:@selector(gpuProcessWithConfiguration:completion:) withObject:configuration.get() withObject:block.get()];
        break;
    }
    }
}
#endif // USE(LEGACY_EXTENSIONKIT_SPI)

bool ProcessLauncher::hasExtensionsInAppBundle()
{
#if PLATFORM(IOS_SIMULATOR)
    static bool hasExtensions = false;
    static dispatch_once_t flag;
    dispatch_once(&flag, ^{
        hasExtensions = [[NSBundle mainBundle] pathForResource:@"WebContentExtension" ofType:@"appex" inDirectory:@"Extensions"]
            && [[NSBundle mainBundle] pathForResource:@"NetworkingExtension" ofType:@"appex" inDirectory:@"Extensions"]
            && [[NSBundle mainBundle] pathForResource:@"GPUExtension" ofType:@"appex" inDirectory:@"Extensions"];
    });
    return hasExtensions;
#else
    return false;
#endif
}

static void launchWithExtensionKit(ProcessLauncher& processLauncher, ProcessLauncher::ProcessType processType, ProcessLauncher::Client* client, WTF::Function<void(ThreadSafeWeakPtr<ProcessLauncher> weakProcessLauncher, ExtensionProcess&& process, ASCIILiteral name, NSError *error)>&& handler)
{
#if USE(LEGACY_EXTENSIONKIT_SPI)
    if (!ProcessLauncher::hasExtensionsInAppBundle()) {
        launchWithExtensionKitFallback(processLauncher, processType, client, WTFMove(handler));
        return;
    }
#endif
    auto [name, identifier] = serviceNameAndIdentifier(processType, client, processLauncher.isRetryingLaunch());

    switch (processType) {
    case ProcessLauncher::ProcessType::Web: {
        auto block = makeBlockPtr([handler = WTFMove(handler), weakProcessLauncher = ThreadSafeWeakPtr { processLauncher }, name = name](BEWebContentProcess *_Nullable process, NSError *_Nullable error) {
            handler(WTFMove(weakProcessLauncher), process, name, error);
        });
        if (ProcessLauncher::hasExtensionsInAppBundle())
            [BEWebContentProcess webContentProcessWithInterruptionHandler:^{ } completion:block.get()];
        else
            [BEWebContentProcess webContentProcessWithBundleID:identifier.get() interruptionHandler:^{ } completion:block.get()];
        break;
    }
    case ProcessLauncher::ProcessType::Network: {
        auto block = makeBlockPtr([handler = WTFMove(handler), weakProcessLauncher = ThreadSafeWeakPtr { processLauncher }, name = name](BENetworkingProcess *_Nullable process, NSError *_Nullable error) {
            handler(WTFMove(weakProcessLauncher), process, name, error);
        });
        if (ProcessLauncher::hasExtensionsInAppBundle())
            [BENetworkingProcess networkProcessWithInterruptionHandler:^{ } completion:block.get()];
        else
            [BENetworkingProcess networkProcessWithBundleID:identifier.get() interruptionHandler:^{ } completion:block.get()];
        break;
    }
    case ProcessLauncher::ProcessType::GPU: {
        auto block = makeBlockPtr([handler = WTFMove(handler), weakProcessLauncher = ThreadSafeWeakPtr { processLauncher }, name = name](BERenderingProcess *_Nullable process, NSError *_Nullable error) {
            handler(WTFMove(weakProcessLauncher), process, name, error);
        });
        if (ProcessLauncher::hasExtensionsInAppBundle())
            [BERenderingProcess renderingProcessWithInterruptionHandler:^{ } completion:block.get()];
        else
            [BERenderingProcess renderingProcessWithBundleID:identifier.get() interruptionHandler:^{ } completion:block.get()];
        break;
    }
    }
}
#endif // USE(EXTENSIONKIT)

#if !USE(EXTENSIONKIT) || !PLATFORM(IOS)
static ASCIILiteral webContentServiceName(const ProcessLauncher::LaunchOptions& launchOptions, ProcessLauncher::Client* client)
{
    if (client && client->shouldEnableLockdownMode())
        return "com.apple.WebKit.WebContent.CaptivePortal"_s;

    return launchOptions.nonValidInjectedCodeAllowed ? "com.apple.WebKit.WebContent.Development"_s : "com.apple.WebKit.WebContent"_s;
}

static ASCIILiteral serviceName(const ProcessLauncher::LaunchOptions& launchOptions, ProcessLauncher::Client* client)
{
    switch (launchOptions.processType) {
    case ProcessLauncher::ProcessType::Web:
        return webContentServiceName(launchOptions, client);
    case ProcessLauncher::ProcessType::Network:
        return "com.apple.WebKit.Networking"_s;
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        return "com.apple.WebKit.GPU"_s;
#endif
#if ENABLE(MODEL_PROCESS)
    case ProcessLauncher::ProcessType::Model:
        return "com.apple.WebKit.Model"_s;
#endif
    }
}
#endif // !USE(EXTENSIONKIT) || !PLATFORM(IOS)

#if USE(EXTENSIONKIT)
Ref<LaunchGrant> LaunchGrant::create(ExtensionProcess& process)
{
    return adoptRef(*new LaunchGrant(process));
}

LaunchGrant::LaunchGrant(ExtensionProcess& process)
{
    AssertionCapability capability(emptyString(), "com.apple.webkit"_s, "Foreground"_s);
    auto grant = process.grantCapability(capability.platformCapability());
    m_grant.setPlatformGrant(WTFMove(grant));
}

LaunchGrant::~LaunchGrant()
{
    m_grant.invalidate();
}
#endif

void ProcessLauncher::platformDestroy()
{
#if USE(EXTENSIONKIT)
    if (m_process)
        m_process->invalidate();
#endif
}

void ProcessLauncher::launchProcess()
{
    ASSERT(!m_xpcConnection);

#if USE(EXTENSIONKIT)
    auto handler = [](ThreadSafeWeakPtr<ProcessLauncher> weakProcessLauncher, ExtensionProcess&& process, ASCIILiteral name, NSError *error)
    {
        if (error) {
            RELEASE_LOG_FAULT(Process, "Error launching process, description '%s', reason '%s'", String([error localizedDescription]).utf8().data(), String([error localizedFailureReason]).utf8().data());
#if PLATFORM(IOS)
            // Fallback to legacy extension identifiers
            // FIXME: this fallback is temporary and should be removed when possible. See rdar://120793705.
            callOnMainRunLoop([weakProcessLauncher = weakProcessLauncher] {
                auto launcher = weakProcessLauncher.get();
                if (!launcher || launcher->isRetryingLaunch())
                    return;
                launcher->setIsRetryingLaunch();
                launcher->launchProcess();
            });
#else
            // Fallback to XPC service launch
            callOnMainRunLoop([weakProcessLauncher = weakProcessLauncher] {
                auto launcher = weakProcessLauncher.get();
                if (!launcher)
                    return;
                auto name = serviceName(launcher->m_launchOptions, launcher->m_client);
                launcher->m_xpcConnection = adoptOSObject(xpc_connection_create(name, nullptr));
                launcher->finishLaunchingProcess(name);
            });
#endif
            process.invalidate();
            return;
        }

        Ref launchGrant = LaunchGrant::create(process);

        callOnMainRunLoop([weakProcessLauncher, name, process = WTFMove(process), launchGrant = WTFMove(launchGrant)] () mutable {
            RefPtr launcher = weakProcessLauncher.get();
            // If m_client is null, the Process launcher has been invalidated, and we should not proceed with the launch.
            if (!launcher || !launcher->m_client) {
                process.invalidate();
                return;
            }

            auto xpcConnection = process.makeLibXPCConnection();

            if (!xpcConnection) {
                RELEASE_LOG_ERROR(Process, "Failed to make libxpc connection for process");
                process.invalidate();
                launcher->didFinishLaunchingProcess(0, { });
                return;
            }

            launcher->m_xpcConnection = WTFMove(xpcConnection);
            launcher->m_process = WTFMove(process);
            launcher->m_launchGrant = WTFMove(launchGrant);
            launcher->finishLaunchingProcess(name);
        });
    };

    launchWithExtensionKit(*this, m_launchOptions.processType, m_client.get(), WTFMove(handler));
#else
    auto name = serviceName(m_launchOptions, m_client.get());
    m_xpcConnection = adoptOSObject(xpc_connection_create(name, nullptr));
    finishLaunchingProcess(name);
#endif
}

void ProcessLauncher::finishLaunchingProcess(ASCIILiteral name)
{
    uuid_t uuid;
    uuid_generate(uuid);

    xpc_connection_set_oneshot_instance(m_xpcConnection.get(), uuid);

    // Inherit UI process localization. It can be different from child process default localization:
    // 1. When the application and system frameworks simply have different localized resources available, we should match the application.
    // 1.1. An important case is WebKitTestRunner, where we should use English localizations for all system frameworks.
    // 2. When AppleLanguages is passed as command line argument for UI process, or set in its preferences, we should respect it in child processes.
#if !USE(EXTENSIONKIT)
    auto initializationMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    _CFBundleSetupXPCBootstrap(initializationMessage.get());
    xpc_connection_set_bootstrap(m_xpcConnection.get(), initializationMessage.get());
#endif

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

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    xpc_dictionary_set_string(bootstrapMessage.get(), "WebKitBundleVersion", WEBKIT_BUNDLE_VERSION);
#endif

    auto languagesIterator = m_launchOptions.extraInitializationData.find<HashTranslatorASCIILiteral>("OverrideLanguages"_s);
    if (languagesIterator != m_launchOptions.extraInitializationData.end()) {
        LOG_WITH_STREAM(Language, stream << "Process Launcher is copying OverrideLanguages into initialization message: " << languagesIterator->value);
        auto languages = adoptOSObject(xpc_array_create(nullptr, 0));
        for (auto language : StringView(languagesIterator->value).split(','))
            xpc_array_set_string(languages.get(), XPC_ARRAY_APPEND, language.utf8().data());
        xpc_dictionary_set_value(bootstrapMessage.get(), "OverrideLanguages", languages.get());
    }

#if PLATFORM(IOS_FAMILY)
    // Clients that set these environment variables explicitly do not have the values automatically forwarded by libxpc.
    auto containerEnvironmentVariables = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    if (const char* environmentHOME = getenv("HOME"))
        xpc_dictionary_set_string(containerEnvironmentVariables.get(), "HOME", environmentHOME);
    if (const char* environmentCFFIXED_USER_HOME = getenv("CFFIXED_USER_HOME"))
        xpc_dictionary_set_string(containerEnvironmentVariables.get(), "CFFIXED_USER_HOME", environmentCFFIXED_USER_HOME);
    if (const char* environmentTMPDIR = getenv("TMPDIR"))
        xpc_dictionary_set_string(containerEnvironmentVariables.get(), "TMPDIR", environmentTMPDIR);
    xpc_dictionary_set_value(bootstrapMessage.get(), "ContainerEnvironmentVariables", containerEnvironmentVariables.get());
#endif

    if (m_client) {
        if (m_client->shouldConfigureJSCForTesting())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "configure-jsc-for-testing", true);
        if (!m_client->isJITEnabled())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "disable-jit", true);
        if (m_client->shouldEnableSharedArrayBuffer())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "enable-shared-array-buffer", true);
        if (m_client->shouldDisableJITCage())
            xpc_dictionary_set_bool(bootstrapMessage.get(), "disable-jit-cage", true);
    }

    xpc_dictionary_set_string(bootstrapMessage.get(), "message-name", "bootstrap");

    xpc_dictionary_set_mach_send(bootstrapMessage.get(), "server-port", listeningPort);

    xpc_dictionary_set_string(bootstrapMessage.get(), "client-identifier", !clientIdentifier.isEmpty() ? clientIdentifier.utf8().data() : *_NSGetProgname());
    xpc_dictionary_set_string(bootstrapMessage.get(), "client-bundle-identifier", applicationBundleIdentifier().utf8().data());
    xpc_dictionary_set_string(bootstrapMessage.get(), "process-identifier", String::number(m_launchOptions.processIdentifier.toUInt64()).utf8().data());
    RetainPtr processName = [&] {
#if PLATFORM(MAC)
        if (auto name = NSRunningApplication.currentApplication.localizedName; name.length)
            return name;
#endif
        return NSProcessInfo.processInfo.processName;
    }();
    xpc_dictionary_set_string(bootstrapMessage.get(), "ui-process-name", [processName UTF8String]);
    xpc_dictionary_set_string(bootstrapMessage.get(), "service-name", name);

    if (m_launchOptions.processType == ProcessLauncher::ProcessType::Web) {
#if ENABLE(REMOVE_XPC_AND_MACH_SANDBOX_EXTENSIONS_IN_WEBCONTENT)
        bool disableLogging = true;
#else
        bool disableLogging = m_client->shouldEnableLockdownMode();
#endif
        xpc_dictionary_set_bool(bootstrapMessage.get(), "disable-logging", disableLogging);
    }

    if (!AuxiliaryProcess::isSystemWebKit()) {
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stdout", STDOUT_FILENO);
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stderr", STDERR_FILENO);
    }
    
    auto sdkBehaviors = sdkAlignedBehaviors();
    xpc_dictionary_set_data(bootstrapMessage.get(), "client-sdk-aligned-behaviors", sdkBehaviors.storage(), sdkBehaviors.storageLengthInBytes());

    auto extraInitializationData = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));

    for (const auto& keyValuePair : m_launchOptions.extraInitializationData)
        xpc_dictionary_set_string(extraInitializationData.get(), keyValuePair.key.utf8().data(), keyValuePair.value.utf8().data());

    xpc_dictionary_set_value(bootstrapMessage.get(), "extra-initialization-data", extraInitializationData.get());

    Function<void(xpc_object_t)> errorHandlerImpl = [weakProcessLauncher = ThreadSafeWeakPtr { *this }, listeningPort, logName = CString(name)] (xpc_object_t event) {
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

    Function<void(xpc_object_t)> eventHandler = [errorHandlerImpl = WTFMove(errorHandlerImpl), xpcEventHandler = m_client->xpcEventHandler()] (xpc_object_t event) mutable {

        if (!event || xpc_get_type(event) == XPC_TYPE_ERROR) {
            RunLoop::main().dispatch([errorHandlerImpl = std::exchange(errorHandlerImpl, nullptr), event = OSObjectPtr(event)] {
                if (errorHandlerImpl)
                    errorHandlerImpl(event.get());
                else if (event.get() != XPC_ERROR_CONNECTION_INVALID)
                    LOG_ERROR("Multiple errors while launching: %@", event.get());
            });
            return;
        }

        if (xpcEventHandler) {
            RunLoop::main().dispatch([xpcEventHandler = xpcEventHandler, event = OSObjectPtr(event)] {
                xpcEventHandler->handleXPCEvent(event.get());
            });
        }
    };

    auto eventHandlerBlock = makeBlockPtr(WTFMove(eventHandler));
    xpc_connection_set_event_handler(m_xpcConnection.get(), eventHandlerBlock.get());

    xpc_connection_resume(m_xpcConnection.get());

    if (UNLIKELY(m_launchOptions.shouldMakeProcessLaunchFailForTesting)) {
        eventHandlerBlock(nullptr);
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
#if USE(EXTENSIONKIT)
    if (m_process)
        m_process->invalidate();
#endif

    terminateXPCConnection();

    m_processID = 0;
}

void ProcessLauncher::platformInvalidate()
{
#if USE(EXTENSIONKIT)
    releaseLaunchGrant();
    if (m_process)
        m_process->invalidate();
#endif

    terminateXPCConnection();
}

void ProcessLauncher::terminateXPCConnection()
{
    if (!m_xpcConnection)
        return;

    xpc_connection_cancel(m_xpcConnection.get());
#if !USE(EXTENSIONKIT)
    terminateWithReason(m_xpcConnection.get(), WebKit::ReasonCode::Invalidation, "ProcessLauncher::platformInvalidate");
#endif
    m_xpcConnection = nullptr;
}

} // namespace WebKit
