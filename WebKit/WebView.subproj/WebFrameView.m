/*	WebView.m
	Copyright 2001, 2002, Apple Computer, Inc. All rights reserved.
*/

#import <WebKit/WebView.h>

#import <WebKit/WebClipView.h>
#import <WebKit/WebCookieAdapter.h>
#import <WebKit/WebController.h>
#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebKitErrors.h>
#import <WebKit/WebKitStatisticsPrivate.h>
#import <WebKit/WebNSPasteboardExtras.h>
#import <WebKit/WebNSViewExtras.h>
#import <WebKit/WebTextRendererFactory.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKit/WebViewFactory.h>
#import <WebKit/WebWindowOperationsDelegate.h>

#import <WebFoundation/WebError.h>
#import <WebFoundation/WebLocalizableStrings.h>
#import <WebFoundation/WebNSDictionaryExtras.h>
#import <WebFoundation/WebNSURLExtras.h>
#import <WebFoundation/WebResourceRequest.h>

enum {
    SpaceKey = 0x0020
};

NSString *WebErrorDomainWebKit = @"WebErrorDomainWebKit";

@implementation WebView

#define WebErrorDescriptionCannotCreateFile UI_STRING("Cannot create file", "WebErrorCannotCreateFile description")
#define WebErrorDescriptionCannotOpenFile UI_STRING("Cannot open file", "WebErrorCannotOpenFile description")
#define WebErrorDescriptionCannotWriteToFile UI_STRING("Cannot write file", "WebErrorCannotWriteToFile description")
#define WebErrorDescriptionCannotRemoveFile UI_STRING("Cannot remove file", "WebErrorCannotRemoveFile description")
#define WebErrorDescriptionCannotFindApplicationForFile UI_STRING("Cannot find application for file", "WebErrorCannotFindApplicationForFile description")
#define WebErrorDescriptionFinderCannotOpenDirectory UI_STRING("Finder cannot open directory", "WebErrorFinderCannotOpenDirectory description")
#define WebErrorDescriptionCannotShowDirectory UI_STRING("Cannot show a file directory", "WebErrorCannotShowDirectory description")
#define WebErrorDescriptionCannotShowMIMEType UI_STRING("Cannot show content with specified mime type", "WebErrorCannotShowMIMEType description")
#define WebErrorDescriptionCannotShowURL UI_STRING("Cannot show URL", "WebErrorCannotShowURL description")
#define WebErrorDescriptionCannotFindApplicationForURL UI_STRING("Cannot find application for URL", "WebErrorCannotNotFindApplicationForURL description")
#define WebErrorDescriptionWebErrorCannotFindPlugin UI_STRING("Cannot find plug-in", "WebErrorCannotFindPlugin description")
#define WebErrorDescriptionWebErrorCannotLoadPlugin UI_STRING("Cannot load plug-in", "WebErrorCannotLoadPlugin description")
#define WebErrorDescriptionWebErrorJavaUnavailable UI_STRING("Java is unavailable", "WebErrorJavaUnavailable description")
#define WebErrorDescriptionDownloadDecodingFailedToComplete UI_STRING_KEY("Download decoding failed", "Download decoding failed (at end)", "WebErrorDownloadDecodingFailedToComplete description")
#define WebErrorDescriptionDownloadDecodingFailedMidStream UI_STRING_KEY("Download decoding failed", "Download decoding failed (midstream)", "WebErrorDownloadDecodingFailedMidStream description")

