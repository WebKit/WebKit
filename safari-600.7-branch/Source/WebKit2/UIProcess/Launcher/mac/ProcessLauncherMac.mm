/*
 * Copyright (C) 2010-2014 Apple Inc. All rights reserved.
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
#import "WebKitSystemInterface.h"
#import <WebCore/SoftLinking.h>
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <mach/machine.h>
#import <servers/bootstrap.h>
#import <spawn.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Threading.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>
#import <xpc/xpc.h>

#if __has_include(<xpc/private.h>)
#import <xpc/private.h>
#endif

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);

extern "C" void xpc_connection_set_instance(xpc_connection_t, uuid_t);
extern "C" void xpc_dictionary_set_mach_send(xpc_object_t, const char*, mach_port_t);

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 10100
extern "C" void xpc_connection_set_bootstrap(xpc_connection_t connection, xpc_object_t bootstrap);

// FIXME: Soft linking is temporary, make this into a regular function call once this function is available everywhere we need.
SOFT_LINK_FRAMEWORK(CoreFoundation)
SOFT_LINK_OPTIONAL(CoreFoundation, _CFBundleSetupXPCBootstrap, void, unused, (xpc_object_t))

#endif

namespace WebKit {

namespace {

struct UUIDHolder : public RefCounted<UUIDHolder> {
    static PassRefPtr<UUIDHolder> create()
    {
        return adoptRef(new UUIDHolder);
    }

    UUIDHolder()
    {
        uuid_generate(uuid);
    }

    uuid_t uuid;
};

}

static void setUpTerminationNotificationHandler(pid_t pid)
{
    dispatch_source_t processDiedSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid, DISPATCH_PROC_EXIT, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    dispatch_source_set_event_handler(processDiedSource, ^{
        int status;
        waitpid(dispatch_source_get_handle(processDiedSource), &status, 0);
        dispatch_source_cancel(processDiedSource);
    });
    dispatch_source_set_cancel_handler(processDiedSource, ^{
        dispatch_release(processDiedSource);
    });
    dispatch_resume(processDiedSource);
}

static void addDYLDEnvironmentAdditions(const ProcessLauncher::LaunchOptions& launchOptions, bool isWebKitDevelopmentBuild, EnvironmentVariables& environmentVariables)
{
    DynamicLinkerEnvironmentExtractor environmentExtractor([[NSBundle mainBundle] executablePath], _NSGetMachExecuteHeader()->cputype);
    environmentExtractor.getExtractedEnvironmentVariables(environmentVariables);

    NSBundle *webKitBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];
    NSString *frameworksPath = [[webKitBundle bundlePath] stringByDeletingLastPathComponent];

    // To make engineering builds work, if the path is outside of /System set up
    // DYLD_FRAMEWORK_PATH to pick up other frameworks, but don't do it for the
    // production configuration because it involves extra file system access.
    if (isWebKitDevelopmentBuild)
        environmentVariables.appendValue("DYLD_FRAMEWORK_PATH", [frameworksPath fileSystemRepresentation], ':');

    NSString *processShimPathNSString = nil;
#if ENABLE(NETSCAPE_PLUGIN_API)
    if (launchOptions.processType == ProcessLauncher::PluginProcess) {
        NSString *processPath = [webKitBundle pathForAuxiliaryExecutable:@"PluginProcess.app"];
        NSString *processAppExecutablePath = [[NSBundle bundleWithPath:processPath] executablePath];

        processShimPathNSString = [[processAppExecutablePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"PluginProcessShim.dylib"];
    } else
#endif // ENABLE(NETSCAPE_PLUGIN_API)
#if ENABLE(NETWORK_PROCESS)
    if (launchOptions.processType == ProcessLauncher::NetworkProcess) {
        NSString *processPath = [webKitBundle pathForAuxiliaryExecutable:@"NetworkProcess.app"];
        NSString *processAppExecutablePath = [[NSBundle bundleWithPath:processPath] executablePath];

        processShimPathNSString = [[processAppExecutablePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"SecItemShim.dylib"];
    } else
#endif // ENABLE(NETWORK_PROCESS)
    if (launchOptions.processType == ProcessLauncher::WebProcess) {
        NSString *processPath = [webKitBundle pathForAuxiliaryExecutable:@"WebProcess.app"];
        NSString *processAppExecutablePath = [[NSBundle bundleWithPath:processPath] executablePath];

        processShimPathNSString = [[processAppExecutablePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"WebProcessShim.dylib"];
    }

    // Make sure that the shim library file exists and insert it.
    if (processShimPathNSString) {
        const char* processShimPath = [processShimPathNSString fileSystemRepresentation];
        struct stat statBuf;
        if (stat(processShimPath, &statBuf) == 0 && (statBuf.st_mode & S_IFMT) == S_IFREG)
            environmentVariables.appendValue("DYLD_INSERT_LIBRARIES", processShimPath, ':');
    }

}

typedef void (ProcessLauncher::*DidFinishLaunchingProcessFunction)(PlatformProcessIdentifier, IPC::Connection::Identifier);

static const char* serviceName(const ProcessLauncher::LaunchOptions& launchOptions, bool forDevelopment)
{
    switch (launchOptions.processType) {
    case ProcessLauncher::WebProcess:
        if (forDevelopment)
            return "com.apple.WebKit.WebContent.Development";
        return "com.apple.WebKit.WebContent";
#if ENABLE(NETWORK_PROCESS)
    case ProcessLauncher::NetworkProcess:
        if (forDevelopment)
            return "com.apple.WebKit.Networking.Development";
        return "com.apple.WebKit.Networking";
#endif
#if ENABLE(DATABASE_PROCESS)
    case ProcessLauncher::DatabaseProcess:
        if (forDevelopment)
            return "com.apple.WebKit.Databases.Development";
        return "com.apple.WebKit.Databases";
#endif
#if ENABLE(NETSCAPE_PLUGIN_API)
    case ProcessLauncher::PluginProcess:
        if (forDevelopment)
            return "com.apple.WebKit.Plugin.Development";

        // FIXME: Support plugins that require an executable heap.
        if (launchOptions.architecture == CPU_TYPE_X86)
            return "com.apple.WebKit.Plugin.32";
        if (launchOptions.architecture == CPU_TYPE_X86_64)
            return "com.apple.WebKit.Plugin.64";

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
#elif ENABLE(NETWORK_PROCESS)
    // On Mac, leak a boost onto the NetworkProcess.
    return launchOptions.processType == ProcessLauncher::NetworkProcess;
#else
    UNUSED_PARAM(launchOptions);
    return false;
#endif
}
    
static void connectToService(const ProcessLauncher::LaunchOptions& launchOptions, bool forDevelopment, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction, UUIDHolder* instanceUUID)
{
    // Create a connection to the WebKit XPC service.
    auto connection = IPC::adoptXPC(xpc_connection_create(serviceName(launchOptions, forDevelopment), 0));
    xpc_connection_set_instance(connection.get(), instanceUUID->uuid);

#if PLATFORM(IOS) || __MAC_OS_X_VERSION_MIN_REQUIRED >= 10100
    // Inherit UI process localization. It can be different from child process default localization:
    // 1. When the application and system frameworks simply have different localized resources available, we should match the application.
    // 1.1. An important case is WebKitTestRunner, where we should use English localizations for all system frameworks.
    // 2. When AppleLanguages is passed as command line argument for UI process, or set in its preferences, we should respect it in child processes.
    RetainPtr<CFStringRef> localization = adoptCF(WKCopyCFLocalizationPreferredName(0));
    if (localization && _CFBundleSetupXPCBootstrapPtr()) {
        auto initializationMessage = IPC::adoptXPC(xpc_dictionary_create(nullptr, nullptr, 0));
        _CFBundleSetupXPCBootstrapPtr()(initializationMessage.get());
        xpc_connection_set_bootstrap(connection.get(), initializationMessage.get());
    }
#endif

    // XPC requires having an event handler, even if it is not used.
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t event) { });
    xpc_connection_resume(connection.get());
    
    if (shouldLeakBoost(launchOptions)) {
        auto preBootstrapMessage = IPC::adoptXPC(xpc_dictionary_create(nullptr, nullptr, 0));
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
    auto bootstrapMessage = IPC::adoptXPC(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(bootstrapMessage.get(), "message-name", "bootstrap");
    xpc_dictionary_set_string(bootstrapMessage.get(), "framework-executable-path", [[[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] executablePath] fileSystemRepresentation]);
    xpc_dictionary_set_mach_send(bootstrapMessage.get(), "server-port", listeningPort);
    xpc_dictionary_set_string(bootstrapMessage.get(), "client-identifier", clientIdentifier.data());
    xpc_dictionary_set_string(bootstrapMessage.get(), "ui-process-name", [[[NSProcessInfo processInfo] processName] UTF8String]);

    if (forDevelopment) {
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stdout", STDOUT_FILENO);
        xpc_dictionary_set_fd(bootstrapMessage.get(), "stderr", STDERR_FILENO);
    }

    auto extraInitializationData = IPC::adoptXPC(xpc_dictionary_create(nullptr, nullptr, 0));

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

            RunLoop::main().dispatch(bind(didFinishLaunchingProcessFunction, that, 0, IPC::Connection::Identifier()));
        } else {
            ASSERT(type == XPC_TYPE_DICTIONARY);
            ASSERT(!strcmp(xpc_dictionary_get_string(reply, "message-name"), "process-finished-launching"));

            // The process has finished launching, grab the pid from the connection.
            pid_t processIdentifier = xpc_connection_get_pid(connection.get());

            // We've finished launching the process, message back to the main run loop. This takes ownership of the connection.
            RunLoop::main().dispatch(bind(didFinishLaunchingProcessFunction, that, processIdentifier, IPC::Connection::Identifier(listeningPort, connection)));
        }

        that->deref();
    });
}

static void connectToReExecService(const ProcessLauncher::LaunchOptions& launchOptions, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction)
{
    EnvironmentVariables environmentVariables;
    addDYLDEnvironmentAdditions(launchOptions, true, environmentVariables);

#if ENABLE(NETSCAPE_PLUGIN_API)
    if (launchOptions.processType == ProcessLauncher::PluginProcess) {
        // Tagged pointers break video in Flash, see bug 135354.
        environmentVariables.set("NSStringDisableTagged", "YES");
    }
#endif

    // Generate the uuid for the service instance we are about to create.
    // FIXME: This UUID should be stored on the ChildProcessProxy.
    RefPtr<UUIDHolder> instanceUUID = UUIDHolder::create();

    // FIXME: It would be nice if we could use XPCPtr for this connection as well, but we'd have to be careful
    // not to introduce any retain cycles in the call to xpc_connection_set_event_handler below.
    xpc_connection_t reExecConnection = xpc_connection_create(serviceName(launchOptions, true), 0);
    xpc_connection_set_instance(reExecConnection, instanceUUID->uuid);

    // Keep the ProcessLauncher alive while we do the re-execing (balanced in event handler).
    that->ref();

    // We wait for the connection to tear itself down (indicated via an error event)
    // to indicate that the service instance re-execed itself, and is now ready to be
    // connected to.
    xpc_connection_set_event_handler(reExecConnection, ^(xpc_object_t event) {
        ASSERT(xpc_get_type(event) == XPC_TYPE_ERROR);

        connectToService(launchOptions, true, that, didFinishLaunchingProcessFunction, instanceUUID.get());

        // Release the connection.
        xpc_release(reExecConnection);

        // Other end of ref called before we setup the event handler.
        that->deref();
    });
    xpc_connection_resume(reExecConnection);

    xpc_object_t reExecMessage = xpc_dictionary_create(0, 0, 0);
    xpc_dictionary_set_string(reExecMessage, "message-name", "re-exec");

    cpu_type_t architecture = launchOptions.architecture == ProcessLauncher::LaunchOptions::MatchCurrentArchitecture ? _NSGetMachExecuteHeader()->cputype : launchOptions.architecture;
    xpc_dictionary_set_uint64(reExecMessage, "architecture", (uint64_t)architecture);
    
    xpc_object_t environment = xpc_array_create(0, 0);
    char** environmentPointer = environmentVariables.environmentPointer();
    Vector<CString> temps;
    for (size_t i = 0; environmentPointer[i]; ++i) {
        CString temp(environmentPointer[i], strlen(environmentPointer[i]));
        temps.append(temp);

        xpc_array_set_string(environment, XPC_ARRAY_APPEND, temp.data());
    }
    xpc_dictionary_set_value(reExecMessage, "environment", environment);
    xpc_release(environment);

    xpc_dictionary_set_bool(reExecMessage, "executable-heap", launchOptions.executableHeap);

    xpc_connection_send_message(reExecConnection, reExecMessage);
    xpc_release(reExecMessage);
}

static void createService(const ProcessLauncher::LaunchOptions& launchOptions, bool forDevelopment, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction)
{
    if (forDevelopment) {
        connectToReExecService(launchOptions, that, didFinishLaunchingProcessFunction);
        return;
    }

    // Generate the uuid for the service instance we are about to create.
    // FIXME: This UUID should be stored on the ChildProcessProxy.
    RefPtr<UUIDHolder> instanceUUID = UUIDHolder::create();
    connectToService(launchOptions, false, that, didFinishLaunchingProcessFunction, instanceUUID.get());
}

static bool tryPreexistingProcess(const ProcessLauncher::LaunchOptions& launchOptions, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction)
{
    EnvironmentVariables environmentVariables;
    static const char* preexistingProcessServiceName = environmentVariables.get(EnvironmentVariables::preexistingProcessServiceNameKey());

    ProcessLauncher::ProcessType preexistingProcessType;
    if (preexistingProcessServiceName)
        ProcessLauncher::getProcessTypeFromString(environmentVariables.get(EnvironmentVariables::preexistingProcessTypeKey()), preexistingProcessType);

    bool usePreexistingProcess = preexistingProcessServiceName && preexistingProcessType == launchOptions.processType;
    if (!usePreexistingProcess)
        return false;

    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    
    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    pid_t processIdentifier = 0;

    mach_port_t lookupPort;
    bootstrap_look_up(bootstrap_port, preexistingProcessServiceName, &lookupPort);

    mach_msg_header_t header;
    header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
    header.msgh_id = 0;
    header.msgh_local_port = listeningPort;
    header.msgh_remote_port = lookupPort;
    header.msgh_size = sizeof(header);
    kern_return_t kr = mach_msg(&header, MACH_SEND_MSG, sizeof(header), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    mach_port_deallocate(mach_task_self(), lookupPort);
    preexistingProcessServiceName = 0;

    if (kr) {
        LOG_ERROR("Failed to pick up preexisting process at %s (%x). Launching a new process of type %s instead.", preexistingProcessServiceName, kr, ProcessLauncher::processTypeAsString(launchOptions.processType));
        return false;
    }
    
    // We've finished launching the process, message back to the main run loop.
    RunLoop::main().dispatch(bind(didFinishLaunchingProcessFunction, that, processIdentifier, IPC::Connection::Identifier(listeningPort)));
    return true;
}

static void createProcess(const ProcessLauncher::LaunchOptions& launchOptions, bool isWebKitDevelopmentBuild, ProcessLauncher* that, DidFinishLaunchingProcessFunction didFinishLaunchingProcessFunction)
{
    EnvironmentVariables environmentVariables;
    addDYLDEnvironmentAdditions(launchOptions, isWebKitDevelopmentBuild, environmentVariables);

    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);

    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    NSBundle *webKitBundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit"];

    NSString *processPath = nil;
    switch (launchOptions.processType) {
    case ProcessLauncher::WebProcess:
        processPath = [webKitBundle pathForAuxiliaryExecutable:@"WebProcess.app"];
        break;
#if ENABLE(NETSCAPE_PLUGIN_API)
    case ProcessLauncher::PluginProcess:
        processPath = [webKitBundle pathForAuxiliaryExecutable:@"PluginProcess.app"];
        break;
#endif
#if ENABLE(NETWORK_PROCESS)
    case ProcessLauncher::NetworkProcess:
        processPath = [webKitBundle pathForAuxiliaryExecutable:@"NetworkProcess.app"];
        break;
#endif
#if ENABLE(DATABASE_PROCESS)
    case ProcessLauncher::DatabaseProcess:
        processPath = [webKitBundle pathForAuxiliaryExecutable:@"DatabaseProcess.app"];
        break;
#endif
    }

    NSString *frameworkExecutablePath = [webKitBundle executablePath];
    NSString *processAppExecutablePath = [[NSBundle bundleWithPath:processPath] executablePath];

    NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];
    CString clientIdentifier = bundleIdentifier ? String([[NSBundle mainBundle] bundleIdentifier]).utf8() : *_NSGetProgname();

    // Make a unique, per pid, per process launcher web process service name.
    CString serviceName = String::format("com.apple.WebKit.WebProcess-%d-%p", getpid(), that).utf8();

    // Inherit UI process localization. It can be different from child process default localization:
    // 1. When the application and system frameworks simply have different localized resources available, we should match the application.
    // 1.1. An important case is WebKitTestRunner, where we should use English localizations for all system frameworks.
    // 2. When AppleLanguages is passed as command line argument for UI process, or set in its preferences, we should respect it in child processes.
    CString appleLanguagesArgument = String("('" + String(adoptCF(WKCopyCFLocalizationPreferredName(0)).get()) + "')").utf8();

    Vector<const char*> args;
    args.append([processAppExecutablePath fileSystemRepresentation]);
    args.append([frameworkExecutablePath fileSystemRepresentation]);
    args.append("-type");
    args.append(ProcessLauncher::processTypeAsString(launchOptions.processType));
    args.append("-servicename");
    args.append(serviceName.data());
    args.append("-client-identifier");
    args.append(clientIdentifier.data());
    args.append("-ui-process-name");
    args.append([[[NSProcessInfo processInfo] processName] UTF8String]);
    args.append("-AppleLanguages"); // This argument will be handled by Core Foundation.
    args.append(appleLanguagesArgument.data());

    HashMap<String, String>::const_iterator it = launchOptions.extraInitializationData.begin();
    HashMap<String, String>::const_iterator end = launchOptions.extraInitializationData.end();
    Vector<CString> temps;
    for (; it != end; ++it) {
        String keyPlusDash = "-" + it->key;
        CString key(keyPlusDash.utf8().data());
        temps.append(key);
        args.append(key.data());

        CString value(it->value.utf8().data());
        temps.append(value);
        args.append(value.data());
    }

    args.append(nullptr);

    // Register ourselves.
    kern_return_t kr = bootstrap_register2(bootstrap_port, const_cast<char*>(serviceName.data()), listeningPort, 0);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    short flags = 0;

    // We want our process to receive all signals.
    sigset_t signalMaskSet;
    sigemptyset(&signalMaskSet);

    posix_spawnattr_setsigmask(&attr, &signalMaskSet);
    flags |= POSIX_SPAWN_SETSIGMASK;

    // Determine the architecture to use.
    cpu_type_t architecture = launchOptions.architecture;
    if (architecture == ProcessLauncher::LaunchOptions::MatchCurrentArchitecture)
        architecture = _NSGetMachExecuteHeader()->cputype;

    cpu_type_t cpuTypes[] = { architecture };
    size_t outCount = 0;
    posix_spawnattr_setbinpref_np(&attr, 1, cpuTypes, &outCount);

    // Start suspended so we can set up the termination notification handler.
    flags |= POSIX_SPAWN_START_SUSPENDED;

    static const int allowExecutableHeapFlag = 0x2000;
    if (launchOptions.executableHeap)
        flags |= allowExecutableHeapFlag;

    posix_spawnattr_setflags(&attr, flags);

    pid_t processIdentifier = 0;
    int result = posix_spawn(&processIdentifier, args[0], 0, &attr, const_cast<char**>(args.data()), environmentVariables.environmentPointer());

    posix_spawnattr_destroy(&attr);

    if (!result) {
        // Set up the termination notification handler and then ask the child process to continue.
        setUpTerminationNotificationHandler(processIdentifier);
        kill(processIdentifier, SIGCONT);
    } else {
        // We failed to launch. Release the send right.
        mach_port_deallocate(mach_task_self(), listeningPort);

        // And the receive right.
        mach_port_mod_refs(mach_task_self(), listeningPort, MACH_PORT_RIGHT_RECEIVE, -1);
    
        listeningPort = MACH_PORT_NULL;
        processIdentifier = 0;
    }

    // We've finished launching the process, message back to the main run loop.
    RunLoop::main().dispatch(bind(didFinishLaunchingProcessFunction, that, processIdentifier, IPC::Connection::Identifier(listeningPort)));
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
    if (tryPreexistingProcess(m_launchOptions, this, &ProcessLauncher::didFinishLaunchingProcess))
        return;

    bool isWebKitDevelopmentBuild = ![[[[NSBundle bundleWithIdentifier:@"com.apple.WebKit"] bundlePath] stringByDeletingLastPathComponent] hasPrefix:systemDirectoryPath()];

    if (m_launchOptions.useXPC) {
        createService(m_launchOptions, isWebKitDevelopmentBuild, this, &ProcessLauncher::didFinishLaunchingProcess);
        return;
    }

    createProcess(m_launchOptions, isWebKitDevelopmentBuild, this, &ProcessLauncher::didFinishLaunchingProcess);
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
