/*	IFWebViewPrivate.mm
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/
#import <WebKit/WebKitDebug.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFWebViewPrivate.h>
#import <WebKit/IFWebController.h>
#import <WebKit/IFHTMLView.h>
#import <WebKit/IFImageView.h>
#import <WebKit/IFTextView.h>

static NSMutableDictionary *_viewTypes=nil;

@implementation IFWebViewPrivate

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

@implementation IFWebView (IFPrivate)

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


- (void)_setController: (IFWebController *)controller
{
    // Not retained; the controller owns the view.
    _private->controller = controller;    
}

- (IFWebController *)_controller
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

- (void)_scrollToTopLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, 0)];
}

- (void)_scrollToBottomLeft
{
    [[self _contentView] scrollPoint: NSMakePoint(0, [[self _contentView] bounds].size.height)];
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
    if(!_viewTypes){
        _viewTypes = [[NSMutableDictionary dictionary] retain];
        [_viewTypes setObject:[IFHTMLView class]  forKey:@"text/html"];
        [_viewTypes setObject:[IFTextView class]  forKey:@"text/"];
        [_viewTypes setObject:[IFImageView class] forKey:@"image/jpeg"];
        [_viewTypes setObject:[IFImageView class] forKey:@"image/gif"];
        [_viewTypes setObject:[IFImageView class] forKey:@"image/png"];
    }
    return _viewTypes;
}


+ (BOOL)_canShowMIMEType:(NSString *)MIMEType
{
    NSMutableDictionary *viewTypes = [[self class] _viewTypes];
    NSArray *keys;
    unsigned i;
    
    if([viewTypes objectForKey:MIMEType]){
        return YES;
    }else{
        keys = [viewTypes allKeys];
        for(i=0; i<[keys count]; i++){
            if([[keys objectAtIndex:i] hasSuffix:@"/"] && [MIMEType hasPrefix:[keys objectAtIndex:i]]){
                if([viewTypes objectForKey:[keys objectAtIndex:i]]){
                    return YES;
                }
            }
        }
    }
    return NO;
}

@end
