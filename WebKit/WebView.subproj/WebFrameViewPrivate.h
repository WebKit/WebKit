/*	IFWebViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _viewPrivate in 
        NSWebPageView.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/IFWebController.h>
#import <WebKit/IFDynamicScrollBarsView.h>
#import <WebKit/IFWebView.h>

class QWidget;
class KHTMLPart;
class KHTMLView;

@interface WKWebViewPrivate : NSObject
{
    id <WKWebController>controller;
    KHTMLView *widget;
    WKDynamicScrollBarsView *frameScrollView;
    bool isFlipped;
    bool needsLayout;
}

@end

@interface WKWebView (WKPrivate)
- (void)_setController: (id <WKWebController>)controller;
- (void)_resetView;
- (KHTMLView *)_widget;
- (void)_setFrameScrollView: (WKDynamicScrollBarsView *)sv;
- (WKDynamicScrollBarsView *)_frameScrollView;
@end
