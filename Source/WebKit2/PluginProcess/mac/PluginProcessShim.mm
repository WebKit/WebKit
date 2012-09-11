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

#import <wtf/Platform.h>
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

#ifndef __LP64__

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

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
    pluginProcessShimCallbacks.beginModal();
    ModalDialog(modalFilter, itemHit);
    pluginProcessShimCallbacks.endModal();
}

static DialogItemIndex shimAlert(SInt16 alertID, ModalFilterUPP modalFilter)
{
    pluginProcessShimCallbacks.beginModal();
    DialogItemIndex index = Alert(alertID, modalFilter);
    pluginProcessShimCallbacks.endModal();
    
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

#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

#endif

__attribute__((visibility("default")))
void WebKitPluginProcessShimInitialize(const PluginProcessShimCallbacks& callbacks)
{
    pluginProcessShimCallbacks = callbacks;
}

} // namespace WebKit
