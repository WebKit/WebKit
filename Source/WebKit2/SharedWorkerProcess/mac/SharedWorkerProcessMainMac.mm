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
#import "SharedWorkerProcessMain.h"

#if ENABLE(SHARED_WORKER_PROCESS)

#import "CommandLine.h"
#import "EnvironmentUtilities.h"
#import "SharedWorkerProcess.h"
#import <Foundation/NSUserDefaults.h>
#import <WebCore/RunLoop.h>
#import <WebKitSystemInterface.h>
#import <mach/mach_error.h>
#import <runtime/InitializeThreading.h>
#import <servers/bootstrap.h>
#import <stdio.h>
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

#define SHOW_CRASH_REPORTER 1

using namespace WebCore;

namespace WebKit {

// FIXME: There is much code here that is duplicated in WebProcessMainMac.mm, we should add a shared base class where
// we can put everything.

int SharedWorkerProcessMain(const CommandLine& commandLine)
{
    String serviceName = commandLine["servicename"];
    if (serviceName.isEmpty())
        return EXIT_FAILURE;
    
    // Get the server port.
    mach_port_t serverPort;
    kern_return_t kr = bootstrap_look_up(bootstrap_port, serviceName.utf8().data(), &serverPort);
    if (kr) {
        WTFLogAlways("bootstrap_look_up result: %s (%x)\n", mach_error_string(kr), kr);
        return EXIT_FAILURE;
    }

    String localization = commandLine["localization"];
    if (!localization.isEmpty()) {
        RetainPtr<CFStringRef> cfLocalization(AdoptCF, CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar*>(localization.characters()), localization.length()));
        WKSetDefaultLocalization(cfLocalization.get());
    }

#if defined(__i386__)
    {
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

        NSDictionary *defaults = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithBool:YES], @"AppleMagnifiedMode", nil];
        [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];
        [defaults release];

        [pool drain];
    }
#endif

#if !SHOW_CRASH_REPORTER
    // Installs signal handlers that exit on a crash so that CrashReporter does not show up.
    signal(SIGILL, _exit);
    signal(SIGFPE, _exit);
    signal(SIGBUS, _exit);
    signal(SIGSEGV, _exit);
#endif

    @autoreleasepool {
        JSC::initializeThreading();
        WTF::initializeMainThread();
        RunLoop::initializeMainRunLoop();

        // Initialize the shared worker process connection.
        SharedWorkerProcess::shared().initializeConnection(CoreIPC::Connection::Identifier(serverPort));

        [NSApplication sharedApplication];
    }

    RunLoop::run();
    
    return 0;
}

}

#endif // ENABLE(SHARED_WORKER_PROCESS)
