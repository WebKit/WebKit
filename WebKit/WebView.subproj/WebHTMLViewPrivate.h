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

@class IFWebCoreBridge;

@interface IFHTMLViewPrivate : NSObject
{
    IFWebController *controller;
    KHTMLView *widget;
    BOOL widgetOwned;
    KHTMLView *provisionalWidget;
    BOOL needsLayout;
    BOOL needsToApplyStyles;
    BOOL canDragTo;
    BOOL canDragFrom;
    NSCursor *cursor;
    BOOL liveAllowsScrolling;
    BOOL inWindow;
}

@end

@interface IFHTMLView (IFPrivate)
- (void)_reset;

- (void)_setController: (IFWebController *)controller;

- (KHTMLView *)_widget;
- (KHTMLView *)_provisionalWidget;
- (IFWebCoreBridge *)_bridge;
- (void)_adjustFrames;
@end
