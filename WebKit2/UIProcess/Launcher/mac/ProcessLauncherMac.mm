/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "ProcessLauncher.h"

#include "RunLoop.h"
#include "WebProcess.h"
#include "WebSystemInterface.h"
#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <mach/machine.h>
#include <runtime/InitializeThreading.h>
#include <servers/bootstrap.h>
#include <spawn.h>
#include <sys/param.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);

namespace WebKit {

#if !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
static const char* processName()
{
    return [[[NSProcessInfo processInfo] processName] fileSystemRepresentation];
}
#else
// -[NSProcessInfo processName] isn't thread-safe on Leopard and Snow Leopard so we have our own implementation.
static const char* createProcessName()
{
    uint32_t bufferSize = MAXPATHLEN;
    char executablePath[bufferSize];
    
    if (_NSGetExecutablePath(executablePath, &bufferSize))
        return "";
    
    const char *processName = strrchr(executablePath, '/') + 1;
    return strdup(processName);
}

static const char* processName()
{
    static const char* processName = createProcessName();
    return processName;
}
#endif

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
    
    NSString *webProcessAppPath = [[NSBundle bundleWithIdentifier:@"com.apple.WebKit2"] pathForAuxiliaryExecutable:@"WebProcess.app"];
    NSString *webProcessAppExecutablePath = [[NSBundle bundleWithPath:webProcessAppPath] executablePath];

    // Make a unique, per pid, per process launcher web process service name.
    CString serviceName = String::format("com.apple.WebKit.WebProcess-%d-%p", getpid(), this).utf8();

    const char* path = [webProcessAppExecutablePath fileSystemRepresentation];
    const char* args[] = { path, "-mode", "legacywebprocess", "-servicename", serviceName.data(), "-parentprocessname", processName(), 0 };

    // Register ourselves.
    kern_return_t kr = bootstrap_register2(bootstrap_port, const_cast<char*>(serviceName.data()), listeningPort, 0);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);

    // FIXME: Should we restore signals here?

#if CPU(X86)
    // Ensure that the child process runs as the same architecture as the parent process. 
    cpu_type_t cpuTypes[] = { CPU_TYPE_X86 };    
    size_t outCount = 0;
    posix_spawnattr_setbinpref_np(&attr, 1, cpuTypes, &outCount);
#endif

    // Start suspended so we can set up the termination notification handler.
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_START_SUSPENDED);

    pid_t processIdentifier;
    int result = posix_spawn(&processIdentifier, path, 0, &attr, (char *const*)args, *_NSGetEnviron());
    ASSERT_UNUSED(result, !result);

    posix_spawnattr_destroy(&attr);

    // Set up the termination notification handler and then ask the child process to continue.
    setUpTerminationNotificationHandler(processIdentifier);
    kill(processIdentifier, SIGCONT);
    
    // We've finished launching the process, message back to the main run loop.
    RunLoop::main()->scheduleWork(WorkItem::create(this, &ProcessLauncher::didFinishLaunchingProcess, processIdentifier, listeningPort));
}

void ProcessLauncher::terminateProcess()
{    
    if (!m_processIdentifier)
        return;
    
    kill(m_processIdentifier, SIGKILL);
}
    
} // namespace WebKit
