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
    WebController *controller = [self controller];
    
    NSDictionary *element = [NSDictionary dictionaryWithObjectsAndKeys:
        [representation image], WebElementImageKey,
        [representation URL], WebElementImageURLKey,
        [controller frameForView:webView], WebElementFrameKey, nil];
        
    return [controller _menuForElement:element];
}

- (void)mouseDragged:(NSEvent *)event
{
    if(acceptsDrags){
        [self dragPromisedFilesOfTypes:[NSArray arrayWithObject:[[[representation URL] path] pathExtension]]
                              fromRect:NSZeroRect
                                source:self
                             slideBack:YES
                                 event:event];
    }
}

// Subclassing dragImage for image drags let's us change aspects of the drag that the
// promised file API doesn't provide such as a different drag image, other pboard types etc.
- (void)dragImage:(NSImage *)anImage
               at:(NSPoint)imageLoc
           offset:(NSSize)mouseOffset
            event:(NSEvent *)theEvent
       pasteboard:(NSPasteboard *)pboard
           source:(id)sourceObject
        slideBack:(BOOL)slideBack
{
    [self _web_setPromisedImageDragImage:&anImage
                                      at:&imageLoc
                                  offset:&mouseOffset
                           andPasteboard:pboard
                               withImage:[representation image]
                                andEvent:theEvent];
    
    [super dragImage:anImage
                  at:imageLoc
              offset:mouseOffset
               event:theEvent
          pasteboard:pboard
              source:sourceObject
           slideBack:slideBack];
}

- (NSArray *)namesOfPromisedFilesDroppedAtDestination:(NSURL *)dropDestination
{
    NSURL *URL = [representation URL];
    NSString *filename = [[URL path] lastPathComponent];
    NSString *path = [[dropDestination path] stringByAppendingPathComponent:filename];

    [[self controller] _downloadURL:URL toPath:path];

    return [NSArray arrayWithObject:filename];
}

@end
