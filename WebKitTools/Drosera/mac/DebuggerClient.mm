/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2006, 2007 Vladimir Olexa (vladimir.olexa@gmail.com)
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

#import "config.h"
#import "DebuggerClient.h"

#import "DebuggerApplication.h"
#import "DebuggerDocument.h"
#import "ServerConnection.h"

#import <JavaScriptCore/JSContextRef.h>

static NSString *DebuggerConsoleToolbarItem = @"DebuggerConsoleToolbarItem";
static NSString *DebuggerContinueToolbarItem = @"DebuggerContinueToolbarItem";
static NSString *DebuggerPauseToolbarItem = @"DebuggerPauseToolbarItem";
static NSString *DebuggerStepIntoToolbarItem = @"DebuggerStepIntoToolbarItem";
static NSString *DebuggerStepOverToolbarItem = @"DebuggerStepOverToolbarItem";
static NSString *DebuggerStepOutToolbarItem = @"DebuggerStepOutToolbarItem";

@implementation DebuggerClient
+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return NO;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return NO;
}

#pragma mark -

- (id)initWithServerName:(NSString *)serverName;
{
    if ((self = [super init])) {
        server = [[ServerConnection alloc] initWithServerName:serverName];
        debuggerDocument = new DebuggerDocument(server);
    }

    return self;
}

- (void)dealloc
{
    delete debuggerDocument;
    [server release];
    [super dealloc];
}

#pragma mark -
#pragma mark Interface Actions

- (IBAction)pause:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "pause", 0, 0);
}

- (IBAction)resume:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "resume", 0, 0);
}

- (IBAction)stepInto:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "stepInto", 0, 0);
}

- (IBAction)stepOver:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "stepOver", 0, 0);
}

- (IBAction)stepOut:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "stepOut", 0, 0);
}

- (IBAction)showConsole:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "showConsoleWindow", 0, 0);
}

- (IBAction)closeCurrentFile:(id)sender
{
    DebuggerDocument::callGlobalFunction([[webView mainFrame] globalContext], "closeCurrentFile", 0, 0);
}

#pragma mark -
#pragma mark Window Controller Overrides

- (NSString *)windowNibName
{
    return @"Debugger";
}

- (void)windowDidLoad
{
    [super windowDidLoad];

    NSString *path = [[NSBundle mainBundle] pathForResource:@"debugger" ofType:@"html" inDirectory:nil];
    [[webView mainFrame] loadRequest:[[[NSURLRequest alloc] initWithURL:[NSURL fileURLWithPath:path]] autorelease]];

    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"debugger"];
    [toolbar setDelegate:self];
    [toolbar setAllowsUserCustomization:YES];
    [toolbar setAutosavesConfiguration:YES];
    [[self window] setToolbar:toolbar];
    [toolbar release];
}

- (void)windowWillClose:(NSNotification *)notification
{
    [[webView windowScriptObject] removeWebScriptKey:@"DebuggerDocument"];

    [server switchToServerNamed:nil];

    [self autorelease]; // DebuggerApplication expects us to release on close
}

