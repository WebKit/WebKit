/*	WebFrameViewPrivate.m
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrameViewPrivate.h>

#import <WebKit/WebDataSource.h>
#import <WebKit/WebDocument.h>
#import <WebKit/WebDynamicScrollBarsView.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebImageView.h>
#import <WebKit/WebTextView.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSDictionary_NSURLExtras.h>
#import <Foundation/NSURLResponse.h>
#import <WebKit/WebAssertions.h>

@implementation WebFrameViewPrivate

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

@implementation WebFrameView (WebPrivate)

// Note that the WebVew is not retained.
- (WebView *)_webView
{
    return _private->webView;
}


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

- (void)_setDocumentView:(NSView *)view
{
    WebDynamicScrollBarsView *sv = (WebDynamicScrollBarsView *)[self _scrollView];
    
    [sv setSuppressLayout: YES];
    
    // Always start out with arrow.  New docView can then change as needed, but doesn't have to
    // clean up after the previous docView.  Also TextView will set cursor when added to view
    // tree, so must do this before setDocumentView:.
    [sv setDocumentCursor:[NSCursor arrowCursor]];

    [sv setDocumentView: view];
    [sv setSuppressLayout: NO];
}

-(NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource
{
    NSString *MIMEType = [[dataSource response] MIMEType];
    
    Class viewClass = [[self class] _viewClassForMIMEType:MIMEType];
    NSView <WebDocumentView> *documentView = viewClass ? [[viewClass alloc] init] : nil;
    [self _setDocumentView:documentView];
    [documentView release];
    
    return documentView;
}

- (void)_setWebView:(WebView *)webView
{
    // Not retained because the WebView owns the WebFrame, which owns the WebFrameView.
    _private->webView = webView;    
}

- (NSScrollView *)_scrollView
{
    return _private->frameScrollView;
}


- (NSClipView *)_contentView
{
    return [[self _scrollView] contentView];
}

- (void)_scrollVerticallyBy: (float)delta
{
    NSPoint point = [[self _contentView] bounds].origin;
    point.y += delta;
    [[self _contentView] scrollPoint: point];
}

- (void)_scrollHorizontallyBy: (float)delta
{
    NSPoint point = [[self _contentView] bounds].origin;
    point.x += delta;
    [[self _contentView] scrollPoint: point];
}

- (float)_verticalKeyboardScrollAmount
{
    // verticalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    return [[self _scrollView] verticalLineScroll] * 4;
}

- (float)_horizontalKeyboardScrollAmount
{
    // verticalLineScroll is quite small, to make scrolling from the scroll bar
    // arrows relatively smooth. But this seemed too small for scrolling with
    // the arrow keys, so we bump up the number here. Cheating? Perhaps.
    return [[self _scrollView] horizontalLineScroll] * 4;
}

- (void)_pageVertically:(BOOL)up
{
    float pageOverlap = [self _verticalKeyboardScrollAmount];
    float delta = [[self _contentView] bounds].size.height;
    
    delta = (delta < pageOverlap) ? delta / 2.0 : delta - pageOverlap;

    if (up) {
        delta = -delta;
    }

    [self _scrollVerticallyBy: delta];
}

- (void)_pageHorizontally: (BOOL)left
{
    float pageOverlap = [self _horizontalKeyboardScrollAmount];
    float delta = [[self _contentView] bounds].size.width;
    
    delta = (delta < pageOverlap) ? delta / 2.0 : delta - pageOverlap;

    if (left) {
        delta = -delta;
    }

    [self _scrollHorizontallyBy: delta];
}

- (void)_scrollLineVertically: (BOOL)up
{
    float delta = [self _verticalKeyboardScrollAmount];

    if (up) {
        delta = -delta;
    }

    [self _scrollVerticallyBy: delta];
}

- (void)_scrollLineHorizontally: (BOOL)left
{
    float delta = [self _horizontalKeyboardScrollAmount];

    if (left) {
        delta = -delta;
    }

    [self _scrollHorizontallyBy: delta];
}

- (void)scrollPageUp:(id)sender
{
    // After hitting the top, tell our parent to scroll
    float oldY = [[self _contentView] bounds].origin.y;
    [self _pageVertically:YES];
    if (oldY == [[self _contentView] bounds].origin.y) {
        [[self nextResponder] tryToPerform:@selector(scrollPageUp:) with:nil];
    }
}

- (void)scrollPageDown:(id)sender
{
    // After hitting the bottom, tell our parent to scroll
    float oldY = [[self _contentView] bounds].origin.y;
    [self _pageVertically:NO];
    if (oldY == [[self _contentView] bounds].origin.y) {
        [[self nextResponder] tryToPerform:@selector(scrollPageDown:) with:nil];
    }
}

- (void)_pageLeft
{
    [self _pageHorizontally:YES];
}

- (void)_pageRight
{
    [self _pageHorizontally:NO];
}

- (void)_scrollToTopLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, 0)];
}

- (void)_scrollToBottomLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, [[[self _scrollView] documentView] bounds].size.height)];
}

- (void)scrollLineUp:(id)sender
{
    [self _scrollLineVertically: YES];
}

- (void)scrollLineDown:(id)sender
{
    [self _scrollLineVertically: NO];
}

- (void)_lineLeft
{
    [self _scrollLineHorizontally: YES];
}

- (void)_lineRight
{
    [self _scrollLineHorizontally: NO];
}

static NSMutableDictionary *viewTypes;

+ (NSMutableDictionary *)_viewTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission
{
    static BOOL addedImageTypes;

    if (!viewTypes) {
        viewTypes = [[NSMutableDictionary alloc] initWithObjectsAndKeys:
            [WebHTMLView class], @"text/html",
	    [WebHTMLView class], @"text/xml",
	    [WebHTMLView class], @"application/xml",
	    [WebHTMLView class], @"application/xhtml+xml",
            [WebTextView class], @"text/",
            [WebTextView class], @"application/x-javascript",
            nil];
    }

    if (!addedImageTypes && !allowImageTypeOmission) {
        NSEnumerator *enumerator = [[WebImageView supportedImageMIMETypes] objectEnumerator];
        ASSERT(enumerator != nil);
        NSString *mime;
        while ((mime = [enumerator nextObject]) != nil) {
            // Don't clobber previously-registered user image types
            if ([viewTypes objectForKey:mime] == nil) {
                [viewTypes setObject:[WebImageView class] forKey:mime];
            }
        }
        addedImageTypes = YES;
    }
    
    return viewTypes;
}

+ (BOOL)_canShowMIMETypeAsHTML:(NSString *)MIMEType
{
    return ([viewTypes objectForKey:MIMEType] == [WebHTMLView class]);
}


+ (Class)_viewClassForMIMEType:(NSString *)MIMEType
{
    // Getting the image types is slow, so don't do it until we have to.
    Class c = [[self _viewTypesAllowImageTypeOmission:YES] _web_objectForMIMEType:MIMEType];
    if (c == nil) {
        c = [[self _viewTypesAllowImageTypeOmission:NO] _web_objectForMIMEType:MIMEType];
    }
    return c;
}

- (void)_goBack
{
    [_private->webView goBack];
}

- (void)_goForward
{
    [_private->webView goForward];
}

- (BOOL)_isMainFrame
{
    return [_private->webView mainFrame] == [self webFrame];
}

@end
