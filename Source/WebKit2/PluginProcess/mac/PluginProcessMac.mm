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

#import "config.h"
#import "PluginProcess.h"

#if ENABLE(PLUGIN_PROCESS)

// FIXME (WebKit2) <rdar://problem/8728860> WebKit2 needs to be localized
#define UI_STRING(__str, __desc) [NSString stringWithUTF8String:__str]

#import "NetscapePlugin.h"
#import "PluginProcessShim.h"
#import "PluginProcessCreationParameters.h"
#import <WebKitSystemInterface.h>
#import <dlfcn.h>

namespace WebKit {

static bool isUserbreakSet = false;

static void initShouldCallRealDebugger()
{
    char* var = getenv("USERBREAK");
    
    if (var)
        isUserbreakSet = atoi(var);
}

static bool shouldCallRealDebugger()
{
    static pthread_once_t shouldCallRealDebuggerOnce = PTHREAD_ONCE_INIT;
    pthread_once(&shouldCallRealDebuggerOnce, initShouldCallRealDebugger);
    
    return isUserbreakSet;
}

static bool isWindowActive(WindowRef windowRef, bool& result)
{
#ifndef NP_NO_CARBON
    if (NetscapePlugin* plugin = NetscapePlugin::netscapePluginFromWindow(windowRef)) {
        result = plugin->isWindowActive();
        return true;
    }
#endif
    return false;
}

static UInt32 getCurrentEventButtonState()
{
#ifndef NP_NO_CARBON
    return NetscapePlugin::buttonState();
#else
    ASSERT_NOT_REACHED();
    return 0;
#endif
}
    
static void cocoaWindowShown(NSWindow *)
{
    // FIXME: Implement.
}

static void cocoaWindowHidden(NSWindow *)
{
    // FIXME: Implement.
}

static void carbonWindowShown(WindowRef)
{
    // FIXME: Implement.
}

static void carbonWindowHidden(WindowRef)
{
}

static void setModal(bool)
{
    // FIXME: Implement.
}

void PluginProcess::initializeShim()
{
    const PluginProcessShimCallbacks callbacks = {
        shouldCallRealDebugger,
        isWindowActive,
        getCurrentEventButtonState,
        cocoaWindowShown,
        cocoaWindowHidden,
        carbonWindowShown,
        carbonWindowHidden,
        setModal
    };

    PluginProcessShimInitializeFunc initFunc = reinterpret_cast<PluginProcessShimInitializeFunc>(dlsym(RTLD_DEFAULT, "WebKitPluginProcessShimInitialize"));
    initFunc(callbacks);
}

void PluginProcess::platformInitialize(const PluginProcessCreationParameters& parameters)
{
    m_compositingRenderServerPort = parameters.acceleratedCompositingPort.port();

    NSString *applicationName = [NSString stringWithFormat:UI_STRING("%@ (%@ Internet plug-in)",
                                                                     "visible name of the plug-in host process. The first argument is the plug-in name "
                                                                     "and the second argument is the application name."),
                                 [[(NSString *)parameters.pluginPath lastPathComponent] stringByDeletingPathExtension], 
                                 (NSString *)parameters.parentProcessName];
    
    WKSetVisibleApplicationName((CFStringRef)applicationName);
}

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
