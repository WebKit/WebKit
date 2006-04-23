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

#import <WebKit/WebImageView.h>

#import <WebKit/WebArchiver.h>
#import <JavaScriptCore/Assertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrameView.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebNSFileManagerExtras.h>
#import <WebKit/WebNSObjectExtras.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebViewInternal.h>
#import <WebKit/WebUIDelegatePrivate.h>

#import <WebCore/WebCoreImageRenderer.h>
#import <WebCore/WebCoreFrameBridge.h>

@implementation WebImageView

+ (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[NSArray arrayWithObject:NSTIFFPboardType] returnTypes:nil];
}

+ (NSArray *)supportedMIMETypes
{
    return [WebImageRepresentation supportedMIMETypes];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (!self)
        return nil;
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

- (void)dealloc
{
    [[rep image] stopAnimation];
    [rep release];
    [mouseDownEvent release];
    
    [super dealloc];
}

- (void)finalize
{
    [[rep image] stopAnimation];
    [super finalize];
}

- (BOOL)haveCompleteImage
{
    NSSize imageSize = [[rep image] size];
    return [rep doneLoading] && imageSize.width > 0 && imageSize.width > 0;
}

- (BOOL)isFlipped 
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    // Being first responder is useful for scrolling from the keyboard at least.
    return YES;
}

- (NSRect)drawingRect
{
    NSSize imageSize = [[rep image] size];
    return NSMakeRect(0, 0, imageSize.width, imageSize.height);
}

- (void)drawRect:(NSRect)rect
{
    if (needsLayout) {
        [self layout];
    }
    
    if ([[self _webView] drawsBackground]) {
        [[NSColor whiteColor] set];
        NSRectFill(rect);
    }
    
    NSRect drawingRect = [self drawingRect];
    [[rep image] drawImageInRect:drawingRect fromRect:drawingRect];
}

- (void)adjustFrameSize
{
    NSSize size = [[rep image] size];
    
    // When drawing on screen, ensure that the view always fills the content area 
    // (so we draw over the entire previous page), and that the view is at least 
    // as large as the image.. Otherwise we're printing, and we want the image to 
    // fill the view so that the printed size doesn't depend on the window size.
    if ([NSGraphicsContext currentContextDrawingToScreen]) {
        NSSize clipViewSize = [[self _web_superviewOfClass:[NSClipView class]] frame].size;
        size.width = MAX(size.width, clipViewSize.width);
        size.height = MAX(size.height, clipViewSize.height);
    }
    
    [super setFrameSize:size];
}

- (void)setFrameSize:(NSSize)size
{
    [self adjustFrameSize];
}

- (void)layout
{
    [self adjustFrameSize];    
    needsLayout = NO;
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    ASSERT(!rep);
    rep = [[dataSource representation] retain];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
    NSSize imageSize = [[rep image] size];
    if (imageSize.width > 0 && imageSize.height > 0) {
        [self setNeedsLayout:YES];
        [self layout];
        [self setNeedsDisplay:YES];
    }
}

- (void)setNeedsLayout:(BOOL)flag
{
    needsLayout = flag;
}

- (void)viewWillMoveToHostWindow:(NSWindow *)hostWindow
{
}

- (void)viewDidMoveToHostWindow
{
}

- (void)viewDidMoveToWindow
{
    if (![self window]){
        [[rep image] stopAnimation];
    }
    
    [super viewDidMoveToWindow];
}

- (WebView *)webView
{
    return [self _web_parentWebView];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    if ([item action] == @selector(copy:)){
        return [self haveCompleteImage];
    }

    return YES;
}

- (id)validRequestorForSendType:(NSString *)sendType returnType:(NSString *)returnType
{
    if (sendType && [sendType isEqualToString:NSTIFFPboardType]){
        return self;
    }

    return [super validRequestorForSendType:sendType returnType:returnType];
}

- (BOOL)writeImageToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
    if ([self haveCompleteImage]) {
        [pasteboard _web_writeImage:[rep image] element:nil URL:[rep URL] title:nil archive:[WebArchiver archiveFrame:frame] types:types];
        return YES;
    }
    
    return NO;
}

