/*	
        WebController.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#import <WebKit/WebLocationChangeHandler.h>
#import <WebKit/WebControllerPolicyHandler.h>

@class WebBackForwardList;
@class WebController;
@class WebControllerPrivate;
@class WebDataSource;
@class WebDownloadHandler;
@class WebError;
@class WebFrame;
@class WebLoadProgress;
@class WebResourceHandle;
@class WebView;

@protocol WebWindowContext;
@protocol WebResourceProgressHandler;
@protocol WebContextMenuHandler;


/*!
    @class WebController
    WebController manages the interaction between WebViews and WebDataSources.
    WebView and WebDataSource cannot function without a controller.  
*/
@interface WebController : NSObject
{
@private
    WebControllerPrivate *_private;
}

/*!
    @method 
*/    
+ (WebURLPolicy *)defaultURLPolicyForURL: (NSURL *)URL;

/*!
    @method canShowMIMEType:
    @param MIMEType
*/    
+ (BOOL)canShowMIMEType:(NSString *)MIMEType;

/*!
    @method canShowFile:
    @param path
*/    
+ (BOOL)canShowFile:(NSString *)path;

/*! 
    @method init
    @abstract Calls designated initializer with nil arguments.
*/
- init;

/*!
    @method initWithView:provisionalDataSource:controllerSetName:
    @abstract Designated initializer.
    @param view
    @param dataSource
    @param name
*/
- initWithView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource controllerSetName: (NSString *)name;

/*!
    @method setWindowContext:
    @param context
*/    
- (void)setWindowContext: (id<WebWindowContext>)context;

/*!
    @method windowContext
*/
- (id<WebWindowContext>)windowContext;

/*!
    @method setResourceProgressHandler:
    @param handler
*/
- (void)setResourceProgressHandler: (id<WebResourceProgressHandler>)handler;

/*!
    @method resourceProgressHandler
*/    
- (id<WebResourceProgressHandler>)resourceProgressHandler;

/*!
    @method setDownloadProgressHandler:
    @param handler
*/    
- (void)setDownloadProgressHandler: (id<WebResourceProgressHandler>)handler;

/*!
    @method downloadProgressHandler
*/    
- (id<WebResourceProgressHandler>)downloadProgressHandler;

/*!
    @method setContextMenuHandler:
    @param handler
*/    
- (void)setContextMenuHandler: (id<WebContextMenuHandler>)handler;

/*!
    @method contextMenuHandler
*/    
- (id<WebContextMenuHandler>)contextMenuHandler;

/*!
    @method setLocationChangeHandler:
    @param handler
*/    
- (void)setLocationChangeHandler:(id <WebLocationChangeHandler>)handler;

/*!
    @method locationChangeHandler
*/    
- (id <WebLocationChangeHandler>)locationChangeHandler;

/*!
    @method setPolicyHandler:
    @param handler
*/    
- (void)setPolicyHandler: (id<WebControllerPolicyHandler>)handler;

/*!
    @method policyHandler
*/    
- (id<WebControllerPolicyHandler>)policyHandler;

/*!
    @method frameNamed:
    Look for a frame named name, recursively.
    @param name
*/    
- (WebFrame *)frameNamed: (NSString *)name;

/*!
    @method mainFrame
    Return the top level frame.  Note that even document that are not framesets will have a
    mainFrame.
*/    
- (WebFrame *)mainFrame;

/*!
    @method frameForDataSource:
    Return the frame associated with the data source.  Traverses the
    frame tree to find the data source.
    @param dataSource
*/    
- (WebFrame *)frameForDataSource: (WebDataSource *)dataSource;

/*!
    @method frameForView:
    Return the frame associated with the view.  Traverses the
    frame tree to find the view. 
    @param aView
*/    
- (WebFrame *)frameForView: (WebView *)aView;

/*!
    @method stopAnimatedImages
*/    
- (void)stopAnimatedImages;

/*!
    @method startAnimatedImages
*/    
- (void)startAnimatedImages;

/*!
    @method stopAnimatedImageLooping
*/    
- (void)stopAnimatedImageLooping;

/*!
    @method startAnimatedImageLooping
*/    
- (void)startAnimatedImageLooping;

/*!
    @method backForwardList
*/    
- (WebBackForwardList *)backForwardList;

/*!
    @method setUseBackForwardList:
    @param flag turns use of the back forward list on or off
*/    
- (void)setUseBackForwardList: (BOOL)flag;

/*!
    @method useBackForwardList
*/    
- (BOOL)useBackForwardList;

/*!
    @method goBack
*/    
- (BOOL)goBack;

/*!
    @method goForward
*/    
- (BOOL)goForward;

/*!
    @method setTextSizeMultiplier:
    @param multiplier
*/    
- (void)setTextSizeMultiplier:(float)multiplier; // 1.0 is normal size

/*!
    @method textSizeMultiplier
*/    
- (float)textSizeMultiplier;

/*!
    @method setApplicationNameForUserAgent:
    Set the application name. This name will be used in user-agent strings
    that are chosen for best results in rendering web pages.
    @param applicationName the application name
*/
- (void)setApplicationNameForUserAgent:(NSString *)applicationName;
- (NSString *)applicationNameForUserAgent;

/*!
    @method setUserAgent:
    Set the user agent explicitly. Setting the user-agent string to nil means
    that WebKit should construct the best possible user-agent string for each URL
    for best results rendering web pages. Setting it to any string means
    that WebKit should use that user-agent string for all purposes until it is set
    back to nil.
    @param userAgentString the user agent description
*/
- (void)setUserAgent:(NSString *)userAgentString;
- (NSString *)userAgent;

/*!
    @method userAgentForURL:
    @abstract Get the appropriate user-agent string for a particular URL.
    @param URL
*/
- (NSString *)userAgentForURL:(NSURL *)URL;

/*!
    @method supportsTextEncoding
    @abstract Find out if the current web page supports text encodings.
*/
- (BOOL)supportsTextEncoding;

/*!
    @method setCustomTextEncoding:
    @discussion Make the page display with a different text encoding; stops any load in progress.
    The text encoding passed in overrides the normal text encoding smarts including
    what's specified in a web page's header or HTTP response.
    The text encoding automatically goes back to the default when the top level frame
    changes to a new location.
    @param encoding
*/
- (void)setCustomTextEncoding:(CFStringEncoding)encoding;

/*!
    @method resetTextEncoding
*/
- (void)resetTextEncoding;

/*!
    @method hasCustomTextEncoding
    @abstract Determine whether or not a custom text encoding is in use.
    @discussion It's an error to call customTextEncoding if hasCustomTextEncoding is NO.
*/
- (BOOL)hasCustomTextEncoding;

/*!
    @method customTextEncoding
*/
- (CFStringEncoding)customTextEncoding;

@end
