/*	WebViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/WebView.h>

@class WebDynamicScrollBarsView;

@interface WebViewPrivate : NSObject
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

@interface WebView (WebPrivate)
- (void)_setDocumentView:(id <WebDocumentLoading>)view;
- (void)_setController:(WebController *)controller;
- (WebController *)_controller;
- (int)_marginWidth;
- (int)_marginHeight;
- (void)_setMarginWidth:(int)w;
- (void)_setMarginHeight:(int)h;
- (NSClipView *)_contentView;
- (void)_scrollLineVertically: (BOOL)up;
- (void)_scrollLineHorizontally: (BOOL)left;
- (void)_pageDown;
- (void)_pageUp;
- (void)_pageLeft;
- (void)_pageRight;
- (void)_scrollToTopLeft;
- (void)_scrollToBottomLeft;
- (void)_lineDown;
- (void)_lineUp;
- (void)_lineLeft;
- (void)_lineRight;
- (void)_goBack;
+ (NSMutableDictionary *)_viewTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
@end