- (void)copy:(id)sender
{
    WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
    NSArray *types = [NSPasteboard _web_writableTypesForImageIncludingArchive:([WebArchiver archiveFrame:frame] != nil)];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard declareTypes:types owner:nil];
    [self writeImageToPasteboard:pasteboard types:types];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [pasteboard declareTypes:types owner:nil];
    return [self writeImageToPasteboard:pasteboard types:types];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point
{
    WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
    ASSERT(frame);
    
    return [NSDictionary dictionaryWithObjectsAndKeys:
        [[rep image] image],                    WebElementImageKey,
        [NSValue valueWithRect:[self bounds]],  WebElementImageRectKey,
        [rep URL],                              WebElementImageURLKey,
        [NSNumber numberWithBool:NO],           WebElementIsSelectedKey,
        frame,                                  WebElementFrameKey, nil];
}

- (NSDictionary *)elementAtPoint:(NSPoint)point allowShadowContent:(BOOL)allow
{
    return [self elementAtPoint:point];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    WebView *webView = [self webView];
    ASSERT(webView);
    return [webView _menuForElement:[self elementAtPoint:NSZeroPoint] defaultItems:nil];
}

- (void)mouseDown:(NSEvent *)event
{
    ignoringMouseDraggedEvents = NO;
    [mouseDownEvent release];
    mouseDownEvent = [event retain];
    
    WebView *webView = [self webView];
    NSPoint point = [webView convertPoint:[mouseDownEvent locationInWindow] fromView:nil];
    dragSourceActionMask = [[webView _UIDelegateForwarder] webView:webView dragSourceActionMaskForPoint:point];
    
    [super mouseDown:event];
}

- (void)mouseDragged:(NSEvent *)mouseDraggedEvent
{
    if (ignoringMouseDraggedEvents || ![self haveCompleteImage] || !(dragSourceActionMask & WebDragSourceActionImage)) {
        return;
    }
    
    WebFrame *frame = [[self _web_parentWebFrameView] webFrame];
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithName:NSDragPboard];
    id source = [pasteboard _web_declareAndWriteDragImage:[rep image]
                                                  element:nil
                                                      URL:[rep URL]
                                                    title:nil
                                                  archive:[WebArchiver archiveFrame:frame]
                                                   source:self];
    
    WebView *webView = [self webView];
    NSPoint point = [webView convertPoint:[mouseDownEvent locationInWindow] fromView:nil];
    [[webView _UIDelegateForwarder] webView:webView willPerformDragSourceAction:WebDragSourceActionImage fromPoint:point withPasteboard:pasteboard];
    
    [[self webView] _setInitiatedDrag:YES];
    
    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];
    
    [self _web_dragImage:[rep image]
                 element:nil
                    rect:[self drawingRect]
                   event:mouseDraggedEvent
              pasteboard:pasteboard
                  source:source
                  offset:NULL];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[rep filename]];
    path = [[NSFileManager defaultManager] _webkit_pathWithUniqueFilenameForPath:path];
    [[rep data] writeToFile:path atomically:NO];
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    // Prevent queued mouseDragged events from coming after the drag which can cause a double drag.
    ignoringMouseDraggedEvents = YES;
    
    [[self webView] _setInitiatedDrag:NO];

    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSImage *)image
{
    return [[rep image] image];
}

#pragma mark PRINTING

- (void)drawPageBorderWithSize:(NSSize)borderSize
{
    ASSERT(NSEqualSizes(borderSize, [[[NSPrintOperation currentOperation] printInfo] paperSize]));
    // FIXME: How to determine the number of pages required to print the whole image?
    [[self webView] _drawHeaderAndFooter];
}

- (void)beginDocument
{
    [self adjustFrameSize];
    [[self webView] _adjustPrintingMarginsForHeaderAndFooter];
    [super beginDocument];
}

- (void)endDocument
{
    [super endDocument];
    [self adjustFrameSize];
}

- (BOOL)canPrintHeadersAndFooters
{
    return YES;
}

@end
