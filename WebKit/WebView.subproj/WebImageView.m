/*	
    WebImageView.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebImageView.h>

#import <WebCore/WebCoreImageRenderer.h>
#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRepresentation.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebView.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebAssertions.h>

@implementation WebImageView

- (void)initialize
{
    [NSApp registerServicesMenuSendTypes:[NSArray arrayWithObject:NSTIFFPboardType] returnTypes:nil];
}

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        acceptsDrags = YES;
        acceptsDrops = YES;
    }
    return self;
}

- (void)dealloc
{
    [[representation image] stopAnimation];
    [representation release];
    
    [super dealloc];
}

- (BOOL)isFlipped 
{
    return YES;
}

- (void)drawRect:(NSRect)rect
{
    [[representation image] beginAnimationInRect:[self frame] fromRect:[self frame]];
}

- (void)setDataSource:(WebDataSource *)dataSource
{
    representation = [[dataSource representation] retain];
}

- (void)dataSourceUpdated:(WebDataSource *)dataSource
{
}

- (void)setNeedsLayout: (BOOL)flag
{
}

- (void)layout
{
    WebImageRenderer *image = [representation image];
    if (image) {
        [self setFrameSize:[image size]];
    } else {
        [self setFrameSize:NSMakeSize(0, 0)];
    }
}

- (void)setAcceptsDrags: (BOOL)flag
{
    acceptsDrags = flag;
}

- (BOOL)acceptsDrags
{
    return acceptsDrags;
}

- (void)setAcceptsDrops: (BOOL)flag
{
    acceptsDrops = flag;
}

- (BOOL)acceptsDrops
{
    return acceptsDrops;
}

- (void)viewDidMoveToWindow
{
    if (![self window]){
        [[representation image] stopAnimation];
    }
    
    [super viewDidMoveToWindow];
}

- (WebController *)controller
{
    return [[self _web_parentWebView] controller];
}

- (BOOL)validateUserInterfaceItem:(id <NSValidatedUserInterfaceItem>)item
{
    if ([item action] == @selector(copy:)){
        return ([representation image] != nil);
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

- (void)writeImageToPasteboard:(NSPasteboard *)pasteboard
{
    NSData *TIFFData = [[representation image] TIFFRepresentation];
    
    if(TIFFData){
        [pasteboard declareTypes:[NSArray arrayWithObject:NSTIFFPboardType] owner:nil];
        [pasteboard setData:TIFFData forType:NSTIFFPboardType];
    }
}

- (void)copy:(id)sender
{
    [self writeImageToPasteboard:[NSPasteboard generalPasteboard]];
}

- (BOOL)writeSelectionToPasteboard:(NSPasteboard *)pasteboard types:(NSArray *)types
{
    [self writeImageToPasteboard:pasteboard];
    return YES;
}

- (NSMenu *)menuForEvent:(NSEvent *)theEvent
{
    WebView *webView = [self _web_parentWebView];
    WebController *controller = [webView controller];
    WebFrame *frame = [controller frameForView:webView];

    ASSERT(frame);
    ASSERT(controller);
    
    NSDictionary *element = [NSDictionary dictionaryWithObjectsAndKeys:
        [representation image], WebElementImageKey,
        [representation URL], WebElementImageURLKey,
        [NSNumber numberWithBool:NO], WebElementIsSelectedTextKey,
        frame, WebElementFrameKey, nil];
        
    return [controller _menuForElement:element];
}

- (void)mouseDragged:(NSEvent *)event
{
    if(acceptsDrags){
        // Don't allow drags to be accepted by this WebView.
        [[self _web_parentWebView] unregisterDraggedTypes];

        // Retain this view during the drag because it may be released before the drag ends.
        [self retain];
        
        [self _web_dragPromisedImage:[representation image]
                              origin:NSZeroPoint
                                 URL:[representation URL]
                            fileType:[[[representation URL] path] pathExtension]
                               title:nil
                               event:event];
    }
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSURL *URL = [representation URL];
    NSString *filename = [[URL path] lastPathComponent];
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:filename];
    
    [[self controller] _downloadURL:URL toPath:path];

    return [NSArray arrayWithObject:filename];
}

- (void)draggedImage:(NSImage *)anImage endedAt:(NSPoint)aPoint operation:(NSDragOperation)operation
{
    // Reregister for drag types because they were unregistered before the drag.
    [[self _web_parentWebView] _reregisterDraggedTypes];

    // Balance the previous retain from when the drag started.
    [self release];
}

@end
