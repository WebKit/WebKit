/*	
    WebImageView.m
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebImageView.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebFrameViewPrivate.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebViewPrivate.h>

#import <WebCore/WebCoreImageRenderer.h>

#import <WebKit/WebAssertions.h>

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

// Ensures that the view always fills the content area (so we draw over the previous page)
// and that the view is at least as large as the image.
- (void)setFrameSizeUsingImage
{
    NSSize size = [[self _web_superviewOfClass:[NSClipView class]] frame].size;
    NSSize imageSize = [[rep image] size];
    size.width = MAX(size.width, imageSize.width);
    size.height = MAX(size.height, imageSize.height);
    [super setFrameSize:size];
}

- (void)setFrameSize:(NSSize)size
{
    [self setFrameSizeUsingImage];
}

- (void)layout
{
    [self setFrameSizeUsingImage];    
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
    NSData *TIFFData = [self haveCompleteImage] ? [[rep image] TIFFRepresentation] : nil;
    
    if (TIFFData) {
        [pasteboard declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
        [pasteboard setData:TIFFData forType:NSTIFFPboardType];
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

    [self _web_dragPromisedImage:[rep image]
                            rect:[self drawingRect]
                             URL:[rep URL]
                           title:nil
                           event:event];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSURL *URL = [rep URL];
    [[self webView] _downloadURL:URL toDirectory:[dropDestination path]];

    // FIXME: The file is supposed to be created at this point so the Finder places the file
    // where the drag ended. Since we can't create the file until the download starts,
    // this fails. Even if we did create the file at this point, the Finder doesn't
    // place the file in the right place anyway (2825055).
    // FIXME: We may return a different filename than the file that we will create.
    // Since the file isn't created at this point anwyway, it doesn't matter what we return.
    return [NSArray arrayWithObject:[[URL path] lastPathComponent]];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    // Reregister for drag types because they were unregistered before the drag.
    [[[self _web_parentWebFrameView] _webView] _registerDraggedTypes];

    // Balance the previous retain from when the drag started.
    [self release];
}

@end
