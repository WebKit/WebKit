/*	IFHTMLViewPrivate.h
	Copyright 2002, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/IFHTMLView.h>

class QWidget;
class KHTMLPart;
class KHTMLView;

@interface IFHTMLViewPrivate : NSObject
{
    IFWebController *controller;
    KHTMLView *widget;
    KHTMLView *provisionalWidget;
    NSScrollView *frameScrollView;
    bool isFlipped;
    bool needsLayout;
    bool needsToApplyStyles;
    bool canDragTo;
    bool canDragFrom;
    NSArray *draggingTypes;
    NSCursor *cursor;
}

@end

@interface IFHTMLView (IFPrivate)
- (void)_setController: (IFWebController *)controller;
- (void)_resetWidget;
- (KHTMLView *)_widget;
- (KHTMLView *)_provisionalWidget;
- (void)_stopPlugins;
- (void)_removeSubviews;
- (void)_setFrameScrollView: (NSScrollView *)sv;
- (NSScrollView *)_frameScrollView;
- (void)_setupScrollers;
@end
