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

#import "PluginProcessShim.h"

#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <WebKitSystemInterface.h>
#import <stdio.h>
#import <objc/objc-runtime.h>

#define DYLD_INTERPOSE(_replacement,_replacee) \
    __attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
    __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };

namespace WebKit {

extern "C" void WebKitPluginProcessShimInitialize(const PluginProcessShimCallbacks& callbacks);

static PluginProcessShimCallbacks pluginProcessShimCallbacks;

static IMP NSApplication_RunModalForWindow;
static unsigned modalCount = 0;

static void beginModal()
{
    // Make sure to make ourselves the front process
    ProcessSerialNumber psn;
    GetCurrentProcess(&psn);
    SetFrontProcess(&psn);
    
    if (!modalCount++)
        pluginProcessShimCallbacks.setModal(true);
}

static void endModal()
{
    if (!--modalCount)
        pluginProcessShimCallbacks.setModal(false);
}    

static NSInteger shim_NSApplication_RunModalForWindow(id self, SEL _cmd, NSWindow* window)
{
    beginModal();
    NSInteger result = ((NSInteger (*)(id, SEL, NSWindow *))NSApplication_RunModalForWindow)(self, _cmd, window);
    endModal();

    return result;
}

#ifndef __LP64__
static void shimDebugger(void)
{
    if (!pluginProcessShimCallbacks.shouldCallRealDebugger())
        return;
    
    Debugger();
}

static UInt32 shimGetCurrentEventButtonState()
{
    return pluginProcessShimCallbacks.getCurrentEventButtonState();
}

static Boolean shimIsWindowActive(WindowRef window)
{
    bool result;
    if (pluginProcessShimCallbacks.isWindowActive(window, result))
        return result;
    
    return IsWindowActive(window);
}

static void shimModalDialog(ModalFilterUPP modalFilter, DialogItemIndex *itemHit)
{
    beginModal();
    ModalDialog(modalFilter, itemHit);
    endModal();
}

static DialogItemIndex shimAlert(SInt16 alertID, ModalFilterUPP modalFilter)
{
    beginModal();
    DialogItemIndex index = Alert(alertID, modalFilter);
    endModal();
    
    return index;
}

static void shimShowWindow(WindowRef window)
{
    pluginProcessShimCallbacks.carbonWindowShown(window);
    ShowWindow(window);
}

static void shimHideWindow(WindowRef window)
{
    pluginProcessShimCallbacks.carbonWindowHidden(window);
    HideWindow(window);
}

DYLD_INTERPOSE(shimDebugger, Debugger);
DYLD_INTERPOSE(shimGetCurrentEventButtonState, GetCurrentEventButtonState);
DYLD_INTERPOSE(shimIsWindowActive, IsWindowActive);
DYLD_INTERPOSE(shimModalDialog, ModalDialog);
DYLD_INTERPOSE(shimAlert, Alert);
DYLD_INTERPOSE(shimShowWindow, ShowWindow);
DYLD_INTERPOSE(shimHideWindow, HideWindow);

#endif

__attribute__((visibility("default")))
void WebKitPluginProcessShimInitialize(const PluginProcessShimCallbacks& callbacks)
{
    pluginProcessShimCallbacks = callbacks;

    // Override -[NSApplication runModalForWindow:]
    Method runModalForWindowMethod = class_getInstanceMethod(objc_getClass("NSApplication"), @selector(runModalForWindow:));
    NSApplication_RunModalForWindow = method_setImplementation(runModalForWindowMethod, reinterpret_cast<IMP>(shim_NSApplication_RunModalForWindow));

    NSNotificationCenter *defaultCenter = [NSNotificationCenter defaultCenter];

    // Track when any Cocoa window is about to be be shown.
    id orderOnScreenObserver = [defaultCenter addObserverForName:WKWindowWillOrderOnScreenNotification()
                                                          object:nil
                                                           queue:nil
                                                           usingBlock:^(NSNotification *notification) { pluginProcessShimCallbacks.cocoaWindowShown([notification object]); }];
    // Track when any cocoa window is about to be hidden.
    id orderOffScreenObserver = [defaultCenter addObserverForName:WKWindowWillOrderOffScreenNotification()
                                                           object:nil
                                                            queue:nil
                                                       usingBlock:^(NSNotification *notification) { pluginProcessShimCallbacks.cocoaWindowHidden([notification object]); }];

    // Leak the two observers so that they observe notifications for the lifetime of the process.
    CFRetain(orderOnScreenObserver);
    CFRetain(orderOffScreenObserver);
}

} // namespace WebKit
