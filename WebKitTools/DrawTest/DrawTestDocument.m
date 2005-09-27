/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebCore/DrawDocumentPrivate.h>

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
    [document release];
    [super dealloc];
}

- (NSString *)windowNibName
{
    return @"DrawTestDocument";
}

- (IBAction)dumpSVGToConsole:(id)sender
{
    NSLog(@"SVG: %@", [document svgText]);
}

- (void)sizeWindowToFitCanvas
{
    NSSize canvasSize = [document canvasSize];
    if ((canvasSize.width > 10) && (canvasSize.height > 10)) {
        NSWindow *window = [drawView window];
        //canvasSize.height += [drawView frame].origin.y; // to accomidate the tool pallette
        NSRect newFrame = [window frameRectForContentRect:NSMakeRect(0,0,canvasSize.width, canvasSize.height)];
        newFrame = [window constrainFrameRect:newFrame toScreen:[window screen]];
        // we really should not show margins here.
        [window setFrame:newFrame display:YES];
    }
}

- (void)windowControllerDidLoadNib:(NSWindowController *)aController
{
    [super windowControllerDidLoadNib:aController];
    toolbarController = [[DrawTestToolbarController alloc] initWithDrawView:drawView];
    [drawView setDocument:[self drawDocument]];
    [self sizeWindowToFitCanvas];
}

- (IBAction)openSourceForSelection:(id)sender
{
    [[NSWorkspace sharedWorkspace] openFile:[self fileName] withApplication:@"TextEdit"];
}

- (IBAction)zoomToContent:(id)sender
{
    [document sizeCanvasToFitContent];
    [drawView setNeedsDisplay:YES];
    [self sizeWindowToFitCanvas];
}

- (NSData *)dataRepresentationOfType:(NSString *)aType
{
    return [[document svgText] dataUsingEncoding:NSUTF8StringEncoding];
}

- (BOOL)loadDataRepresentation:(NSData *)data ofType:(NSString *)aType
{
    [self setDrawDocument:[DrawDocument documentWithSVGData:data]];
    return YES;
}

- (void)setDrawDocument:(DrawDocument *)drawDocument
{
    id oldDoc = document;
    document = [drawDocument retain];
    [oldDoc release];
}

- (DrawDocument *)drawDocument
{
    //if (!document) document = [[DrawDocument alloc] init];
    return document;
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
