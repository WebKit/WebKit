/*	
        WebFrame.h
	    
	    Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebController;
@class WebDataSource;
@class WebError;
@class WebFramePrivate;
@class WebView;
@class WebRequest;

/*!
    @class WebFrame
    @discussion Every web page is represented by at least one WebFrame.  A WebFrame
    has a WebView and a WebDataSource.
*/
@interface WebFrame : NSObject
{
@private
    WebFramePrivate *_private;
}

/*!
    @method initWithName:webView:controller:
    @abstract The designated initializer of WebFrame.
    @param name The name of the frame.
    @param view The view for the frame.
    @param controller The controller that manages the frame.
    @result Returns an initialized WebFrame.
*/
- (id)initWithName: (NSString *)name webView: (WebView *)view controller: (WebController *)controller;

/*!
    @method name
    @result The frame name.
*/
- (NSString *)name;

/*!
    @method controller
    @result Returns the controller of this frame.
*/
- (WebController *)controller;

/*!
    @method webView
    @result The WebView for this frame.
*/
- (WebView *)webView;

/*!
    @method loadRequest:
    @param request The web request to load.
*/
-(void)loadRequest:(WebRequest *)request;

/*!
    @method dataSource
    @discussion Returns the committed data source.  Will return nil if the
    provisional data source hasn't yet been loaded.
    @result The datasource for this frame.
*/
- (WebDataSource *)dataSource;

/*!
    @method provisionalDataSource
    @discussion Will return the provisional data source.  The provisional data source will
    be nil if no data source has been set on the frame, or the data source
    has successfully transitioned to the committed data source.
    @result The provisional datasource of this frame.
*/
- (WebDataSource *)provisionalDataSource;

/*!
    @method stopLoading
    @discussion Stop any pending loads on the frame's data source,
    and its children.
*/
- (void)stopLoading;

/*!
    @method reload
*/
- (void)reload;

/*!
    @method findFrameNamed:
    @discussion This method returns a frame with the given name. findFrameNamed returns self 
    for _self and _current, the parent frame for _parent and the main frame for _top. 
    findFrameNamed returns self for _parent and _top if the receiver is the mainFrame.
    findFrameNamed first searches from the current frame to all descending frames then the
    rest of the frames in the controller. If still not found, findFrameNamed searches the
    frames of the other controllers.
    @param name The name of the frame to find.
    @result The frame matching the provided name. nil if the frame is not found.
*/
- (WebFrame *)findFrameNamed:(NSString *)name;

/*!
    @method parent
    @result The frame containing this frame, or nil if this is a top level frame.
*/
- (WebFrame *)parent;

/*!
    @method children
    @discussion The frames in the array are associated with a frame set or iframe.
    @result Returns an array of WebFrame.
*/
- (NSArray *)children;

/*!
    @method registerViewClass:representationClass:forMIMEType:
    @discussion Register classes that implement WebDocumentView and WebDocumentRepresentation respectively.
    A document class may register for a primary MIME type by excluding
    a subtype, i.e. "video/" will match the document class with
    all video types.  More specific matching takes precedence
    over general matching.
    @param viewClass The WebDocumentView class to use to render data for a given MIME type.
    @param representationClass The WebDocumentRepresentation class to use to represent data of the given MIME type.
    @param MIMEType The MIME type to represent with an object of the given class.
*/
+ (void) registerViewClass:(Class)viewClass representationClass: (Class)representationClass forMIMEType:(NSString *)MIMEType;

@end
