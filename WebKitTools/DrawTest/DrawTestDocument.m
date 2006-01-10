/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Nefaur Khandker <nefaurk@gmail.com>  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "DrawTestDocument.h"
#import "DrawTestView.h"
#import "DrawTestToolbarController.h"
#import <WebKit/WebView.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebDataSource.h>

@implementation DrawTestDocument

- (id)initWithType:(NSString *)typeName error:(NSError **)outError
{
    if (outError) {
        NSDictionary *errorInfo = [NSDictionary dictionaryWithObjectsAndKeys:
            @"No document could be created.", NSLocalizedDescriptionKey,
            @"New document creation not yet supported.", NSLocalizedFailureReasonErrorKey,
            nil];
        *outError = [NSError errorWithDomain:NSCocoaErrorDomain code:0 userInfo:errorInfo];
    }
    [self release];
    return nil;
}

- (void)dealloc
{
    [toolbarController release];
    [super dealloc];
}

- (NSString *)windowNibName
{
    return @"DrawTestDocument";
}

- (BOOL)readFromFile:(NSString *)filename ofType:(NSString *)docType
{
    // TODO: Check the validity of the document before returning YES.
    return YES;
}

- (void)windowControllerDidLoadNib:(NSWindowController *)aController
{
    [super windowControllerDidLoadNib:aController];
    toolbarController = [[DrawTestToolbarController alloc] initWithDrawView:drawView];
    [drawView setDocument:[self fileURL]];
}

- (IBAction)dumpSVGToConsole:(id)sender
{
    WebDataSource* dataSource = [[drawView mainFrame] dataSource];
    NSLog(@"SVG Markup for file %@:\n%@", [self fileURL], [[dataSource representation] documentSource]);
}

- (IBAction)openSourceForSelection:(id)sender
{
    // TODO: The "path" message (below) will not produce a valid pathname if we are dealing with a remote file.
    NSString *filename = [[self fileURL] path];
    [[NSWorkspace sharedWorkspace] openFile:filename withApplication:@"TextEdit"];
}

- (NSData *)dataRepresentationOfType:(NSString *)aType
{
    WebDataSource* dataSource = [[drawView mainFrame] dataSource];
    return [dataSource data];
}

#pragma mark -
#pragma mark Debug Methods

- (IBAction)toggleDebugDrawer:(id)sender
{
    [debugDrawer toggle:sender];
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    return nil;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return NO;
}

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return 0;
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    return nil;
}

- (IBAction)runWindowResizeTest:(id)sender
{
    NSWindow *window = [drawView window];
    NSScreen *screen = [window screen];
    float screenHeight = [screen visibleFrame].size.height;
    NSRect originalFrame = [window frame];
    // initial setup
    BOOL toolbarVisible = [[window toolbar] isVisible];
    if (toolbarVisible) [window toggleToolbarShown:self];
    [window setFrame:NSMakeRect(0,screenHeight-100,100,100) display:YES];
    
    // grab time.
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
    
    // run test
    for (int x = 0; x < 3; x++) {
        for (float size = 100; size < 500.f; size += 20.f) {
            [window setFrame:NSMakeRect(0, screenHeight-size, size, size) display:YES];
        }
    }
    
    double elapsed = CFAbsoluteTimeGetCurrent() - start;
    
    // log
    NSLog(@"Window resize test: %fs", elapsed);
    
    // restore
    if (toolbarVisible) [window toggleToolbarShown:self];
    [window setFrame:originalFrame display:YES];
}

@end
