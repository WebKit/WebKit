/*	
        WebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@class WebController;
@class WebViewPrivate;

@protocol WebDocumentView;

/*!
    @class WebView
*/
@interface WebView : NSView
{
@private
    WebViewPrivate *_private;
}

/*!
    @method initWithFrame:
    @param frame
*/
- initWithFrame: (NSRect) frame;

/*!
    @method controller
    @discussion Note that the controller is not retained.
*/
- (WebController *)controller;

/*!
    @method frameScrollView
*/
- frameScrollView;

/*!
    @method documentView
*/
- (NSView <WebDocumentView> *)documentView;

/*!
    @method isDocumentHTML
*/
- (BOOL)isDocumentHTML;

/*!
    @method setAllowsScroling:
    @param flag
*/
- (void)setAllowsScrolling: (BOOL)flag;

/*!
    @method allowsScrolling
*/
- (BOOL)allowsScrolling;

/*!
    @method registerViewClass:forMIMEType:
    @discussion Extends the views that WebKit supports
    The view must conform to the WebDocumentView protocol
    @param viewClass
    @param MIMEType
*/
+ (void)registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType;

@end
