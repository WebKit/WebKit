/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#import "DynamicLinkerEnvironmentExtractor.h"
#import "EnvironmentVariables.h"
#import <WebCore/CFBundleSPI.h>
#import <WebCore/ServersSPI.h>
#import <WebCore/SoftLinking.h>
#import <WebCore/WebCoreNSStringExtras.h>
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <mach/machine.h>
#import <spawn.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Threading.h>
#import <wtf/spi/darwin/XPCSPI.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

namespace WebKit {

namespace {

struct UUIDHolder : public RefCounted<UUIDHolder> {
    static Ref<UUIDHolder> create()
    {
        return adoptRef(*new UUIDHolder);
    }

    UUIDHolder()
    {
        uuid_generate(uuid);
    }

    uuid_t uuid;
};

}

#if ASAN_ENABLED
static const char* copyASanDynamicLibraryPath()
{
    uint32_t imageCount = _dyld_image_count();
    for (uint32_t i = 0; i < imageCount; ++i) {
        if (strstr(_dyld_get_image_name(i), "/libclang_rt.asan_"))
            return fastStrDup(_dyld_get_image_name(i));
    }

    return 0;
}
#endif

typedef void (ProcessLauncher::*DidFinishLaunchingProcessFunction)(PlatformProcessIdentifier, IPC::Connection::Identifier);

static const char* serviceName(const ProcessLauncher::LaunchOptions& launchOptions)
{
    switch (launchOptions.processType) {
    case ProcessLauncher::WebProcess:
        return "com.apple.WebKit.WebContent" WK_XPC_SERVICE_SUFFIX;
    case ProcessLauncher::NetworkProcess:
        return "com.apple.WebKit.Networking" WK_XPC_SERVICE_SUFFIX;
#if ENABLE(DATABASE_PROCESS)
    case ProcessLauncher::DatabaseProcess:
        return "com.apple.WebKit.Databases" WK_XPC_SERVICE_SUFFIX;
#endif
#if ENABLE(NETSCAPE_PLUGIN_API)
    case ProcessLauncher::PluginProcess:
        // FIXME: Support plugins that require an executable heap.
        if (launchOptions.architecture == CPU_TYPE_X86)
            return "com.apple.WebKit.Plugin.32" WK_XPC_SERVICE_SUFFIX;
        if (launchOptions.architecture == CPU_TYPE_X86_64)
            return "com.apple.WebKit.Plugin.64" WK_XPC_SERVICE_SUFFIX;

        ASSERT_NOT_REACHED();
        return 0;
#endif
    }
}
    
static bool shouldLeakBoost(const ProcessLauncher::LaunchOptions& launchOptions)
{
#if PLATFORM(IOS)
    // On iOS, leak a boost onto all child processes
    UNUSED_PARAM(launchOptions);
    return true;
#else
    // On Mac, leak a boost onto the NetworkProcess.
    return launchOptions.processType == ProcessLauncher::NetworkProcess;
#endif
}
    
static void connectToService(const ProcessLauncher::LaunchOptions& launchOptions, bool forDevelopment, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction, UUIDHolder* instanceUUID)
{
    // Create a connection to the WebKit XPC service.
    auto connection = adoptOSObject(xpc_connection_create(serviceName(launchOptions), 0));
    xpc_connection_set_oneshot_instance(connection.get(), instanceUUID->uuid);

    // Inherit UI process localization. It can be different from child process default localization:
    // 1. When the application and system frameworks simply have different localized resources available, we should match the application.
    // 1.1. An important case is WebKitTestRunner, where we should use English localizations for all system frameworks.
    // 2. When AppleLanguages is passed as command line argument for UI process, or set in its preferences, we should respect it in child processes.
    auto initializationMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    _CFBundleSetupXPCBootstrap(initializationMessage.get());
#if PLATFORM(IOS)
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

    auto languagesIterator = launchOptions.extraInitializationData.find("OverrideLanguages");
    if (languagesIterator != launchOptions.extraInitializationData.end()) {
        auto languages = adoptOSObject(xpc_array_create(nullptr, 0));
        Vector<String> languageVector;
        languagesIterator->value.split(",", false, languageVector);
        for (auto& language : languageVector)
            xpc_array_set_string(languages.get(), XPC_ARRAY_APPEND, language.utf8().data());
        xpc_dictionary_set_value(initializationMessage.get(), "OverrideLanguages", languages.get());
    }

    xpc_connection_set_bootstrap(connection.get(), initializationMessage.get());

    // XPC requires having an event handler, even if it is not used.
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t event) { });
    xpc_connection_resume(connection.get());
    
    if (shouldLeakBoost(launchOptions)) {
        auto preBootstrapMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(preBootstrapMessage.get(), "message-name", "pre-bootstrap");
        xpc_connection_send_message(connection.get(), preBootstrapMessage.get());
    }
    
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    
    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
    CString clientIdentifier = bundleIdentifier ? String([[NSBundle mainBundle] bundleIdentifier]).utf8() : *_NSGetProgname();

    // FIXME: Switch to xpc_connection_set_bootstrap once it's available everywhere we need.
    auto bootstrapMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(bootstrapMessage.get(), "message-name", "bootstrap");

    xpc_dictionary_set_mach_send(bootstrapMessage.get(), "server-port", listeningPort);
    mach_port_deallocate(mach_task_self(), listeningPort);

    xpc_dictionary_set_string(bootstrapMessage.get(), "client-identifier", clientIdentifier.data());
    xpc_dictionary_set_string(bootstrapMessage.get(), "ui-process-name", [[[NSProcessInfo processInfo] processName] UTF8String]);

    if (forDevelopment) {
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stdout", STDOUT_FILENO);
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stderr", STDERR_FILENO);
    }

    auto extraInitializationData = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));

    for (const auto& keyValuePair : launchOptions.extraInitializationData)
        xpc_dictionary_set_string(extraInitializationData.get(), keyValuePair.key.utf8().data(), keyValuePair.value.utf8().data());

    xpc_dictionary_set_value(bootstrapMessage.get(), "extra-initialization-data", extraInitializationData.get());

    that->ref();

    xpc_connection_send_message_with_reply(connection.get(), bootstrapMessage.get(), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(xpc_object_t reply) {
        xpc_type_t type = xpc_get_type(reply);
        if (type == XPC_TYPE_ERROR) {
            // We failed to launch. Release the send right.
            mach_port_deallocate(mach_task_self(), listeningPort);

            // And the receive right.
            mach_port_mod_refs(mach_task_self(), listeningPort, MACH_PORT_RIGHT_RECEIVE, -1);

            RefPtr<ProcessLauncher> protector(that);
            RunLoop::main().dispatch([protector, didFinishLaunchingProcessFunction] {
                (*protector.*didFinishLaunchingProcessFunction)(0, IPC::Connection::Identifier());
            });
        } else {
            ASSERT(type == XPC_TYPE_DICTIONARY);
            ASSERT(!strcmp(xpc_dictionary_get_string(reply, "message-name"), "process-finished-launching"));

            // The process has finished launching, grab the pid from the connection.
            pid_t processIdentifier = xpc_connection_get_pid(connection.get());

            // We've finished launching the process, message back to the main run loop. This takes ownership of the connection.
            RefPtr<ProcessLauncher> protector(that);
            RunLoop::main().dispatch([protector, didFinishLaunchingProcessFunction, processIdentifier, listeningPort, connection] {
                (*protector.*didFinishLaunchingProcessFunction)(processIdentifier, IPC::Connection::Identifier(listeningPort, connection));
            });
        }

        that->deref();
    });
}

