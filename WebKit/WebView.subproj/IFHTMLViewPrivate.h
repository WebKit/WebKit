/*	IFHTMLViewPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        IFHTMLView.
*/

#import <WebKit/IFHTMLView.h>

#ifdef __cplusplus
class KHTMLView;
#else
@class KHTMLView;
#endif

@interface IFHTMLViewPrivate : NSObject
{
    IFWebController *controller;
    KHTMLView *widget;
    KHTMLView *provisionalWidget;
    BOOL isFlipped;
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL canDragTo;
    BOOL canDragFrom;
    NSCursor *cursor;
    BOOL liveAllowsScrolling;
}

@end

@interface IFHTMLView (IFPrivate)
- (void)_setController: (IFWebController *)controller;
- (void)_resetWidget;
- (KHTMLView *)_widget;
- (KHTMLView *)_provisionalWidget;
- (void)_stopPlugins;
- (void)_removeSubviews;
@end
