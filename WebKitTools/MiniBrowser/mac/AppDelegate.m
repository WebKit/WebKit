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

#import "AppDelegate.h"

#import "BrowserWindowController.h"
#import "BrowserStatisticsWindowController.h"

#import <WebKit2/WKStringCF.h>
#import <WebKit2/WKContextPrivate.h>

static NSString *defaultURL = @"file:///Users/andersca/Desktop/t.html";

@implementation BrowserAppDelegate

void _didRecieveMessageFromInjectedBundle(WKContextRef context, WKStringRef message, const void *clientInfo)
{
    CFStringRef cfMessage = WKStringCopyCFString(0, message);
    LOG(@"ContextInjectedBundleClient - didRecieveMessage - message: %@", cfMessage);
    CFRelease(cfMessage);

    WKStringRef newMessage = WKStringCreateWithCFString(CFSTR("Roger that!"));
    WKContextPostMessageToInjectedBundle(context, newMessage);
    WKStringRelease(newMessage);
}

- (id)init
{
    self = [super init];
    if (self) {
        if ([NSEvent modifierFlags] & NSShiftKeyMask)
            currentProcessModel = kProcessModelSharedSecondaryThread;
        else
            currentProcessModel = kProcessModelSharedSecondaryProcess;

        WKContextRef threadContext = WKContextGetSharedThreadContext();
        threadPageNamespace = WKPageNamespaceCreate(threadContext);
        WKContextRelease(threadContext);

        CFStringRef bundlePathCF = (CFStringRef)[[NSBundle mainBundle] pathForAuxiliaryExecutable:@"WebBundle.bundle"];
        WKStringRef bundlePath = WKStringCreateWithCFString(bundlePathCF);

        WKContextRef processContext = WKContextCreateWithInjectedBundlePath(bundlePath);
        
        WKContextInjectedBundleClient bundleClient = {
            0,      /* version */
            0,      /* clientInfo */
            _didRecieveMessageFromInjectedBundle
        };
        WKContextSetInjectedBundleClient(processContext, &bundleClient);
        
        processPageNamespace = WKPageNamespaceCreate(processContext);
        WKContextRelease(processContext);

        WKStringRelease(bundlePath);
    }

    return self;
}

- (IBAction)newWindow:(id)sender
{
    BrowserWindowController *controller = [[BrowserWindowController alloc] initWithPageNamespace:[self getCurrentPageNamespace]];
    [[controller window] makeKeyAndOrderFront:sender];
    
    [controller loadURLString:defaultURL];
}

- (WKPageNamespaceRef)getCurrentPageNamespace
{
    return (currentProcessModel == kProcessModelSharedSecondaryThread) ? threadPageNamespace : processPageNamespace;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    if ([menuItem action] == @selector(setSharedProcessProcessModel:))
        [menuItem setState:currentProcessModel == kProcessModelSharedSecondaryProcess ? NSOnState : NSOffState];
    else if ([menuItem action] == @selector(setSharedThreadProcessModel:))
        [menuItem setState:currentProcessModel == kProcessModelSharedSecondaryThread ? NSOnState : NSOffState];
    return YES;
}        

- (void)_setProcessModel:(ProcessModel)processModel
{
    if (processModel == currentProcessModel)
        return;
 
    currentProcessModel = processModel;
}

- (IBAction)setSharedProcessProcessModel:(id)sender
{
    [self _setProcessModel:kProcessModelSharedSecondaryProcess];
}

- (IBAction)setSharedThreadProcessModel:(id)sender
{
    [self _setProcessModel:kProcessModelSharedSecondaryThread];
}

- (IBAction)showStatisticsWindow:(id)sender
{
    static BrowserStatisticsWindowController* windowController;
    if (!windowController)
        windowController = [[BrowserStatisticsWindowController alloc] initWithThreadedWKContextRef:WKPageNamespaceGetContext(threadPageNamespace) 
                                                                               processWKContextRef:WKPageNamespaceGetContext(processPageNamespace)];

    [[windowController window] makeKeyAndOrderFront:self];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self newWindow:self];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    NSArray* windows = [NSApp windows];
    for (NSWindow* window in windows) {
        id delegate = [window delegate];
        if ([delegate isKindOfClass:[BrowserWindowController class]]) {
            BrowserWindowController *controller = (BrowserWindowController *)delegate;
            [controller applicationTerminating];
        }
    }
    
    WKPageNamespaceRelease(threadPageNamespace);
    threadPageNamespace = 0;

    WKPageNamespaceRelease(processPageNamespace);
    processPageNamespace = 0;
}

@end
