/*	WebView.mm
	Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebView.h>

#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebController.h>
#import <WebKit/WebViewFactory.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebCookieAdapter.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSViewExtras.h>

#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebFoundation.h>

enum {
    SpaceKey = 0x0020
};

@implementation WebView

+ (void)initialize
{
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
    
    WebErrorDescriptionCannotShowMIMEType, [NSNumber numberWithInt: WebErrorCannotShowMIMEType],
    WebErrorDescriptionCouldNotFindApplicationForFile, [NSNumber numberWithInt: WebErrorCouldNotFindApplicationForFile],
    WebErrorDescriptionCouldNotFindApplicationForURL, [NSNumber numberWithInt: WebErrorCouldNotFindApplicationForURL],
    WebErrorDescriptionFileDoesNotExist, [NSNumber numberWithInt: WebErrorFileDoesNotExist],
    WebErrorDescriptionFileNotReadable, [NSNumber numberWithInt: WebErrorFileNotReadable],
    WebErrorDescriptionFinderCouldNotOpenDirectory, [NSNumber numberWithInt: WebErrorFinderCouldNotOpenDirectory],
    WebErrorDescriptionCannotShowDirectory, [NSNumber numberWithInt: WebErrorCannotShowDirectory],
    WebErrorDescriptionCannotShowURL, [NSNumber numberWithInt: WebErrorCannotShowURL],
    nil];

    [WebError addErrorsFromDictionary:dict];
}

- initWithFrame: (NSRect) frame
{
    [super initWithFrame: frame];
 
    [WebViewFactory createSharedFactory];
    [WebTextRendererFactory createSharedFactory];
    [WebImageRendererFactory createSharedFactory];
    [WebCookieAdapter createSharedAdapter];
    
    _private = [[WebViewPrivate alloc] init];

    WebDynamicScrollBarsView *scrollView  = [[WebDynamicScrollBarsView alloc] initWithFrame: NSMakeRect(0,0,frame.size.width,frame.size.height)];
    _private->frameScrollView = scrollView;
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    
    [self registerForDraggedTypes:[self _web_acceptableDragTypes]];
    
    ++WebViewCount;
    
    return self;
}

- (void)dealloc 
{
    --WebViewCount;
    
    [_private release];
    
    [super dealloc];
}

- (void)setFrame: (NSRect)f
{
    if ([self isDocumentHTML] && !NSEqualRects(f, [self frame]))
        [(WebHTMLView *)[self documentView] setNeedsLayout: YES];
    [super setFrame: f];
}


- (void)viewWillStartLiveResize
{
}

- (void)viewDidEndLiveResize
{
    [self display];
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
- (WebController *)controller
{
    return _private->controller;
}


- (BOOL) isDocumentHTML
{
    return [[[self documentView] className] isEqualToString:@"WebHTMLView"];
}


- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    return [self _web_dragOperationForDraggingInfo:sender];
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
    NSURL *URL = [self _web_bestURLForDraggingInfo:sender];

    if(URL){
        WebDataSource *dataSource = [[WebDataSource alloc] initWithURL:URL];
        WebFrame *frame = [[self controller] mainFrame];
        if ([frame setProvisionalDataSource:dataSource]){
            [frame startLoading];
        }
        [dataSource release];
    }
}

+ (void) registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType
{
    // FIXME: OK to allow developers to override built-in views?
    [[self _viewTypes] setObject:viewClass forKey:MIMEType];
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
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self _goBack];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self _pageLeft];
                } else {
                    [self _lineLeft];
                }
                callSuper = NO;
                break;
            case NSRightArrowFunctionKey:
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self _goForward];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
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
