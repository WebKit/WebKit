/*	IFWebViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/IFWebView.h>

class QWidget;


@interface IFWebViewPrivate : NSObject
{
    IFWebController *controller;
    id documentView;
    NSScrollView *frameScrollView;
    
    // These margin values are used to temporarily hold
    // the margins of a frame until we have the appropriate
    // document view type.
    int marginWidth;
    int marginHeight;
    
    BOOL allowsScrolling;
}

@end

@interface IFWebView (IFPrivate)
- (void)_setDocumentView:(id <IFDocumentLoading>)view;
- (void)_setController: (IFWebController *)controller;
- (void)_setFrameScrollView: (NSScrollView *)sv;
- (NSScrollView *)_frameScrollView;
- (void)_setupScrollers;
- (int)_marginWidth;
- (int)_marginHeight;
+ (NSMutableDictionary *)_viewTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
@end
