/*	WebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/WebViewPrivate.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebController.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebTextView.h>

#import <WebFoundation/WebNSDictionaryExtras.h>

@implementation WebViewPrivate

- init
{
    [super init];
    
    marginWidth = -1;
    marginHeight = -1;
    
    return self;
}

- (void)dealloc
{
    [frameScrollView release];
    [draggingTypes release];
    [super dealloc];
}

@end

@implementation WebView (WebPrivate)

- (void)_setMarginWidth: (int)w
{
    _private->marginWidth = w;
}

- (int)_marginWidth
{
    return _private->marginWidth;
}

- (void)_setMarginHeight: (int)h
{
    _private->marginHeight = h;
}

- (int)_marginHeight
{
    return _private->marginHeight;
}

- (void)_setDocumentView:(id)view
{
    [[self frameScrollView] setDocumentView: view];    
}

-(void)_makeDocumentViewForDataSource:(WebDataSource *)dataSource
{
    Class viewClass = [[[self class] _viewTypes] _web_objectForMIMEType:[dataSource contentType]];
    id documentView = viewClass ? [[viewClass alloc] init] : nil;
    [self _setDocumentView:(id<WebDocumentView>)documentView];
    [documentView release];

    [[self documentView] setDataSource:dataSource];
}

- (void)_setController: (WebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

- (WebController *)_controller
{
    return _private->controller;
}

- (NSClipView *)_contentView
{
    return [[self frameScrollView] contentView];
}

- (void)_scrollVerticallyBy: (float)delta
{
    NSPoint point;

    point = [[self _contentView] bounds].origin;
    point.y += delta;
    [[self _contentView] scrollPoint: point];
}

- (void)_scrollHorizontallyBy: (float)delta
{
    NSPoint point;

    point = [[self _contentView] bounds].origin;
    point.x += delta;
    [[self _contentView] scrollPoint: point];
}

- (void)_pageVertically: (BOOL)up
{
    float pageOverlap = [[self frameScrollView] verticalPageScroll];
    float delta = [[self _contentView] bounds].size.height;
    
    delta = (delta < pageOverlap) ? delta / 2.0 : delta - pageOverlap;

    if (up) {
        delta = -delta;
    }

    [self _scrollVerticallyBy: delta];
}

- (void)_pageHorizontally: (BOOL)left
{
    float pageOverlap = [[self frameScrollView] horizontalPageScroll];
    float delta = [[self _contentView] bounds].size.width;
    
    delta = (delta < pageOverlap) ? delta / 2.0 : delta - pageOverlap;

    if (left) {
        delta = -delta;
    }

    [self _scrollHorizontallyBy: delta];
}

- (void)_scrollLineVertically: (BOOL)up
{
    // verticalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    float delta = [[self frameScrollView] verticalLineScroll] * 4;

    if (up) {
        delta = -delta;
    }

    [self _scrollVerticallyBy: delta];
}

- (void)_scrollLineHorizontally: (BOOL)left
{
    float delta = [[self frameScrollView] horizontalLineScroll] * 4;

    if (left) {
        delta = -delta;
    }

    [self _scrollHorizontallyBy: delta];
}

- (void)_pageDown
{
    [self _pageVertically: NO];
}

- (void)_pageUp
{
    [self _pageVertically: YES];
}

- (void)_pageLeft
{
    [self _pageHorizontally: YES];
}

- (void)_pageRight
{
    [self _pageHorizontally: NO];
}

- (void)_scrollToTopLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, 0)];
}

- (void)_scrollToBottomLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, [[[self frameScrollView] documentView] bounds].size.height)];
}

- (void)_lineDown
{
    [self _scrollLineVertically: NO];
}

- (void)_lineUp
{
    [self _scrollLineVertically: YES];
}

- (void)_lineLeft
{
    [self _scrollLineHorizontally: YES];
}

- (void)_lineRight
{
    [self _scrollLineHorizontally: NO];
}

+ (NSMutableDictionary *)_viewTypes
{
    static NSMutableDictionary *viewTypes;

    if (!viewTypes) {
        viewTypes = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
            [WebHTMLView class], @"text/html",
	    [WebHTMLView class], @"text/xml",
            [WebTextView class], @"text/",
            [WebTextView class], @"application/x-javascript",
            [WebImageView class], @"image/jpeg",
            [WebImageView class], @"image/gif",
            [WebImageView class], @"image/png",
            nil];
    }
    
    return viewTypes;
}

+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    return [[self _viewTypes] _web_objectForMIMEType:MIMEType] != nil;
}

- (void)_goBack
{
    [[self _controller] goBack];
}

- (void)_goForward
{
    [[self _controller] goForward];
}

@end
