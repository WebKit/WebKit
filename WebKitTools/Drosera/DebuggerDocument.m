/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "DebuggerDocument.h"

static NSString *DebuggerContinueToolbarItem = @"DebuggerContinueToolbarItem";
static NSString *DebuggerPauseToolbarItem = @"DebuggerPauseToolbarItem";
static NSString *DebuggerStepIntoToolbarItem = @"DebuggerStepIntoToolbarItem";

@implementation DebuggerDocument
+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return NO;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return NO;
}

#pragma mark -

- (id)initWithServerName:(NSString *)serverName
{
    if ((self = [super init]))
        [self switchToServerNamed:serverName];
    return self;
}

- (void)dealloc
{
    [server release];
    [currentServerName release];
    [super dealloc];
}

#pragma mark -
#pragma mark Stack & Variables

- (WebScriptCallFrame *)currentFrame
{
    return currentFrame;
}

- (NSString *)currentFrameFunctionName
{
    return [currentFrame functionName];
}

- (NSArray *)currentFunctionStack
{
    NSMutableArray *result = [[NSMutableArray alloc] init];
    WebScriptCallFrame *frame = currentFrame;
    while (frame) {
        if ([frame functionName])
            [result addObject:[frame functionName]];
        frame = [frame caller];
    }
    return [result autorelease];
}

#pragma mark -
#pragma mark Pause & Step

- (BOOL)isPaused
{
    return paused;
}

- (void)pause
{
    paused = YES;
    if ([[(NSDistantObject *)server connectionForProxy] isValid])
        [server pause];
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

- (void)resume
{
    paused = NO;
    if ([[(NSDistantObject *)server connectionForProxy] isValid])
        [server resume];
}

- (void)stepInto
{
    if ([[(NSDistantObject *)server connectionForProxy] isValid])
        [server step];
}

- (void)log:(NSString *)msg
{
    NSLog(@"%@", msg);
}

#pragma mark -
#pragma mark Interface Actions

- (IBAction)pause:(id)sender
{
    [[webView windowScriptObject] callWebScriptMethod:@"pause" withArguments:nil];
}

- (IBAction)resume:(id)sender
{
    [[webView windowScriptObject] callWebScriptMethod:@"resume" withArguments:nil];
}

- (IBAction)stepInto:(id)sender
{
    [[webView windowScriptObject] callWebScriptMethod:@"stepInto" withArguments:nil];
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

    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(applicationTerminating:) name:NSApplicationWillTerminateNotification object:nil];

    NSString *path = [[NSBundle bundleForClass:[self class]] pathForResource:@"debugger" ofType:@"html" inDirectory:nil];
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

    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillTerminateNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:WebScriptDebugServerQueryReplyNotification object:nil];
    [[NSDistributedNotificationCenter defaultCenter] removeObserver:self name:WebScriptDebugServerWillUnloadNotification object:nil];

    [self switchToServerNamed:nil];

    [self autorelease]; // DebuggerApplication expects us to release on close
}

#pragma mark -
#pragma mark Connection Handling

- (void)switchToServerNamed:(NSString *)name
{
    if (server) {
        [[NSNotificationCenter defaultCenter] removeObserver:self name:NSConnectionDidDieNotification object:[(NSDistantObject *)server connectionForProxy]];
        if ([[(NSDistantObject *)server connectionForProxy] isValid]) {
            [self resume];
            [server removeListener:self];
        }
    }

    id old = server;
    server = ([name length] ? [[NSConnection rootProxyForConnectionWithRegisteredName:name host:nil] retain] : nil);
    [old release];

    old = currentServerName;
    currentServerName = [name copy];
    [old release];

    if (server) {
        @try {
            [(NSDistantObject *)server setProtocolForProxy:@protocol(WebScriptDebugServer)];
            [server addListener:self];
        } @catch (NSException *exception) {
            [currentServerName release];
            currentServerName = nil;
            [server release];
            server = nil;
        }

        if (server)
            [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(serverConnectionDidDie:) name:NSConnectionDidDieNotification object:[(NSDistantObject *)server connectionForProxy]];  
    }
}

- (void)applicationTerminating:(NSNotification *)notifiction
{
    if (server && [[(NSDistantObject *)server connectionForProxy] isValid]) {
        [self switchToServerNamed:nil];
        // call the runloop for a while to make sure our removeListener: is sent to the server
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.25]];
    }
}

