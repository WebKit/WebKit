/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#import "WebProcessMain.h"

#import "CommandLine.h"
#import "EnvironmentUtilities.h"
#import "RunLoop.h"
#import "WebProcess.h"
#import "WebSystemInterface.h"
#import <WebKit2/WKView.h>
#import <WebKitSystemInterface.h>
#import <objc/objc-auto.h>
#import <runtime/InitializeThreading.h>
#import <servers/bootstrap.h>
#import <signal.h>
#import <stdio.h>
#import <sysexits.h>
#import <unistd.h>
#import <wtf/RetainPtr.h>
#import <wtf/Threading.h>
#import <wtf/text/CString.h>
#import <wtf/text/StringBuilder.h>

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_look_up2(mach_port_t, const name_t, mach_port_t*, pid_t, uint64_t);

@interface NSApplication (WebNSApplicationDetails)
-(void)_installAutoreleasePoolsOnCurrentThreadIfNecessary;
@end

#define SHOW_CRASH_REPORTER 1

using namespace WebCore;

namespace WebKit {

int WebProcessMain(const CommandLine& commandLine)
{
#ifdef BUILDING_ON_SNOW_LEOPARD
    // Remove the WebProcess shim from the DYLD_INSERT_LIBRARIES environment variable so any processes spawned by
    // the WebProcess don't try to insert the shim and crash.
    EnvironmentUtilities::stripValuesEndingWithString("DYLD_INSERT_LIBRARIES", "/WebProcessShim.dylib");
#endif

    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    String serviceName = commandLine["servicename"];
    if (serviceName.isEmpty())
        return EXIT_FAILURE;

    // Get the server port.
    mach_port_t serverPort;
    kern_return_t kr = bootstrap_look_up2(bootstrap_port, serviceName.utf8().data(), &serverPort, 0, 0);
    if (kr) {
        printf("bootstrap_look_up2 result: %x", kr);
        return 2;
    }

    String localization = commandLine["localization"];
    RetainPtr<CFStringRef> cfLocalization(AdoptCF, CFStringCreateWithCharacters(0, reinterpret_cast<const UniChar*>(localization.characters()), localization.length()));
    if (cfLocalization)
        WKSetDefaultLocalization(cfLocalization.get());

#if !SHOW_CRASH_REPORTER
    // Installs signal handlers that exit on a crash so that CrashReporter does not show up.
    signal(SIGILL, _exit);
    signal(SIGFPE, _exit);
    signal(SIGBUS, _exit);
    signal(SIGSEGV, _exit);
#endif

    InitWebCoreSystemInterface();
    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    // Initialize the shim.
    WebProcess::shared().initializeShim();

    // Create the connection.
    WebProcess::shared().initialize(serverPort, RunLoop::main());

    [pool drain];

     // Initialize AppKit.
    [NSApplication sharedApplication];

    // Installs autorelease pools on the current CFRunLoop which prevents memory from accumulating between user events.
    // FIXME: Remove when <rdar://problem/8929426> is fixed.
    [[NSApplication sharedApplication] _installAutoreleasePoolsOnCurrentThreadIfNecessary];

    WKAXRegisterRemoteApp();
    
    RunLoop::run();

    // FIXME: Do more cleanup here.

    return 0;
}

} // namespace WebKit

