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
    @result Returns TRUE is the document represented in the view is HTML.
*/
- (BOOL)isDocumentHTML;

/*!
    @method setAllowsScrolling:
    @param flag
*/
- (void)setAllowsScrolling: (BOOL)flag;

/*!
    @method allowsScrolling
*/
- (BOOL)allowsScrolling;

/*!
    @method registerViewClass:forMIMEType:
    @discussion Extends the views that WebKit supports.
    The view must conform to the WebDocumentView protocol
    A view may register for a primary MIME type by excluding
    a subtype, i.e. "video/" will match the view with
    all video types.  More specific matching takes precedence
    over general matching.
    @param viewClass
    @param MIMEType
*/
+ (void)registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType;

@end
