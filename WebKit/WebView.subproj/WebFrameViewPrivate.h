/*	WebFrameViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebFrameView.h>

@class WebDynamicScrollBarsView;
@class WebView;

@interface WebFrameViewPrivate : NSObject
{
@public
    WebView *webView;
    WebDynamicScrollBarsView *frameScrollView;
    
    // These margin values are used to temporarily hold
    // the margins of a frame until we have the appropriate
    // document view type.
    int marginWidth;
    int marginHeight;
    
    NSArray *draggingTypes;
    
    BOOL inNextValidKeyView;
    BOOL hasBorder;
}

@end

@interface WebFrameView (WebPrivate)

- (WebView *)_webView;
- (void)_setDocumentView:(NSView <WebDocumentView> *)view;
- (NSView <WebDocumentView> *)_makeDocumentViewForDataSource:(WebDataSource *)dataSource;
- (void)_setWebView:(WebView *)webView;
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
+ (NSMutableDictionary *)_viewTypesAllowImageTypeOmission:(BOOL)allowImageTypeOmission;
+ (Class)_viewClassForMIMEType:(NSString *)MIMEType;
+ (BOOL)_canShowMIMETypeAsHTML:(NSString *)MIMEType;
- (BOOL)_isMainFrame;
- (NSScrollView *)_scrollView;
- (void)_setHasBorder:(BOOL)hasBorder;
- (void)_tile;
- (void)_drawBorder;
- (BOOL)_shouldDrawBorder;
- (BOOL)_firstResponderIsControl;

@end
