/*	
        WebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebDataSource;
@class WebFrame;
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
    @param frame The frame rectangle for the view
    @result An initialized WebView
*/
- (id)initWithFrame: (NSRect) frame;

/*!
    @method controller
    @abstract Returns the WebController associated with this WebView
    @result The WebView's controller
*/
- (WebFrame *)webFrame;

/*!
    @method frameScrollView
    @abstract Returns the WebView's scroll view
    @result The scrolling view used by the WebView to display its document view
*/
- (NSScrollView *)frameScrollView;

/*!
    @method documentView
    @abstract Returns the WebView's document subview
    @result The subview that renders the WebView's contents
*/
- (NSView <WebDocumentView> *)documentView;

/*!
    @method isDocumentHTML
    @abstract Returns whether the WebView's document view is rendering HTML content
    @result YES if the document represented in the view is HTML, otherwise NO.
*/
- (BOOL)isDocumentHTML;

/*!
    @method setAllowsScrolling:
    @abstract Sets whether the WebView allows its document to be scrolled
    @param flag YES to allow the document to be scrolled, NO to disallow scrolling
*/
- (void)setAllowsScrolling: (BOOL)flag;

/*!
    @method allowsScrolling
    @abstract Returns whether the WebView allows its document to be scrolled
    @result YES if the document is allowed to scroll, otherwise NO
*/
- (BOOL)allowsScrolling;

/*!
    @method registerViewClass:forMIMEType:
    @abstract Registers an NSView subclass to use to render data of the given MIME type
    @discussion Extends the views that WebKit supports.
    The view must conform to the WebDocumentView protocol
    A view may register for a primary MIME type by excluding
    a subtype, i.e. "video/" will match the view with
    all video types.  More specific matching takes precedence
    over general matching.
    @param viewClass The NSView subclass to instantiate when rendering data of the given MIME type
    @param MIMEType The MIME type for which to instantiate the given class
*/
+ (void)registerViewClass:(Class)viewClass forMIMEType:(NSString *)MIMEType;

@end
