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
#import "PluginProcessMain.h"

#if ENABLE(PLUGIN_PROCESS)

#import "CommandLine.h"
#import "PluginProcess.h"
#import "RunLoop.h"
#import <WebKitSystemInterface.h>
#import <runtime/InitializeThreading.h>
#import <servers/bootstrap.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

// FIXME: We should be doing this another way.
extern "C" kern_return_t bootstrap_look_up2(mach_port_t, const name_t, mach_port_t*, pid_t, uint64_t);

#define SHOW_CRASH_REPORTER 1

namespace WebKit {

// FIXME: There is much code here that is duplicated in WebProcessMainMac.mm, we should add a shared base class where
// we can put everything.

int PluginProcessMain(const CommandLine& commandLine)
{
    // Unset DYLD_INSERT_LIBRARIES. We don't want our plug-in process shim to be loaded 
    // by any child processes that the plug-in may launch.
    unsetenv("DYLD_INSERT_LIBRARIES");

    String serviceName = commandLine["servicename"];
    if (serviceName.isEmpty())
        return EXIT_FAILURE;
    
    // Get the server port.
    mach_port_t serverPort;
    kern_return_t kr = bootstrap_look_up2(bootstrap_port, serviceName.utf8().data(), &serverPort, 0, 0);
    if (kr) {
        printf("bootstrap_look_up2 result: %x", kr);
        return EXIT_FAILURE;
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

    // FIXME: It would be better to proxy set cursor calls over to the UI process instead of
    // allowing plug-ins to change the mouse cursor at any time.
    WKEnableSettingCursorWhenInBackground();

    JSC::initializeThreading();
    WTF::initializeMainThread();
    RunLoop::initializeMainRunLoop();

    // Initialize the shim.
    PluginProcess::shared().initializeShim();
    
    // Initialize the plug-in process connection.
    PluginProcess::shared().initialize(serverPort, RunLoop::main());

    [NSApplication sharedApplication];

    RunLoop::run();
    
    return 0;
}

}

#endif // ENABLE(PLUGIN_PROCESS)
