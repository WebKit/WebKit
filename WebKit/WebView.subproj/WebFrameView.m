/*	IFWebView.mm
	Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/IFWebView.h>

#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFHTMLView.h>
#import <WebKit/IFImageView.h>
#import <WebKit/IFTextView.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFWebCoreViewFactory.h>
#import <WebKit/IFWebDataSource.h>
#import <WebKit/IFWebFrame.h>
#import <WebKit/IFWebKitErrors.h>
#import <WebKit/IFTextRendererFactory.h>
#import <WebKit/IFImageRendererFactory.h>
#import <WebKit/IFCookieAdapter.h>

#import <WebFoundation/IFNSDictionaryExtensions.h>
#import <WebFoundation/IFNSStringExtensions.h>
#import <WebFoundation/IFNSURLExtensions.h>
#import <WebFoundation/WebFoundation.h>

enum {
    SpaceKey = 0x0020
};

@implementation IFWebView

+ (void)initialize
{
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
    
    IFErrorDescriptionCantShowMIMEType, [NSNumber numberWithInt: IFErrorCodeCantShowMIMEType],
    IFErrorDescriptionCouldntFindApplicationForFile, [NSNumber numberWithInt: IFErrorCodeCouldntFindApplicationForFile],
    IFErrorDescriptionCouldntFindApplicationForURL, [NSNumber numberWithInt: IFErrorCodeCouldntFindApplicationForURL],
    IFErrorDescriptionFileDoesntExist, [NSNumber numberWithInt: IFErrorCodeFileDoesntExist],
    IFErrorDescriptionFileNotReadable, [NSNumber numberWithInt: IFErrorCodeFileNotReadable],
    IFErrorDescriptionFinderCouldntOpenDirectory, [NSNumber numberWithInt: IFErrorCodeFinderCouldntOpenDirectory],
    IFErrorDescriptionCantShowDirectory, [NSNumber numberWithInt: IFErrorCodeCantShowDirectory],
    IFErrorDescriptionCantShowURL, [NSNumber numberWithInt: IFErrorCodeCantShowURL],
    nil];

    [IFError addErrorsFromDictionary:dict];
}

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
 
    [IFWebCoreViewFactory createSharedFactory];
    [IFTextRendererFactory createSharedFactory];
    [IFImageRendererFactory createSharedFactory];
    [IFCookieAdapter createSharedAdapter];
    
    _private = [[IFWebViewPrivate alloc] init];

    IFDynamicScrollBarsView *scrollView  = [[IFDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,frame.size.width,frame.size.height)];
    _private->frameScrollView = scrollView;
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    
    _private->draggingTypes = [[NSArray arrayWithObjects:@"NSFilenamesPboardType", 
                                    @"NSURLPboardType", @"NSStringPboardType", nil] retain];
    [self registerForDraggedTypes:_private->draggingTypes];
    
    return self;
}


- (void)dealloc 
{
    [_private release];
    [super dealloc];
}

- (void)setAllowsScrolling: (BOOL)flag
{
    [[self frameScrollView] setAllowsScrolling: flag];
}

- (BOOL)allowsScrolling
{
    return [[self frameScrollView] allowsScrolling];
}

- frameScrollView
{
    return _private->frameScrollView;
}   

- documentView
{
    return [[self frameScrollView] documentView];
}

// Note that the controller is not retained.
- (IFWebController *)controller
{
    return _private->controller;
}


- (BOOL) isDocumentHTML
{
    return [[[self documentView] className] isEqualToString:@"IFHTMLView"];
}


- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    NSString *dragType, *file, *URLString;
    NSArray *files;
        
    dragType = [[sender draggingPasteboard] availableTypeFromArray:_private->draggingTypes];
    if([dragType isEqualToString:@"NSFilenamesPboardType"]){
        files = [[sender draggingPasteboard] propertyListForType:@"NSFilenamesPboardType"];
        
        // FIXME: We only look at the first dragged file (2931225)
        file = [files objectAtIndex:0];
        
        if([IFWebController canShowFile:file])
            return NSDragOperationCopy;
            
    }else if([dragType isEqualToString:@"NSURLPboardType"]){
        return NSDragOperationCopy;
    }else if([dragType isEqualToString:@"NSStringPboardType"]){
        URLString = [[sender draggingPasteboard] stringForType:@"NSStringPboardType"];
        if([URLString _IF_looksLikeAbsoluteURL])
            return NSDragOperationCopy;
    }
    return NSDragOperationNone;
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender
{
    return YES;
}

- (void)concludeDragOperation:(id <NSDraggingInfo>)sender
{
    IFWebDataSource *dataSource;
    IFWebFrame *frame;
    NSArray *files;
    NSString *file, *dragType;
    NSURL *URL=nil;

    dragType = [[sender draggingPasteboard] availableTypeFromArray:_private->draggingTypes];
    if([dragType isEqualToString:@"NSFilenamesPboardType"]){
        files = [[sender draggingPasteboard] propertyListForType:@"NSFilenamesPboardType"];
        file = [files objectAtIndex:0];
        URL = [NSURL fileURLWithPath:file];
    }else if([dragType isEqualToString:@"NSURLPboardType"]){
        URL = [NSURL URLFromPasteboard:[sender draggingPasteboard]];
    }else if([dragType isEqualToString:@"NSStringPboardType"]){
        URL = [NSURL _IF_URLWithString:[[sender draggingPasteboard] stringForType:@"NSStringPboardType"]];
    }

    if(!URL){
        return;
    }
    
    dataSource = [[[IFWebDataSource alloc] initWithURL:URL] autorelease];
    frame = nil;
    frame = [[self controller] mainFrame];
    if([frame setProvisionalDataSource:dataSource])
        [frame startLoading];
}

+ (void) registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType
{
    // FIXME: OK to allow developers to override built-in views?
    [[self _viewTypes] setObject:viewClass forKey:MIMEType];
}

+ (id <IFDocumentLoading>) createViewForMIMEType:(NSString *)MIMEType
{
    Class viewClass = [[self _viewTypes] _IF_objectForMIMEType:MIMEType];
    return viewClass ? [[[viewClass alloc] init] autorelease] : nil;
}

-(BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}

- (void)drawRect:(NSRect)rect
{
    if ([self documentView] == nil) {
        // Need to paint ourselves if there's no documentView to do it instead.
        [[NSColor whiteColor] set];
        NSRectFill(rect);
    }
}

- (NSWindow *)window
{
    NSWindow *window = [super window];

    if (window == nil) {
	window = [[[self controller] windowContext] window];
    }

    return window;
}

- (void)keyDown:(NSEvent *)event
{
    NSString *characters = [event characters];
    int index, count;
    BOOL callSuper = YES;

    count = [characters length];
    for (index = 0; index < count; ++index) {
        switch ([characters characterAtIndex:index]) {
            case NSDeleteCharacter:
                [self _goBack];
                callSuper = NO;
                break;
            case SpaceKey:
                if ([event modifierFlags] & NSShiftKeyMask) {
                    [self _pageUp];
                } else {
                    [self _pageDown];
                }
                callSuper = NO;
                break;
            case NSPageUpFunctionKey:
                [self _pageUp];
                callSuper = NO;
                break;
            case NSPageDownFunctionKey:
                [self _pageDown];
                callSuper = NO;
                break;
            case NSHomeFunctionKey:
                [self _scrollToTopLeft];
                callSuper = NO;
                break;
            case NSEndFunctionKey:
                [self _scrollToBottomLeft];
                callSuper = NO;
                break;
            case NSUpArrowFunctionKey:
                if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageUp];
                } else {
                    [self _lineUp];
                }
                callSuper = NO;
                break;
            case NSDownArrowFunctionKey:
                if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageDown];
                } else {
                    [self _lineDown];
                }
                callSuper = NO;
                break;
            case NSLeftArrowFunctionKey:
                if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageLeft];
                } else {
                    [self _lineLeft];
                }
                callSuper = NO;
                break;
            case NSRightArrowFunctionKey:
                if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageRight];
                } else {
                    [self _lineRight];
                }
                callSuper = NO;
                break;
        }
    }
    
    if (callSuper) {
        [super keyDown:event];
    }
}

@end
