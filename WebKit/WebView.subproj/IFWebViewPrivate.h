/*	IFWebViewPrivate.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Private header file.  This file may reference classes (both ObjectiveC and C++)
        in WebCore.  Instances of this class are referenced by _private in 
        NSWebPageView.
*/

#import <WebKit/IFWebView.h>

class QWidget;

@class IFDynamicScrollBarsView;

@interface IFWebViewPrivate : NSObject
{
    IFWebController *controller;
    IFDynamicScrollBarsView *frameScrollView;
    
    // These margin values are used to temporarily hold
    // the margins of a frame until we have the appropriate
    // document view type.
    int marginWidth;
    int marginHeight;
    
    NSArray *draggingTypes;
}

@end

@interface IFWebView (IFPrivate)
- (void)_setDocumentView:(id <IFDocumentLoading>)view;
- (void)_setController: (IFWebController *)controller;
- (int)_marginWidth;
- (int)_marginHeight;
+ (NSMutableDictionary *)_viewTypes;
+ (BOOL)_canShowMIMEType:(NSString *)MIMEType;
@end
