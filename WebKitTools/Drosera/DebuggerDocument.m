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
#import "DebuggerApplication.h"
#import <Carbon/Carbon.h>

static NSString *DebuggerConsoleToolbarItem = @"DebuggerConsoleToolbarItem";
static NSString *DebuggerContinueToolbarItem = @"DebuggerContinueToolbarItem";
static NSString *DebuggerPauseToolbarItem = @"DebuggerPauseToolbarItem";
static NSString *DebuggerStepIntoToolbarItem = @"DebuggerStepIntoToolbarItem";
static NSString *DebuggerStepOverToolbarItem = @"DebuggerStepOverToolbarItem";
static NSString *DebuggerStepOutToolbarItem = @"DebuggerStepOutToolbarItem";

@interface WebScriptObject (WebScriptObjectPrivate)
- (unsigned int)count;
@end

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
        else if ([frame caller])
            [result addObject:@"(anonymous function)"];
        else
            [result addObject:@"(global scope)"];
        frame = [frame caller];
    }
    return [result autorelease];
}

- (id)evaluateScript:(NSString *)script inCallFrame:(int)frame
{
    WebScriptCallFrame *cframe = currentFrame;
    for (unsigned count = 0; count < frame; count++)
        cframe = [cframe caller];
    if (!cframe)
        return nil;

    id result = [cframe evaluateWebScript:script];
    if ([result isKindOfClass:NSClassFromString(@"WebScriptObject")])
        return [result callWebScriptMethod:@"toString" withArguments:nil];
    return result;
}

- (NSArray *)webScriptAttributeKeysForScriptObject:(WebScriptObject *)object
{
    WebScriptObject *enumerateAttributes = [object evaluateWebScript:@"(function () { var result = new Array(); for (var x in this) { result.push(x); } return result; })"];

    NSMutableArray *result = [[NSMutableArray alloc] init];
    WebScriptObject *variables = [enumerateAttributes callWebScriptMethod:@"call" withArguments:[NSArray arrayWithObject:object]];
    unsigned length = [variables count];
    for (unsigned i = 0; i < length; i++) {
        NSString *key = [variables webScriptValueAtIndex:i];
        [result addObject:key];
    }

    [result sortUsingSelector:@selector(compare:)];
    return [result autorelease];
}

- (NSArray *)localScopeVariableNamesForCallFrame:(int)frame
{
    WebScriptCallFrame *cframe = currentFrame;
    for (unsigned count = 0; count < frame; count++)
        cframe = [cframe caller];

    if (![[cframe scopeChain] count])
        return nil;

    WebScriptObject *scope = [[cframe scopeChain] objectAtIndex:0]; // local is always first
    return [self webScriptAttributeKeysForScriptObject:scope];
}

- (NSString *)valueForScopeVariableNamed:(NSString *)key inCallFrame:(int)frame
{
    WebScriptCallFrame *cframe = currentFrame;
    for (unsigned count = 0; count < frame; count++)
        cframe = [cframe caller];

    if (![[cframe scopeChain] count])
        return nil;

    unsigned scopeCount = [[cframe scopeChain] count];
    for (unsigned i = 0; i < scopeCount; i++) {
        WebScriptObject *scope = [[cframe scopeChain] objectAtIndex:i];
        id value = [scope valueForKey:key];
        if ([value isKindOfClass:NSClassFromString(@"WebScriptObject")])
            return [value callWebScriptMethod:@"toString" withArguments:nil];
        if (value && ![value isKindOfClass:[NSString class]])
            return [NSString stringWithFormat:@"%@", value];
        if (value)
            return value;
    }

    return nil;
}

#pragma mark -
#pragma mark System Information

- (int)doubleClickMilliseconds
{
    // GetDblTime() returns values in 1/60ths of a second
    return ((double)GetDblTime() / 60.0) * 1000;
}

#pragma mark -
#pragma mark File Loading

- (NSString *)breakpointEditorHTML
{
    return [NSString stringWithContentsOfFile:[[NSBundle mainBundle] pathForResource:@"breakpointEditor" ofType:@"html"]];
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

- (IBAction)stepOver:(id)sender
{
    [[webView windowScriptObject] callWebScriptMethod:@"stepOver" withArguments:nil];
}

- (IBAction)stepOut:(id)sender
{
    [[webView windowScriptObject] callWebScriptMethod:@"stepOut" withArguments:nil];
}

- (IBAction)showConsole:(id)sender
{
    [[webView windowScriptObject] callWebScriptMethod:@"showConsoleWindow" withArguments:nil];
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

    [[NSNotificationCenter defaultCenter] removeObserver:self name:NSApplicationWillTerminateNotification object:nil];

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
            [server removeListener:self];
            [self resume];
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
    if (action == @selector(pause:))
        return ![self isPaused];
    if (action == @selector(resume:) ||
        action == @selector(stepOver:) ||
        action == @selector(stepOut:) ||
        action == @selector(stepInto:))
        return [self isPaused];
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

- (void)scriptConfirmSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(int *)contextInfo
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

    int result = NSNotFound;
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
    if ([[sender window] isEqual:[self window]])
        [[sender windowScriptObject] setValue:self forKey:@"DebuggerDocument"];
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame
{
    // note: this is the Debuggers's own WebView, not the one being debugged
    if ([[sender window] isEqual:[self window]])
        webViewLoaded = YES;
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame
{
    // note: this is the Debuggers's own WebViews, not the one being debugged
    if ([frame isEqual:[sender mainFrame]]) {
        NSDictionary *info = [[(DebuggerApplication *)[[NSApplication sharedApplication] delegate] knownServers] objectForKey:currentServerName];
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
    NSArray *args = [[NSArray alloc] initWithObjects:(documentSourceCopy ? documentSourceCopy : @""), (urlCopy ? urlCopy : @""), [NSNumber numberWithBool:NO], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"updateFileSource" withArguments:args];

    [args release];
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

    NSArray *args = [[NSArray alloc] initWithObjects:sourceCopy, (documentSourceCopy ? documentSourceCopy : @""), (urlCopy ? urlCopy : @""), [NSNumber numberWithInt:sid], [NSNumber numberWithUnsignedInt:baseLine], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"didParseScript" withArguments:args];

    [args release];
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

    NSArray *args = [[NSArray alloc] initWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"didEnterCallFrame" withArguments:args];
    [args release];
}

- (void)webView:(WebView *)view willExecuteStatement:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    NSArray *args = [[NSArray alloc] initWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"willExecuteStatement" withArguments:args];
    [args release];
}

- (void)webView:(WebView *)view willLeaveCallFrame:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    NSArray *args = [[NSArray alloc] initWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"willLeaveCallFrame" withArguments:args];
    [args release];

    id old = currentFrame;
    currentFrame = [[frame caller] retain];
    [old release];
}

- (void)webView:(WebView *)view exceptionWasRaised:(WebScriptCallFrame *)frame sourceId:(int)sid line:(int)lineno forWebFrame:(WebFrame *)webFrame
{
    if (!webViewLoaded)
        return;

    NSArray *args = [[NSArray alloc] initWithObjects:[NSNumber numberWithInt:sid], [NSNumber numberWithInt:lineno], nil];
    [[webView windowScriptObject] callWebScriptMethod:@"exceptionWasRaised" withArguments:args];
    [args release];
}
@end
