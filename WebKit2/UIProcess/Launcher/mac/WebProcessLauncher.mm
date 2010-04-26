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

#include "WebProcessLauncher.h"

#include "RunLoop.h"
#include "WebProcess.h"
#include "WebSystemInterface.h"
#include <crt_externs.h>
#include <runtime/InitializeThreading.h>
#include <servers/bootstrap.h>
#include <spawn.h>
#include <wtf/PassRefPtr.h>
#include <wtf/Threading.h>

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_register2(mach_port_t, name_t, mach_port_t, uint64_t);

namespace WebKit {

static void* webThreadBody(void* context)
{
    mach_port_t serverPort = static_cast<mach_port_t>(reinterpret_cast<uintptr_t>(context));
    
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    InitWebCoreSystemInterface();
    JSC::initializeThreading();
    WTF::initializeMainThread();

    WebProcess::shared().initialize(serverPort, RunLoop::current());

    [pool drain];

    RunLoop::current()->run();

    return 0;
}

ProcessInfo launchWebProcess(CoreIPC::Connection::Client* client, ProcessModel processModel)
{
    ProcessInfo info = { 0, 0 };

    // Create the listening port.
    mach_port_t listeningPort;
    mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &listeningPort);
    
    // Insert a send right so we can send to it.
    mach_port_insert_right(mach_task_self(), listeningPort, listeningPort, MACH_MSG_TYPE_MAKE_SEND);

    info.connection = CoreIPC::Connection::createServerConnection(listeningPort, client, RunLoop::main());
    info.connection->open();

    switch (processModel) {
        case ProcessModelSecondaryThread: {
            // Pass it to the thread.
            if (!createThread(webThreadBody, reinterpret_cast<void*>(listeningPort), "WebKit2: WebThread")) {
                // FIXME: Destroy ports.
                return info;
            }
            }
            break;
        case ProcessModelSecondaryProcess: {
            NSString *webProcessAppPath = [[NSBundle bundleWithIdentifier:@"com.apple.WebKit2"] pathForAuxiliaryExecutable:@"WebProcess.app"];
            NSString *webProcessAppExecutablePath = [[NSBundle bundleWithPath:webProcessAppPath] executablePath];

            const char* path = [webProcessAppExecutablePath fileSystemRepresentation];
            const char* args[] = { path, 0 };

            // Register ourselves.
            kern_return_t kr = bootstrap_register2(bootstrap_port, (char*)"com.apple.WebKit.WebProcess", listeningPort, /* BOOTSTRAP_PER_PID_SERVICE */ 1);
            if (kr)
                NSLog(@"bootstrap_register2 result: %x", kr);
            
            int result = posix_spawn(&info.processIdentifier, path, 0, 0, (char *const*)args, *_NSGetEnviron());
            if (result)
                NSLog(@"posix_spawn result: %d", result);
        }
        break;
    }

    return info;
}

} // namespace WebKit
