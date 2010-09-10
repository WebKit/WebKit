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

#import "WebProcessMain.h"

#import "CommandLine.h"
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

#if ENABLE(WEB_PROCESS_SANDBOX)
#import <sandbox.h>
#endif

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_look_up2(mach_port_t, const name_t, mach_port_t*, pid_t, uint64_t);

#define SHOW_CRASH_REPORTER 1

using namespace WebCore;

namespace WebKit {

int WebProcessMain(CommandLine* commandLine)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

#if ENABLE(WEB_PROCESS_SANDBOX)
    char* errorBuf;
    const char* frameworkPath = [[[[NSBundle bundleForClass:[WKView class]] bundlePath] stringByDeletingLastPathComponent] UTF8String];
    const char* profilePath = [[[NSBundle mainBundle] pathForResource:@"com.apple.WebProcess" ofType:@"sb"] UTF8String];
    const char* const sandboxParam[] = { "webkit2_framework_path", frameworkPath, NULL };

    if (sandbox_init_with_parameters(profilePath, SANDBOX_NAMED_EXTERNAL, sandboxParam, &errorBuf)) {
        fprintf(stderr, "WebProcess: couldn't initialize sandbox profile [%s] with framework path [%s]: %s\n", profilePath, frameworkPath, errorBuf);
        exit(EX_NOPERM);
    }
#endif

    String serviceName = (*commandLine)["servicename"];
    if (serviceName.isEmpty())
        return EXIT_FAILURE;

    // Get the server port.
    mach_port_t serverPort;
    kern_return_t kr = bootstrap_look_up2(bootstrap_port, serviceName.utf8().data(), &serverPort, 0, 0);
    if (kr) {
        printf("bootstrap_look_up2 result: %x", kr);
        return 2;
    }

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

    // Set the visible application name.
    String parentProcessName = (*commandLine)["parentprocessname"];
    if (!parentProcessName.isNull()) {
        // FIXME: Localization!
        NSString *applicationName = [NSString stringWithFormat:@"%@ Web Content", (NSString *)parentProcessName];
        WKSetVisibleApplicationName((CFStringRef)applicationName);
    }
    
    // Create the connection.
    WebProcess::shared().initialize(serverPort, RunLoop::main());
    
    [pool drain];

     // Initialize AppKit.
    [NSApplication sharedApplication];

    RunLoop::run();

    // FIXME: Do more cleanup here.

    return 0;
}

} // namespace WebKit

