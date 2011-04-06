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

@interface WKPlaceholderModalWindow : NSWindow 
@end

@implementation WKPlaceholderModalWindow

// Prevent NSApp from calling requestUserAttention: when the window is shown 
// modally, even if the app is inactive. See 6823049.
- (BOOL)_wantsUserAttention
{
    return NO;   
}

@end

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

bool PluginProcessProxy::getPluginProcessSerialNumber(ProcessSerialNumber& pluginProcessSerialNumber)
{
    pid_t pluginProcessPID = m_processLauncher->processIdentifier();
    return GetProcessForPID(pluginProcessPID, &pluginProcessSerialNumber) == noErr;
}

void PluginProcessProxy::makePluginProcessTheFrontProcess()
{
    ProcessSerialNumber pluginProcessSerialNumber;
    if (!getPluginProcessSerialNumber(pluginProcessSerialNumber))
        return;

    SetFrontProcess(&pluginProcessSerialNumber);
}

void PluginProcessProxy::makeUIProcessTheFrontProcess()
{
    ProcessSerialNumber processSerialNumber;
    GetCurrentProcess(&processSerialNumber);
    SetFrontProcess(&processSerialNumber);            
}

void PluginProcessProxy::setFullscreenWindowIsShowing(bool fullscreenWindowIsShowing)
{
    if (m_fullscreenWindowIsShowing == fullscreenWindowIsShowing)
        return;

    m_fullscreenWindowIsShowing = fullscreenWindowIsShowing;
    if (m_fullscreenWindowIsShowing)
        enterFullscreen();
    else
        exitFullscreen();
}

void PluginProcessProxy::enterFullscreen()
{
    // Get the current presentation options.
    m_preFullscreenAppPresentationOptions = [NSApp presentationOptions];

    // Figure out which presentation options to use.
    unsigned presentationOptions = m_preFullscreenAppPresentationOptions & ~(NSApplicationPresentationAutoHideDock | NSApplicationPresentationAutoHideMenuBar);
    presentationOptions |= NSApplicationPresentationHideDock | NSApplicationPresentationHideMenuBar;

    [NSApp setPresentationOptions:presentationOptions];
    makePluginProcessTheFrontProcess();
}

void PluginProcessProxy::exitFullscreen()
{
    // If the plug-in host is the current application then we should bring ourselves to the front when it exits full-screen mode.
    ProcessSerialNumber frontProcessSerialNumber;
    GetFrontProcess(&frontProcessSerialNumber);

    // The UI process must be the front process in order to change the presentation mode.
    makeUIProcessTheFrontProcess();
    [NSApp setPresentationOptions:m_preFullscreenAppPresentationOptions];

    ProcessSerialNumber pluginProcessSerialNumber;
    if (!getPluginProcessSerialNumber(pluginProcessSerialNumber))
        return;

    // If the plug-in process was not the front process, switch back to the previous front process.
    // (Otherwise we'll keep the UI process as the front process).
    Boolean isPluginProcessFrontProcess;
    SameProcess(&frontProcessSerialNumber, &pluginProcessSerialNumber, &isPluginProcessFrontProcess);
    if (!isPluginProcessFrontProcess)
        SetFrontProcess(&frontProcessSerialNumber);
}

void PluginProcessProxy::setModalWindowIsShowing(bool modalWindowIsShowing)
{
    if (modalWindowIsShowing == m_modalWindowIsShowing) 
        return;
    
    m_modalWindowIsShowing = modalWindowIsShowing;
    
    if (m_modalWindowIsShowing)
        beginModal();
    else
        endModal();
}

void PluginProcessProxy::beginModal()
{
    ASSERT(!m_placeholderWindow);
    ASSERT(!m_activationObserver);
    
    m_placeholderWindow.adoptNS([[WKPlaceholderModalWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1) styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES]);
    
    m_activationObserver = [[NSNotificationCenter defaultCenter] addObserverForName:NSApplicationWillBecomeActiveNotification object:NSApp queue:nil
                                                                         usingBlock:^(NSNotification *){ applicationDidBecomeActive(); }];
    
    [NSApp runModalForWindow:m_placeholderWindow.get()];
    
    [m_placeholderWindow.get() orderOut:nil];
    m_placeholderWindow = nullptr;
}

void PluginProcessProxy::endModal()
{
    ASSERT(m_placeholderWindow);
    ASSERT(m_activationObserver);
    
    [[NSNotificationCenter defaultCenter] removeObserver:m_activationObserver.get()];
    m_activationObserver = nullptr;
    
    [NSApp stopModal];

    makeUIProcessTheFrontProcess();
}
    
void PluginProcessProxy::applicationDidBecomeActive()
{
    makePluginProcessTheFrontProcess();
}


} // namespace WebKit

#endif // ENABLE(PLUGIN_PROCESS)
