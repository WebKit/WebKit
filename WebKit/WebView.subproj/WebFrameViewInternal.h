/*	WebFrameViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrameView.h>

@class WebDynamicScrollBarsView;
@class WebView;

@interface WebFrameViewPrivate : NSObject
{
@public
    WebView *controller;
    WebDynamicScrollBarsView *frameScrollView;
    
    // These margin values are used to temporarily hold
    // the margins of a frame until we have the appropriate
    // document view type.
    int marginWidth;
    int marginHeight;
    
    NSArray *draggingTypes;
    
    BOOL inNextValidKeyView;
}

@end

@interface WebFrameView (WebPrivate)
- (WebView *)_controller;
- (void)_setDocumentView:(NSView <WebDocumentView> *)view;
- (NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource;
- (void)_setController:(WebView *)controller;
- (int)_marginWidth;
- (int)_marginHeight;
- (void)_setMarginWidth:(int)w;
- (void)_setMarginHeight:(int)h;
- (NSClipView *)_contentView;
- (void)_scrollLineVertically: (BOOL)up;
- (void)_scrollLineHorizontally: (BOOL)left;
- (void)_pageLeft;
- (void)_pageRight;
- (void)_scrollToTopLeft;
- (void)_scrollToBottomLeft;
- (void)_lineLeft;
- (void)_lineRight;
- (void)_goBack;
- (void)_goForward;
+ (NSMutableDictionary *)_viewTypes;
+ (Class)_viewClassForMIMEType:(NSString *)MIMEType;
- (BOOL)_isMainFrame;
@end
