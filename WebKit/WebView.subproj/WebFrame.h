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
    @method initWithName:webView:provisionalDataSource:controller:
    @abstract The designated initializer of WebFrame.
    @param name The name of the frame.
    @param view The view for the frame.
    @param dataSource The dataSource for the frame.
    @param controller The controller that manages the frame.
    @result Returns an initialized WebFrame.
*/
- initWithName: (NSString *)name webView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource controller: (WebController *)controller;

/*!
    @method name
    @result The frame name.
*/
- (NSString *)name;

/*!
    @method setController:
    @param controller
*/
- (void)setController: (WebController *)controller;

/*!
    @method controller
    @result Returns the controller of this frame.
*/
- (WebController *)controller;

/*!
    @method setWebView:
    @param view
*/
- (void)setWebView: (WebView *)view;

/*!
    @method webView
    @result The WebView for this frame.
*/
- (WebView *)webView;

/*!
    @method setProvisionalDataSource:
    @discussion Sets the frame's data source.  Note that the data source will be
    considered 'provisional' until it has been loaded, and at least
    ~some~ data has been received.
    
    @param dataSource
    @result Returns NO and will not set the provisional data source if the controller
    disallows by returning a WebURLPolicyIgnore.
*/
- (BOOL)setProvisionalDataSource: (WebDataSource *)dataSource;

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
    return nil if no data source has been set on the frame, or the data source
    has successfully transitioned to the committed data source.
    @result The provisional datasource of this frame.
*/
- (WebDataSource *)provisionalDataSource;

/*!
    @method startLoading
    @discussion If a frame has a provisional data source this method will begin
    loading data for that provisional data source.  If the frame
    has no provisional data source this method will do nothing.

    To reload an existing data source call reload.
*/
- (void)startLoading;

/*!
    @method stopLoading
    @discussion Stop any pending loads on the frame's data source,
    and it's children.
*/
- (void)stopLoading;


/*!
    @method reload
*/
- (void)reload;

/*!
    @method frameNamed:
    @discussion This method returns a frame with the given name. frameNamed returns self 
    for _self and _current, the parent frame for _parent and the main frame for _top. 
    frameNamed returns self for _parent and _top if the receiver it is the mainFrame. 
    nil is returned if a frame with the given name is not found.
    @param name The name of the frame to find.
    @result The frame matching the provided name.
*/
- (WebFrame *)frameNamed:(NSString *)name;

@end