+ (void)initialize
{
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
    WebErrorDescriptionCannotCreateFile, 		[NSNumber numberWithInt: WebErrorCannotCreateFile],
    WebErrorDescriptionCannotOpenFile, 			[NSNumber numberWithInt: WebErrorCannotOpenFile],
    WebErrorDescriptionCannotWriteToFile, 		[NSNumber numberWithInt: WebErrorCannotWriteToFile],
    WebErrorDescriptionCannotRemoveFile, 		[NSNumber numberWithInt: WebErrorCannotRemoveFile],
    WebErrorDescriptionCannotFindApplicationForFile, 	[NSNumber numberWithInt: WebErrorCannotFindApplicationForFile],
    WebErrorDescriptionFinderCannotOpenDirectory, 	[NSNumber numberWithInt: WebErrorFinderCannotOpenDirectory],
    WebErrorDescriptionCannotShowDirectory, 		[NSNumber numberWithInt: WebErrorCannotShowDirectory],
    WebErrorDescriptionCannotShowMIMEType, 		[NSNumber numberWithInt: WebErrorCannotShowMIMEType],
    WebErrorDescriptionCannotShowURL, 			[NSNumber numberWithInt: WebErrorCannotShowURL],
    WebErrorDescriptionCannotFindApplicationForURL, 	[NSNumber numberWithInt: WebErrorCannotNotFindApplicationForURL],
    WebErrorDescriptionWebErrorCannotFindPlugin,	[NSNumber numberWithInt: WebErrorCannotFindPlugin],
    WebErrorDescriptionWebErrorCannotLoadPlugin,	[NSNumber numberWithInt: WebErrorCannotLoadPlugin],
    WebErrorDescriptionWebErrorJavaUnavailable,		[NSNumber numberWithInt: WebErrorJavaUnavailable],
    WebErrorDescriptionDownloadDecodingFailedToComplete,[NSNumber numberWithInt: WebErrorDownloadDecodingFailedToComplete],
    WebErrorDescriptionDownloadDecodingFailedMidStream, [NSNumber numberWithInt: WebErrorDownloadDecodingFailedMidStream],
    nil];

    [WebError addErrorsWithCodesAndDescriptions:dict inDomain:WebErrorDomainWebKit];
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
    [scrollView setContentView: [[[WebClipView alloc] initWithFrame:[scrollView bounds]] autorelease]];
    [scrollView setDrawsBackground: NO];
    [scrollView setHasVerticalScroller: NO];
    [scrollView setHasHorizontalScroller: NO];
    [scrollView setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [self addSubview: scrollView];
    
    [self registerForDraggedTypes:[NSPasteboard _web_dragTypesForURL]];
    
    ++WebViewCount;
    
    return self;
}

- (void)dealloc 
{
    --WebViewCount;
    
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
- (WebController *)controller
{
    return _private->controller;
}


- (BOOL)isDocumentHTML
{
    return [[self documentView] isKindOfClass:[WebHTMLView class]];
}

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender
{
    id draggingSource = [sender draggingSource];
    if([draggingSource isKindOfClass:[WebFilePromiseDragSource class]]){
        draggingSource = [draggingSource draggingSource];
    }
    
    if([[sender draggingPasteboard] _web_bestURL] &&
       (![self documentView] || (draggingSource != [self documentView]))){
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
    NSURL *URL = [[sender draggingPasteboard] _web_bestURL];

    if (URL) {
	WebResourceRequest *request = [[WebResourceRequest alloc] initWithURL:URL];
	[[[self controller] mainFrame] loadRequest:request];
	[request release];
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

- (BOOL)becomeFirstResponder
{
    if ([self documentView]) {
        [[self window] makeFirstResponder:[self documentView]];
    }
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
	window = [[[self controller] windowOperationsDelegate] window];
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
                // This odd behavior matches some existing browsers,
                // including Windows IE
                if ([event modifierFlags] & NSShiftKeyMask) {
                    [self _goForward];
                } else {
                    [self _goBack];
                }
                callSuper = NO;
                break;
            case SpaceKey:
                if ([event modifierFlags] & NSShiftKeyMask) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollPageDown:nil];
                }
                callSuper = NO;
                break;
            case NSPageUpFunctionKey:
                [self scrollPageUp:nil];
                callSuper = NO;
                break;
            case NSPageDownFunctionKey:
                [self scrollPageDown:nil];
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
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self _scrollToTopLeft];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self scrollPageUp:nil];
                } else {
                    [self scrollLineUp:nil];
                }
                callSuper = NO;
                break;
            case NSDownArrowFunctionKey:
                if ([event modifierFlags] & NSCommandKeyMask) {
                    [self _scrollToBottomLeft];
                } else if ([event modifierFlags] & NSAlternateKeyMask) {
                    [self scrollPageDown:nil];
                } else {
                    [self scrollLineDown:nil];
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
