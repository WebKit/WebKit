/*	WebFrameViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebView.h>

@class WebDynamicScrollBarsView;
@class WebController;

@interface WebFrameViewPrivate : NSObject
{
@public
    WebController *controller;
    WebDynamicScrollBarsView *frameScrollView;
    
    // These margin values are used to temporarily hold
    // the margins of a frame until we have the appropriate
    // document view type.
    int marginWidth;
    int marginHeight;
    
    NSArray *draggingTypes;
}

@end

@interface WebFrameView (WebPrivate)
- (WebController *)_controller;
- (void)_setDocumentView:(NSView <WebDocumentView> *)view;
- (NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource;
- (void)_setController:(WebController *)controller;
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
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
- (BOOL)_isMainFrame;
- (void)_reregisterDraggedTypes;
@end
