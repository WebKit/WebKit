/*	
    WebImageView.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebImageView.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebViewPrivate.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <Foundation/NSFileManager_NSURLExtras.h>

@implementation WebImageView

+ (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[NSArray arrayWithObject:NSTIFFPboardType] returnTypes:nil];
}

+ (NSArray *)supportedImageMIMETypes
{
    return [[WebImageRendererFactory sharedFactory] supportedMIMETypes];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

- (void)dealloc
{
    [[rep image] stopAnimation];
    [rep release];
    
    [super dealloc];
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
    
    [[NSColor whiteColor] set];
    NSRectFill(rect);
    
    NSRect drawingRect = [self drawingRect];
    [[rep image] beginAnimationInRect:drawingRect fromRect:drawingRect];
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

- (void)beginDocument
{
    [self adjustFrameSize];
    [super beginDocument];
}

- (void)endDocument
{
    [super endDocument];
    [self adjustFrameSize];
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
        [self setNeedsDisplay:YES];
    }
}

- (void)setNeedsLayout: (BOOL)flag
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
    return [[self _web_parentWebFrameView] _webView];
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

- (BOOL)writeImageToPasteboard:(NSPasteboard *)pasteboard
{    
    if ([self haveCompleteImage]) {
        [pasteboard declareTypes:[NSArray arrayWithObjects:NSRTFDPboardType, NSTIFFPboardType, nil] owner:nil];
        [pasteboard _web_writeFileDataAsRTFDAttachment:[rep data] withFilename:[rep filename]];
        [pasteboard setData:[[rep image] TIFFRepresentation] forType:NSTIFFPboardType];
        return YES;
    }
    
    return NO;
}

- (void)copy:(id)sender
{
    [self writeImageToPasteboard:[NSPasteboard generalPasteboard]];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    return [self writeImageToPasteboard:pasteboard];
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    WebFrameView *webFrameView = [self _web_parentWebFrameView];
    WebView *webView = [webFrameView _webView];
    WebFrame *frame = [webFrameView webFrame];

    ASSERT(frame);
    ASSERT(webView);
    
    NSDictionary *element = [NSDictionary dictionaryWithObjectsAndKeys:
        [rep image],                            WebElementImageKey,
        [NSValue valueWithRect:[self bounds]], 	WebElementImageRectKey,
        [rep URL],                              WebElementImageURLKey,
        [NSNumber numberWithBool:NO], 		WebElementIsSelectedKey,
        frame, 					WebElementFrameKey, nil];
        
    return [webView _menuForElement:element];
}

- (void)mouseDragged:(NSEvent *)event
{
    if (![self haveCompleteImage]) {
        return;
    }
    
    // Don't allow drags to be accepted by this WebFrameView.
    [[[self _web_parentWebFrameView] _webView] unregisterDraggedTypes];

    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];

    [self _web_dragImage:[rep image]
            originalData:[rep data]
                    rect:[self drawingRect]
                     URL:[rep URL]
                   title:nil
                   event:event];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    // FIXME: Report an error if we fail to create a file.
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:[rep filename]];
    path = [[NSFileManager defaultManager] _web_pathWithUniqueFilenameForPath:path];
    [[rep data] writeToFile:path atomically:NO];
    return [NSArray arrayWithObject:[path lastPathComponent]];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    // Reregister for drag types because they were unregistered before the drag.
    [[[self _web_parentWebFrameView] _webView] _registerDraggedTypes];

    // Balance the previous retain from when the drag started.
    [self release];
}

- (NSImage *)image
{
    return [rep image];
}

@end
