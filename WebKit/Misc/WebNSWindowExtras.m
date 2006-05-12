/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebNSWindowExtras.h"

#import "WebKitLogging.h"
#import <JavaScriptCore/Assertions.h>
#import <objc/objc-runtime.h>

#define DISPLAY_REFRESH_INTERVAL (1.0 / 60.0)

static BOOL throttlingWindowDisplay;
static CFMutableDictionaryRef windowDisplayInfoDictionary;
static IMP oldNSWindowPostWindowNeedsDisplayIMP;
static IMP oldNSWindowCloseIMP;
static IMP oldNSWindowFlushWindowIMP;

typedef struct {
    NSWindow *window;
    CFTimeInterval lastFlushTime;
    NSTimer *displayTimer;
} WindowDisplayInfo;

@interface NSWindow (WebExtrasInternal)
static IMP swizzleInstanceMethod(Class class, SEL selector, IMP newImplementation);
static void replacementPostWindowNeedsDisplay(id self, SEL cmd);
static void replacementClose(id self, SEL cmd);
static void replacementFlushWindow(id self, SEL cmd);
static WindowDisplayInfo *getWindowDisplayInfo(NSWindow *window);
static BOOL requestWindowDisplay(NSWindow *window);
static void cancelPendingWindowDisplay(WindowDisplayInfo *displayInfo);
- (void)_webkit_doPendingPostWindowNeedsDisplay:(NSTimer *)timer;
@end

@interface NSWindow (AppKitSecretsIKnow)
- (void)_postWindowNeedsDisplay;
@end

@implementation NSWindow (WebExtras)

+ (void)_webkit_enableWindowDisplayThrottle
{
    if (throttlingWindowDisplay)
        return;
        
    windowDisplayInfoDictionary = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
    ASSERT(windowDisplayInfoDictionary);

    // Override -[NSWindow _postWindowNeedsDisplay]
    ASSERT(!oldNSWindowPostWindowNeedsDisplayIMP);
    oldNSWindowPostWindowNeedsDisplayIMP = swizzleInstanceMethod(self, @selector(_postWindowNeedsDisplay), (IMP)replacementPostWindowNeedsDisplay);
    ASSERT(oldNSWindowPostWindowNeedsDisplayIMP);
    
    // Override -[NSWindow close]    
    ASSERT(!oldNSWindowCloseIMP);
    oldNSWindowCloseIMP = swizzleInstanceMethod(self, @selector(close), (IMP)replacementClose);
    ASSERT(oldNSWindowCloseIMP);

    // Override -[NSWindow flushWindow]    
    ASSERT(!oldNSWindowFlushWindowIMP);
    oldNSWindowFlushWindowIMP = swizzleInstanceMethod(self, @selector(flushWindow), (IMP)replacementFlushWindow);
    ASSERT(oldNSWindowFlushWindowIMP);
    
//    NSLog(@"Throttling window display to %.3f times per second", 1.0 / DISPLAY_REFRESH_INTERVAL);
    
    throttlingWindowDisplay = YES;
}

static void disableWindowDisplayThrottleApplierFunction(const void *key, const void *value, void *context)
{
    WindowDisplayInfo *displayInfo = (WindowDisplayInfo *)value;
    
    // Display immediately
    cancelPendingWindowDisplay(displayInfo);
    [displayInfo->window _postWindowNeedsDisplay];
    
    free(displayInfo);
}

+ (void)_webkit_disableWindowDisplayThrottle
{
    if (!throttlingWindowDisplay)
        return;
    
    //
    // Restore NSWindow method implementations first.  When window display throttling is disabled, we display any windows
    // with pending displays.  If the window re-displays during our call to -_postWindowNeedsDisplay, we want those displays to
    // not be throttled.
    //
    
    // Restore -[NSWindow _postWindowNeedsDisplay]
    ASSERT(oldNSWindowPostWindowNeedsDisplayIMP);
    swizzleInstanceMethod(self, @selector(_postWindowNeedsDisplay), oldNSWindowPostWindowNeedsDisplayIMP);
    oldNSWindowPostWindowNeedsDisplayIMP = NULL;
    
    // Restore -[NSWindow close]    
    ASSERT(oldNSWindowCloseIMP);
    swizzleInstanceMethod(self, @selector(close), oldNSWindowCloseIMP);
    oldNSWindowCloseIMP = NULL;

    // Restore -[NSWindow flushWindow]    
    ASSERT(oldNSWindowFlushWindowIMP);
    swizzleInstanceMethod(self, @selector(flushWindow), oldNSWindowFlushWindowIMP);
    oldNSWindowFlushWindowIMP = NULL;

    CFDictionaryApplyFunction(windowDisplayInfoDictionary, disableWindowDisplayThrottleApplierFunction, NULL);
    CFRelease(windowDisplayInfoDictionary);
    windowDisplayInfoDictionary = NULL;
    
    throttlingWindowDisplay = NO;
}

- (void)centerOverMainWindow
{
    NSRect frameToCenterOver;
    if ([NSApp mainWindow]) {
        frameToCenterOver = [[NSApp mainWindow] frame];
    } else {
        frameToCenterOver = [[NSScreen mainScreen] visibleFrame];
    }
    
    NSSize size = [self frame].size;
    NSPoint origin;
    origin.y = NSMaxY(frameToCenterOver)
        - (frameToCenterOver.size.height - size.height) / 3
        - size.height;
    origin.x = frameToCenterOver.origin.x
        + (frameToCenterOver.size.width - size.width) / 2;
    [self setFrameOrigin:origin];
}

