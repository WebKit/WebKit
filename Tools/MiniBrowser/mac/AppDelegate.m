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

#import "WK1BrowserWindowController.h"
#import "WK2BrowserWindowController.h"

#import <WebKit2/WebKit2.h>
#import <WebKit2/WKStringCF.h>
#import <WebKit2/WKURLCF.h>

static NSString *defaultURL = @"http://www.webkit.org/";

enum {
    WebKit1NewWindowTag = 1,
    WebKit2NewWindowTag = 2
};

@implementation BrowserAppDelegate

- (id)init
{
    self = [super init];
    if (self) {
#if WK_API_ENABLED
        NSURL *url = [NSURL fileURLWithPath:[[NSBundle mainBundle] pathForAuxiliaryExecutable:@"MiniBrowser.wkbundle"]];
        _processGroup = [[WKProcessGroup alloc] initWithInjectedBundleURL:url];
        _browsingContextGroup = [[WKBrowsingContextGroup alloc] initWithIdentifier:@"MiniBrowser"];
#endif
        _browserWindows = [[NSMutableSet alloc] init];
    }

    return self;
}

- (IBAction)newWindow:(id)sender
{
    BrowserWindowController *controller = nil;
    
    if (![sender respondsToSelector:@selector(tag)] || [sender tag] == WebKit1NewWindowTag)
        controller = [[WK1BrowserWindowController alloc] initWithWindowNibName:@"BrowserWindow"];
#if WK_API_ENABLED
    else if ([sender tag] == WebKit2NewWindowTag)
        controller = [[WK2BrowserWindowController alloc] initWithProcessGroup:_processGroup browsingContextGroup:_browsingContextGroup];
#endif

    if (!controller)
        return;

    [[controller window] makeKeyAndOrderFront:sender];
    [_browserWindows addObject:[controller window]];
    
    [controller loadURLString:defaultURL];
}

- (void)browserWindowWillClose:(NSWindow *)window
{
    [_browserWindows removeObject:window];
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    [self newWindow:self];
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    for (NSWindow* window in _browserWindows) {
        id delegate = [window delegate];
        assert([delegate isKindOfClass:[BrowserWindowController class]]);
        BrowserWindowController *controller = (BrowserWindowController *)delegate;
        [controller applicationTerminating];
    }

#if WK_API_ENABLED
    [_processGroup release];
    _processGroup = nil;

    [_browsingContextGroup release];
    _browsingContextGroup = nil;
#endif
}

- (BrowserWindowController *)frontmostBrowserWindowController
{
    for (NSWindow* window in [NSApp windows]) {
        id delegate = [window delegate];

        if (![delegate isKindOfClass:[BrowserWindowController class]])
            continue;

        BrowserWindowController *controller = (BrowserWindowController *)delegate;
        assert([_browserWindows containsObject:[controller window]]);
        return controller;
    }

    return 0;
}

- (IBAction)openDocument:(id)sender
{
    BrowserWindowController *browserWindowController = [self frontmostBrowserWindowController];

    if (browserWindowController) {
        NSOpenPanel *openPanel = [[NSOpenPanel openPanel] retain];
        [openPanel beginSheetModalForWindow:browserWindowController.window completionHandler:^(NSInteger result) {
            if (result != NSOKButton)
                return;

            NSURL *url = [openPanel.URLs objectAtIndex:0];
            [browserWindowController loadURLString:[url absoluteString]];
        }];
        return;
    }

#if WK_API_ENABLED
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel beginWithCompletionHandler:^(NSInteger result) {
        if (result != NSOKButton)
            return;

        // FIXME: add a way to open in WK1 also.
        BrowserWindowController *newBrowserWindowController = [[WK2BrowserWindowController alloc] initWithProcessGroup:_processGroup browsingContextGroup:_browsingContextGroup];
        [newBrowserWindowController.window makeKeyAndOrderFront:self];

        NSURL *url = [openPanel.URLs objectAtIndex:0];
        [newBrowserWindowController loadURLString:[url absoluteString]];
    }];
#endif // WK_API_ENABLED
}

@end
