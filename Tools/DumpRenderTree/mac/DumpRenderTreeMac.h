/*
 * Copyright (C) 2007-2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
#include <wtf/Forward.h>
#endif

#if PLATFORM(IOS_FAMILY) && defined(__OBJC__)
#import <UIKit/UIKit.h>
#endif

OBJC_CLASS DefaultPolicyDelegate;
OBJC_CLASS DumpRenderTreeDraggingInfo;
OBJC_CLASS NavigationController;
OBJC_CLASS PolicyDelegate;
OBJC_CLASS StorageTrackerDelegate;
OBJC_CLASS WebFrame;
OBJC_CLASS WebScriptWorld;
OBJC_CLASS WebView;

extern CFMutableArrayRef openWindowsRef;
extern WebFrame* mainFrame;
extern WebFrame* topLoadingFrame;
#ifdef __cplusplus
extern RetainPtr<CFMutableSetRef> disallowedURLs;
extern RetainPtr<DumpRenderTreeDraggingInfo> draggingInfo;
extern RetainPtr<NavigationController> gNavigationController;
extern RetainPtr<PolicyDelegate> policyDelegate;
extern RetainPtr<DefaultPolicyDelegate> defaultPolicyDelegate;

#if PLATFORM(IOS_FAMILY)
OBJC_CLASS UIWindow;
extern RetainPtr<UIWindow> mainWindow;
#else
OBJC_CLASS NSWindow;
extern RetainPtr<NSWindow> mainWindow;
#endif

void setWaitToDumpWatchdog(RetainPtr<CFRunLoopTimerRef>&&);
#endif // __cplusplus

bool shouldSetWaitToDumpWatchdog(void);

#ifdef __cplusplus
RetainPtr<WebView> createWebViewAndOffscreenWindow(void);
#endif

void setPersistentUserStyleSheetLocation(CFStringRef);

unsigned worldIDForWorld(WebScriptWorld *);


#if PLATFORM(IOS_FAMILY) && defined(__OBJC__)
@interface DumpRenderTree : UIApplication {
    BOOL _hasFlushedWebThreadRunQueue;
    UIBackgroundTaskIdentifier backgroundTaskIdentifier;
}

- (void)_waitForWebThread;
@end

#ifdef __cplusplus
@class DumpRenderTreeBrowserView;
extern RetainPtr<DumpRenderTreeBrowserView> gWebBrowserView;
#endif
#endif

int DumpRenderTreeMain(int, const char *[]);