#pragma mark -
#pragma mark Toolbar Delegate

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
    if ([itemIdentifier isEqualToString:DebuggerContinueToolbarItem]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        [item setLabel:@"Continue"];
        [item setPaletteLabel:@"Continue"];

        [item setToolTip:@"Continue script execution"];
        [item setImage:[NSImage imageNamed:@"continue"]];

        [item setTarget:self];
        [item setAction:@selector(resume:)];

        return [item autorelease];
    } else if ([itemIdentifier isEqualToString:DebuggerConsoleToolbarItem]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        [item setLabel:@"Console"];
        [item setPaletteLabel:@"Console"];

        [item setToolTip:@"Console"];
        [item setImage:[NSImage imageNamed:@"console"]];

        [item setTarget:self];
        [item setAction:@selector(showConsole:)];

        return [item autorelease];
    } else if ([itemIdentifier isEqualToString:DebuggerPauseToolbarItem]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        [item setLabel:@"Pause"];
        [item setPaletteLabel:@"Pause"];

        [item setToolTip:@"Pause script execution"];
        [item setImage:[NSImage imageNamed:@"pause"]];

        [item setTarget:self];
        [item setAction:@selector(pause:)];

        return [item autorelease];
    } else if ([itemIdentifier isEqualToString:DebuggerStepIntoToolbarItem]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        [item setLabel:@"Step Into"];
        [item setPaletteLabel:@"Step Into"];

        [item setToolTip:@"Step into function call"];
        [item setImage:[NSImage imageNamed:@"step"]];

        [item setTarget:self];
        [item setAction:@selector(stepInto:)];

        return [item autorelease];
    } else if ([itemIdentifier isEqualToString:DebuggerStepOverToolbarItem]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        [item setLabel:@"Step Over"];
        [item setPaletteLabel:@"Step Over"];

        [item setToolTip:@"Step over function call"];
        [item setImage:[NSImage imageNamed:@"stepOver"]];

        [item setTarget:self];
        [item setAction:@selector(stepOver:)];

        return [item autorelease];
    } else if ([itemIdentifier isEqualToString:DebuggerStepOutToolbarItem]) {
        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

        [item setLabel:@"Step Out"];
        [item setPaletteLabel:@"Step Over"];

        [item setToolTip:@"Step out of current function"];
        [item setImage:[NSImage imageNamed:@"stepOut"]];

        [item setTarget:self];
        [item setAction:@selector(stepOut:)];

        return [item autorelease];
    }

    return nil;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar
{
    return [NSArray arrayWithObjects:DebuggerContinueToolbarItem, DebuggerPauseToolbarItem,
        NSToolbarSeparatorItemIdentifier, DebuggerStepIntoToolbarItem, DebuggerStepOutToolbarItem,
        DebuggerStepOverToolbarItem, NSToolbarFlexibleSpaceItemIdentifier, DebuggerConsoleToolbarItem, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
    return [NSArray arrayWithObjects:DebuggerConsoleToolbarItem, DebuggerContinueToolbarItem, DebuggerPauseToolbarItem,
        DebuggerStepIntoToolbarItem, DebuggerStepOutToolbarItem, DebuggerStepOverToolbarItem, NSToolbarCustomizeToolbarItemIdentifier,
        NSToolbarFlexibleSpaceItemIdentifier, NSToolbarSpaceItemIdentifier, NSToolbarSeparatorItemIdentifier, nil];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)interfaceItem
{
    SEL action = [interfaceItem action];
    if (action == @selector(pause:)) {
        if (!webViewLoaded)
            return NO;

        return !debuggerDocument->isPaused([[webView mainFrame] globalContext]);
    }
    if (action == @selector(resume:) ||
        action == @selector(stepOver:) ||
        action == @selector(stepOut:) ||
        action == @selector(stepInto:)) {
        if (!webViewLoaded)
            return YES;

        return debuggerDocument->isPaused([[webView mainFrame] globalContext]);
    }
    return YES;
}

#pragma mark -
#pragma mark WebView UI Delegate

- (WebView *)webView:(WebView *)sender createWebViewWithRequest:(NSURLRequest *)request
{
    WebView *newWebView = [[WebView alloc] initWithFrame:NSZeroRect frameName:nil groupName:nil];
    [newWebView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [newWebView setUIDelegate:self];
    [newWebView setPolicyDelegate:self];
    [newWebView setFrameLoadDelegate:self];
    if (request)
        [[newWebView mainFrame] loadRequest:request];

    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSZeroRect styleMask:(NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask | NSUnifiedTitleAndToolbarWindowMask) backing:NSBackingStoreBuffered defer:NO screen:[[webView window] screen]];
    [window setReleasedWhenClosed:YES];
    [newWebView setFrame:[[window contentView] frame]];
    [[window contentView] addSubview:newWebView];
    [newWebView release];

    return newWebView;
}

- (void)webViewShow:(WebView *)sender
{
    [[sender window] makeKeyAndOrderFront:sender];
}

- (BOOL)webViewAreToolbarsVisible:(WebView *)sender
{
    return [[[sender window] toolbar] isVisible];
}

- (void)webView:(WebView *)sender setToolbarsVisible:(BOOL)visible
{
    [[[sender window] toolbar] setVisible:visible];
}

- (void)webView:(WebView *)sender setResizable:(BOOL)resizable
{
    [[sender window] setShowsResizeIndicator:resizable];
    [[[sender window] standardWindowButton:NSWindowZoomButton] setEnabled:resizable];
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    NSRange range = [message rangeOfString:@"\t"];
    NSString *title = @"Alert";
    if (range.location != NSNotFound) {
        title = [message substringToIndex:range.location];
        message = [message substringFromIndex:(range.location + range.length)];
    }

    NSBeginInformationalAlertSheet(title, nil, nil, nil, [sender window], nil, NULL, NULL, NULL, message);
}

- (void)scriptConfirmSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(long *)contextInfo
{
    *contextInfo = returnCode;
}

- (BOOL)webView:(WebView *)sender runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame
{
    NSRange range = [message rangeOfString:@"\t"];
    NSString *title = @"Alert";
    if (range.location != NSNotFound) {
        title = [message substringToIndex:range.location];
        message = [message substringFromIndex:(range.location + range.length)];
    }

    long result = NSNotFound;
    NSBeginInformationalAlertSheet(title, nil, @"Cancel", nil, [sender window], self, @selector(scriptConfirmSheetDidEnd:returnCode:contextInfo:), NULL, &result, message);

    while (result == NSNotFound) {
        NSEvent *nextEvent = [[NSApplication sharedApplication] nextEventMatchingMask:NSAnyEventMask untilDate:[NSDate distantFuture] inMode:NSDefaultRunLoopMode dequeue:YES];
        [[NSApplication sharedApplication] sendEvent:nextEvent];
    }

    return result;
}

#pragma mark -
#pragma mark WebView Frame Load Delegate

- (void)webView:(WebView *)sender windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject
{
    // note: this is the Debuggers's own WebView, not the one being debugged
    JSContextRef context = [[webView mainFrame] globalContext];
    JSObjectRef globalObject = JSContextGetGlobalObject(context);
    
    debuggerDocument->windowScriptObjectAvailable(context, globalObject);
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    // note: this is the Debuggers's own WebView, not the one being debugged
    if ([[sender window] isEqual:[self window]]) {
        webViewLoaded = YES;
        [server setGlobalContext:[[webView mainFrame] globalContext]];
    }
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    // note: this is the Debuggers's own WebViews, not the one being debugged
    if ([frame isEqual:[sender mainFrame]]) {
        NSDictionary *info = [[(DebuggerApplication *)[[NSApplication sharedApplication] delegate] knownServers] objectForKey:[server currentServerName]];
        NSString *processName = [info objectForKey:WebScriptDebugServerProcessNameKey];
        if (info && [processName length]) {
            NSMutableString *newTitle = [[NSMutableString alloc] initWithString:processName];
            [newTitle appendString:@" - "];
            [newTitle appendString:title];
            [[sender window] setTitle:newTitle];
            [newTitle release];
        } else 
            [[sender window] setTitle:title];
    }
}

@end
