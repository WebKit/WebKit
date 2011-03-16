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
#import "PluginProcessProxy.h"

#if ENABLE(PLUGIN_PROCESS)

#import "PluginProcessCreationParameters.h"
#import "WebKitSystemInterface.h"

namespace WebKit {
    
bool PluginProcessProxy::pluginNeedsExecutableHeap(const PluginInfoStore::Plugin& pluginInfo)
{
    static bool forceNonexecutableHeapForPlugins = [[NSUserDefaults standardUserDefaults] boolForKey:@"ForceNonexecutableHeapForPlugins"];
    if (forceNonexecutableHeapForPlugins)
        return false;
    
    if (pluginInfo.bundleIdentifier == "com.apple.QuickTime Plugin.plugin")
        return false;
    
    return true;
}

void PluginProcessProxy::platformInitializePluginProcess(PluginProcessCreationParameters& parameters)
{
#if USE(ACCELERATED_COMPOSITING) && HAVE(HOSTED_CORE_ANIMATION)
    parameters.parentProcessName = [[NSProcessInfo processInfo] processName];
    mach_port_t renderServerPort = WKInitializeRenderServer();
    if (renderServerPort != MACH_PORT_NULL)
        parameters.acceleratedCompositingPort = CoreIPC::MachPort(renderServerPort, MACH_MSG_TYPE_COPY_SEND);
#endif
}    

} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