static void createService(const ProcessLauncher::LaunchOptions& launchOptions, bool forDevelopment, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction)
{
    // Generate the uuid for the service instance we are about to create.
    // FIXME: This UUID should be stored on the ChildProcessProxy.
    RefPtr<UUIDHolder> instanceUUID = UUIDHolder::create();
    connectToService(launchOptions, forDevelopment, that, didFinishLaunchingProcessFunction, instanceUUID.get());
}

static NSString *systemDirectoryPath()
{
    static NSString *path = [^{
#if PLATFORM(IOS_SIMULATOR)
        char *simulatorRoot = getenv("SIMULATOR_ROOT");
        return simulatorRoot ? [NSString stringWithFormat:@"%s/System/", simulatorRoot] : @"/System/";
#else
        return @"/System/";
#endif
    }() copy];

    return path;
}

void ProcessLauncher::launchProcess()
{
    bool isWebKitDevelopmentBuild = ![[[[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] bundlePath] stringByDeletingLastPathComponent] hasPrefix:systemDirectoryPath()];

    createService(m_launchOptions, isWebKitDevelopmentBuild, this, &ProcessLauncher::didFinishLaunchingProcess);
}

void ProcessLauncher::terminateProcess()
{
    if (m_isLaunching) {
        invalidate();
        return;
    }

    if (!m_processIdentifier)
        return;
    
    kill(m_processIdentifier, SIGKILL);
    m_processIdentifier = 0;
}
    
void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
