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
        [representation image], 		WebElementImageKey,
        [NSValue valueWithRect:[self bounds]], 	WebElementImageRectKey,
        [representation URL], 			WebElementImageURLKey,
        [NSNumber numberWithBool:NO], 		WebElementIsSelectedTextKey,
        frame, 					WebElementFrameKey, nil];
        
    return [controller _menuForElement:element];
}

- (void)mouseDragged:(NSEvent *)event
{
    // Don't allow drags to be accepted by this WebView.
    [[self _web_parentWebView] unregisterDraggedTypes];

    // Retain this view during the drag because it may be released before the drag ends.
    [self retain];
    
    [self _web_dragPromisedImage:[representation image]
                            rect:[self bounds]
                                URL:[representation URL]
                        fileType:[[[representation URL] path] pathExtension]
                            title:nil
                            event:event];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSURL *URL = [representation URL];
    [[self controller] _downloadURL:URL toDirectory:[dropDestination path]];

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
    [[self _web_parentWebView] _reregisterDraggedTypes];

    // Balance the previous retain from when the drag started.
    [self release];
}

@end