- (void)serverConnectionDidDie:(NSNotification *)notifiction
{
    [self switchToServerNamed:nil];
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
    }

    return nil;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar*)toolbar
{
    return [NSArray arrayWithObjects:DebuggerContinueToolbarItem, DebuggerPauseToolbarItem,
        NSToolbarSeparatorItemIdentifier, DebuggerStepIntoToolbarItem, nil];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar*)toolbar
{
    return [NSArray arrayWithObjects:DebuggerContinueToolbarItem, DebuggerPauseToolbarItem, DebuggerStepIntoToolbarItem,
        NSToolbarCustomizeToolbarItemIdentifier, NSToolbarFlexibleSpaceItemIdentifier,
        NSToolbarSpaceItemIdentifier, NSToolbarSeparatorItemIdentifier, nil];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)interfaceItem
{
    if ([interfaceItem action] == @selector(pause:))
        return ![self isPaused];
    if ([interfaceItem action] == @selector(resume:))
        return [self isPaused];
    if ([interfaceItem action] == @selector(stepInto:))
        return [self isPaused];
    return YES;
}

#pragma mark -
#pragma mark WebView Frame Load Delegate

- (void)webView:(WebView *)sender windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject
{
    // note: this is the Debuggers's own WebView, not the one being debugged
    [[sender windowScriptObject] setValue:self forKey:@"DebuggerDocument"];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    // note: this is the Debuggers's own WebView, not the one being debugged
    webViewLoaded = YES;
}

#pragma mark -
#pragma mark Debug Listener Callbacks

- (void)webView:(WebView *)view didLoadMainResourceForDataSource:(WebDataSource *)dataSource
{
    NSString *documentSourceCopy = nil;
    id <WebDocumentRepresentation> rep = [dataSource representation];
    if ([rep canProvideDocumentSource])
        documentSourceCopy = [[rep documentSource] copy];

    if (!documentSourceCopy)
        return;

    NSString *urlCopy = [[[[dataSource response] URL] absoluteString] copy];
    NSArray *args = [NSArray arrayWithObjects:(documentSourceCopy ? documentSourceCopy : @""), (urlCopy ? urlCopy : @""), [NSNumber numberWithBool:NO], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"updateFileSource" withArguments:args];

    [documentSourceCopy release];
    [urlCopy release];
}

- (void)webView:(WebView *)view didParseSource:(NSString *)source baseLineNumber:(unsigned)baseLine fromURL:(NSURL *)url sourceId:(int)sid forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    NSString *sourceCopy = [source copy];
    if (!sourceCopy)
        return;

    NSString *documentSourceCopy = nil;
    NSString *urlCopy = [[url absoluteString] copy];

    WebDataSource *dataSource = [webFrame dataSource];
    if (!url || [[[dataSource response] URL] isEqual:url]) {
        id <WebDocumentRepresentation> rep = [dataSource representation];
        if ([rep canProvideDocumentSource])
            documentSourceCopy = [[rep documentSource] copy];
        if (!urlCopy)
            urlCopy = [[[[dataSource response] URL] absoluteString] copy];
    }

    NSArray *args = [NSArray arrayWithObjects:sourceCopy, (documentSourceCopy ? documentSourceCopy : @""), (urlCopy ? urlCopy : @""), [NSNumber numberWithInt:sid], [NSNumber numberWithUnsignedInt:baseLine], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"didParseScript" withArguments:args];

    [sourceCopy release];
    [documentSourceCopy release];
    [urlCopy release];
}

- (void)webView:(WebView *)view failedToParseSource:(NSString *)source baseLineNumber:(unsigned)baseLine fromURL:(NSURL *)url withError:(NSError *)error forWebFrame:(WebFrame *)webFrame
{
}

- (void)webView:(WebView *)view didEnterCallFrame:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    id old = currentFrame;
    currentFrame = [frame retain];
    [old release];

    NSArray *args = [NSArray arrayWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"didEnterCallFrame" withArguments:args];
}

- (void)webView:(WebView *)view willExecuteStatement:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    NSArray *args = [NSArray arrayWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"willExecuteStatement" withArguments:args];
}

- (void)webView:(WebView *)view willLeaveCallFrame:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    NSArray *args = [NSArray arrayWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"willLeaveCallFrame" withArguments:args];

    id old = currentFrame;
    currentFrame = [[frame caller] retain];
    [old release];
}
@end
