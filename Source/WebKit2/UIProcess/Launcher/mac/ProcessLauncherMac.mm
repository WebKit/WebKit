/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#import "RunLoop.h"
#import "WebProcess.h"
#import "WebKitSystemInterface.h"
#import <crt_externs.h>
#import <mach-o/dyld.h>
#import <mach/machine.h>
#import <runtime/InitializeThreading.h>
#import <servers/bootstrap.h>
#import <spawn.h>
#import <sys/param.h>
#import <sys/stat.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/Threading.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);

namespace WebKit {

static void setUpTerminationNotificationHandler(pid_t pid)
{
#if HAVE(DISPATCH_H)
    dispatch_source_t processDiedSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, pid, DISPATCH_PROC_EXIT, dispatch_get_current_queue());
    dispatch_source_set_event_handler(processDiedSource, ^{
        int status;
        waitpid(dispatch_source_get_handle(processDiedSource), &status, 0);
        dispatch_source_cancel(processDiedSource);
    });
    dispatch_source_set_cancel_handler(processDiedSource, ^{
        dispatch_release(processDiedSource);
    });
    dispatch_resume(processDiedSource);
#endif
}

void ProcessLauncher::launchProcess()
{
    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    
    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    pid_t processIdentifier = 0;

    EnvironmentVariables environmentVariables;
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    static const char* preexistingProcessServiceName = environmentVariables.get(EnvironmentVariables::preexistingProcessServiceNameKey());
    ProcessType preexistingProcessType;
    if (preexistingProcessServiceName)
        getProcessTypeFromString(environmentVariables.get(EnvironmentVariables::preexistingProcessTypeKey()), preexistingProcessType);

    bool usePreexistingProcess = preexistingProcessServiceName && preexistingProcessType == m_launchOptions.processType;

    if (usePreexistingProcess) {
        mach_port_t lookupPort;
        bootstrap_look_up(bootstrap_port, preexistingProcessServiceName, &lookupPort);

        mach_msg_header_t header;
        header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
        header.msgh_id = 0;
        header.msgh_local_port = listeningPort;
        header.msgh_remote_port = lookupPort;
        header.msgh_size = sizeof(header);
        kern_return_t kr = mach_msg(&header, MACH_SEND_MSG, sizeof(header), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

        if (kr) {
            LOG_ERROR("Failed to pick up preexisting process at %s (%x). Launching a new process of type %s instead.", preexistingProcessServiceName, kr, processTypeAsString(m_launchOptions.processType));
            usePreexistingProcess = false;
        }

        mach_port_deallocate(mach_task_self(), lookupPort);

        preexistingProcessServiceName = 0;
    }

    if (!usePreexistingProcess) {
#endif
        NSBundle *webKit2Bundle = [NSBundle bundleWithIdentifier:@"com.apple.WebKit2"];
        NSString *frameworksPath = [[webKit2Bundle bundlePath] stringByDeletingLastPathComponent];
        const char* frameworkExecutablePath = [[webKit2Bundle executablePath] fileSystemRepresentation];

        NSString *processPath;
        if (m_launchOptions.processType == ProcessLauncher::PluginProcess)
            processPath = [webKit2Bundle pathForAuxiliaryExecutable:@"PluginProcess.app"];
        else
            processPath = [webKit2Bundle pathForAuxiliaryExecutable:@"WebProcess.app"];

        NSString *processAppExecutablePath = [[NSBundle bundleWithPath:processPath] executablePath];

        RetainPtr<CFStringRef> cfLocalization(AdoptCF, WKCopyCFLocalizationPreferredName(NULL));
        CString localization = String(cfLocalization.get()).utf8();
        
        // Make a unique, per pid, per process launcher web process service name.
        CString serviceName = String::format("com.apple.WebKit.WebProcess-%d-%p", getpid(), this).utf8();

        const char* args[] = { [processAppExecutablePath fileSystemRepresentation], frameworkExecutablePath, "-type", processTypeAsString(m_launchOptions.processType), "-servicename", serviceName.data(), "-localization", localization.data(), 0 };

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
        cpu_type_t architecture = m_launchOptions.architecture;
        if (architecture == LaunchOptions::MatchCurrentArchitecture)
            architecture = _NSGetMachExecuteHeader()->cputype;

        cpu_type_t cpuTypes[] = { architecture };
        size_t outCount = 0;
        posix_spawnattr_setbinpref_np(&attr, 1, cpuTypes, &outCount);

        // Start suspended so we can set up the termination notification handler.
        flags |= POSIX_SPAWN_START_SUSPENDED;

#ifndef BUILDING_ON_SNOW_LEOPARD
        static const int allowExecutableHeapFlag = 0x2000;
        if (m_launchOptions.executableHeap)
            flags |= allowExecutableHeapFlag;
#endif

        posix_spawnattr_setflags(&attr, flags);

#ifndef BUILDING_ON_SNOW_LEOPARD
        DynamicLinkerEnvironmentExtractor environmentExtractor([[NSBundle mainBundle] executablePath], architecture);
        environmentExtractor.getExtractedEnvironmentVariables(environmentVariables);
#endif

        // To make engineering builds work, if the path is outside of /System set up
        // DYLD_FRAMEWORK_PATH to pick up other frameworks, but don't do it for the
        // production configuration because it involves extra file system access.
        if (![frameworksPath hasPrefix:@"/System/"])
            environmentVariables.appendValue("DYLD_FRAMEWORK_PATH", [frameworksPath fileSystemRepresentation], ':');

        NSString *processShimPathNSString = nil;
        if (m_launchOptions.processType == ProcessLauncher::PluginProcess)
            processShimPathNSString = [[processAppExecutablePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"PluginProcessShim.dylib"];
        else if (m_launchOptions.processType == ProcessLauncher::WebProcess)
            processShimPathNSString = [[processAppExecutablePath stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"WebProcessShim.dylib"];

        // Make sure that the shim library file exists and insert it.
        if (processShimPathNSString) {
            const char* processShimPath = [processShimPathNSString fileSystemRepresentation];
            struct stat statBuf;
            if (stat(processShimPath, &statBuf) == 0 && (statBuf.st_mode & S_IFMT) == S_IFREG)
                environmentVariables.appendValue("DYLD_INSERT_LIBRARIES", processShimPath, ':');
        }
        
        int result = posix_spawn(&processIdentifier, args[0], 0, &attr, const_cast<char**>(args), environmentVariables.environmentPointer());

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
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    }
#endif

    // We've finished launching the process, message back to the main run loop.
    RunLoop::main()->dispatch(bind(&ProcessLauncher::didFinishLaunchingProcess, this, processIdentifier, listeningPort));
}

void ProcessLauncher::terminateProcess()
{    
    if (!m_processIdentifier)
        return;
    
    kill(m_processIdentifier, SIGKILL);
}
    
void ProcessLauncher::platformInvalidate()
{
}

} // namespace WebKit