@end

@implementation NSWindow (WebExtrasInternal)

// Returns the old method implementation
static IMP swizzleInstanceMethod(Class class, SEL selector, IMP newImplementation)
{
    Method method = class_getInstanceMethod(class, selector);
    ASSERT(method);
    IMP oldIMP;
#if OBJC_API_VERSION > 0
    oldIMP = method_setImplementation(method, newImplementation);
#else
    oldIMP = method->method_imp;
    method->method_imp = newImplementation;
#endif
    return oldIMP;
}

static void replacementPostWindowNeedsDisplay(id self, SEL cmd)
{
    ASSERT(throttlingWindowDisplay);

    // Do not call into -[NSWindow _postWindowNeedsDisplay] if requestWindowDisplay() returns NO.  In that case, requestWindowDisplay()
    // will schedule a timer to display at the appropriate time.
    if (requestWindowDisplay(self))
        oldNSWindowPostWindowNeedsDisplayIMP(self, cmd);
}

static void replacementClose(id self, SEL cmd)
{
    ASSERT(throttlingWindowDisplay);

    // Remove WindowDisplayInfo for this window
    WindowDisplayInfo *displayInfo = (WindowDisplayInfo *)CFDictionaryGetValue(windowDisplayInfoDictionary, self);
    if (displayInfo) {
        cancelPendingWindowDisplay(displayInfo);
        free(displayInfo);
        CFDictionaryRemoveValue(windowDisplayInfoDictionary, self);
    }
    
    oldNSWindowCloseIMP(self, cmd);
}

static void replacementFlushWindow(id self, SEL cmd)
{
    ASSERT(throttlingWindowDisplay);

    oldNSWindowFlushWindowIMP(self, cmd);
    getWindowDisplayInfo(self)->lastFlushTime = CFAbsoluteTimeGetCurrent();
}

static WindowDisplayInfo *getWindowDisplayInfo(NSWindow *window)
{
    ASSERT(throttlingWindowDisplay);
    ASSERT(windowDisplayInfoDictionary);
    
    // Get the WindowDisplayInfo for this window, or create it if it does not exist
    WindowDisplayInfo *displayInfo = (WindowDisplayInfo *)CFDictionaryGetValue(windowDisplayInfoDictionary, window);
    if (!displayInfo) {
        displayInfo = (WindowDisplayInfo *)malloc(sizeof(WindowDisplayInfo));
        displayInfo->window = window;
        displayInfo->lastFlushTime = 0;
        displayInfo->displayTimer = nil;
        CFDictionarySetValue(windowDisplayInfoDictionary, window, displayInfo);
    }
    
    return displayInfo;
}

static BOOL requestWindowDisplay(NSWindow *window)
{
    ASSERT(throttlingWindowDisplay);

    // Defer display if there is already a pending display
    WindowDisplayInfo *displayInfo = getWindowDisplayInfo(window);
    if (displayInfo->displayTimer)
        return NO;
        
    // Defer display if it hasn't been at least DISPLAY_REFRESH_INTERVAL seconds since the last display
    CFTimeInterval now = CFAbsoluteTimeGetCurrent();
    CFTimeInterval timeSinceLastDisplay = now - displayInfo->lastFlushTime;
    if (timeSinceLastDisplay < DISPLAY_REFRESH_INTERVAL) {
        // Redisplay soon -- if we redisplay too quickly, we'll block due to pending CG coalesced updates
        displayInfo->displayTimer = [[NSTimer timerWithTimeInterval:(DISPLAY_REFRESH_INTERVAL - timeSinceLastDisplay)
                                                             target:window
                                                           selector:@selector(_webkit_doPendingPostWindowNeedsDisplay:)
                                                           userInfo:nil
                                                            repeats:NO] retain];
        
        // The NSWindow autodisplay mechanism is documented to only work for NSDefaultRunLoopMode, NSEventTrackingRunLoopMode, and NSModalPanelRunLoopMode
        NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
        [runLoop addTimer:displayInfo->displayTimer forMode:NSDefaultRunLoopMode];
        [runLoop addTimer:displayInfo->displayTimer forMode:NSEventTrackingRunLoopMode];
        [runLoop addTimer:displayInfo->displayTimer forMode:NSModalPanelRunLoopMode];
        
        return NO;
    }

    return YES;
}

static void cancelPendingWindowDisplay(WindowDisplayInfo *displayInfo)
{
    ASSERT(throttlingWindowDisplay);

    if (!displayInfo->displayTimer)
        return;

    [displayInfo->displayTimer invalidate];
    [displayInfo->displayTimer release];
    displayInfo->displayTimer = nil;
}

- (void)_webkit_doPendingPostWindowNeedsDisplay:(NSTimer *)timer
{
    WindowDisplayInfo *displayInfo = getWindowDisplayInfo(self);
    ASSERT(timer == displayInfo->displayTimer);
    ASSERT(throttlingWindowDisplay);

    // -_postWindowNeedsDisplay will short-circuit if there is a displayTimer, so invalidate it first.
    cancelPendingWindowDisplay(displayInfo);

    [self _postWindowNeedsDisplay];
}

@end
