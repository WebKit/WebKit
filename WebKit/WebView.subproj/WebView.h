/*	
        WebController.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebBackForwardList;
@class WebController;
@class WebControllerPrivate;
@class WebDataSource;
@class WebDownloadHandler;
@class WebError;
@class WebFrame;
@class WebResourceHandle;
@class WebView;

@protocol WebContextMenuDelegate;
@protocol WebControllerPolicyDelegate;
@protocol WebLocationChangeDelegate;
@protocol WebResourceLoadDelegate;
@protocol WebWindowOperationsDelegate;

// These strings are keys into the element dictionary provided in
// the WebContextMenuDelegate's contextMenuItemsForElement and the WebControllerPolicyDelegate's clickPolicyForElement.
extern NSString *WebElementLinkURLKey;
extern NSString *WebElementLinkTargetFrameKey;
extern NSString *WebElementLinkLabelKey;
extern NSString *WebElementImageURLKey;
extern NSString *WebElementStringKey;
extern NSString *WebElementImageKey;
extern NSString *WebElementImageLocationKey;
extern NSString *WebElementFrameKey;

/*!
    @class WebController
    WebController manages the interaction between WebViews and WebDataSources.  Modification
    of the policies and behavior of the WebKit is largely managed by WebControllers and their
    delegates.
    
    <p>
    Typical usage:
    </p>
    <pre>
    WebController *webController;
    WebDataSource *dataSource;
    WebFrame *mainFrame;
    
    webController  = [[WebController alloc] initWithView: webView provisionalDataSource: nil];
    dataSource = [[[WebDataSource alloc] initWithURL:url] autorelease];
    mainFrame = [webController mainFrame];
    
    if([mainFrame setProvisionalDataSource: dataSource]){
        [mainFrame startLoading];
    }
    </pre>
    
    WebControllers have the following delegates:  WebWindowOperationsDelegate,
    WebResourceLoadDelegate, WebContextMenuDelegate, WebLocationChangeDelegate,
    and WebControllerPolicyDelegate.
    
    WebKit depends on the WebController's WebWindowOperationsDelegate for all window
    related management, including opening new windows and controlling the user interface
    elements in those windows.
    
    WebResourceLoadDelegate is used to monitor the progress of resources as they are
    loaded.  This delegate may be used to present users with a progress monitor.
    
    WebController's WebContextMenuDelegate can customize the context menus that appear
    over content managed by the WebKit.
    
    The WebLocationChangeDelegate receives messages when the URL in a WebFrame is
    changed.
    
    WebController's WebControllerPolicyDelegate can make determinations about how
    content should be handled, based on the resource's URL and MIME type.
*/
@interface WebController : NSObject
{
@private
    WebControllerPrivate *_private;
}

/*!
    @method canShowMIMEType:
    @abstract Checks if the WebKit can show content of a certain MIME type.
    @param MIMEType The MIME type to check.
    @result YES if the WebKit can show content with MIMEtype.
*/    
+ (BOOL)canShowMIMEType:(NSString *)MIMEType;

/*!
    @method canShowFile:
    @abstract Checks if the WebKit can show the content of the file at the specified path.
    @param path The path of the file to check
    @result YES if the WebKit can show the content of the file at the specified path.
*/    
+ (BOOL)canShowFile:(NSString *)path;

/*!
    @method initWithView:provisionalDataSource:controllerSetName:
    @abstract The designated initializer for WebController.
    @discussion Initialize a WebController with the supplied parameters.  This method
    will create a main WebFrame with the view and datasource.  The frame will be
    named "_top".
    @param view The main view to be associated with the controller.  May be nil.
    @param dataSource  The main datasource to be associated with the controller.  May be nil.
    @param name The name of the controller set to which this controller will be added.  May be nil.
    @result Returns an initialized WebController.
*/
- initWithView: (WebView *)view provisionalDataSource: (WebDataSource *)dataSource controllerSetName: (NSString *)name;

/*!
    @method setWindowOperationsDelegate:
    @abstract Set the controller's WebWindowOperationsDelegate.
    @param delegate The WebWindowOperationsDelegate to set as the delegate.
*/    
- (void)setWindowOperationsDelegate: (id<WebWindowOperationsDelegate>)delegate;

/*!
    @method windowOperationsDelegate
    @result Return the controller's WebWindowOperationsDelegate.
*/
- (id<WebWindowOperationsDelegate>)windowOperationsDelegate;

/*!
    @method setResourceLoadDelegate:
    @abstract Set the controller's WebResourceLoadDelegate.
    @param delegate The WebResourceLoadDelegate to set as the delegate.
*/
- (void)setResourceLoadDelegate: (id<WebResourceLoadDelegate>)delegate;

/*!
    @method resourceLoadDelegate
    @result Return the controller's WebResourceLoadDelegate.
*/    
- (id<WebResourceLoadDelegate>)resourceLoadDelegate;

/*!
    @method setDownloadDelegate:
    @abstract Set the controller's WebResourceLoadDelegate download delegate.
    @param delegate The WebResourceLoadDelegate to set as the download delegate.
*/    
- (void)setDownloadDelegate: (id<WebResourceLoadDelegate>)delegate;

/*!
    @method downloadDelegate
    @result Return the controller's WebResourceLoadDelegate download delegate.
*/    
- (id<WebResourceLoadDelegate>)downloadDelegate;

/*!
    @method setContextMenuDelegate:
    @abstract Set the controller's WebContextMenuDelegate download delegate.
    @param delegate The WebContextMenuDelegate to set as the download delegate.
*/    
- (void)setContextMenuDelegate: (id<WebContextMenuDelegate>)delegate;

/*!
    @method contextMenuDelegate
    @result Return the controller's WebContextMenuDelegate.
*/    
- (id<WebContextMenuDelegate>)contextMenuDelegate;

/*!
    @method setLocationChangeDelegate:
    @abstract Set the controller's WebLocationChangeDelegate delegate.
    @param delegate The WebLocationChangeDelegate to set as the delegate.
*/    
- (void)setLocationChangeDelegate:(id <WebLocationChangeDelegate>)delegate;

/*!
    @method locationChangeDelegate
    @result Return the controller's WebLocationChangeDelegate.
*/    
- (id <WebLocationChangeDelegate>)locationChangeDelegate;

/*!
    @method setPolicyDelegate:
    @abstract Set the controller's WebControllerPolicyDelegate delegate.
    @param delegate The WebControllerPolicyDelegate to set as the delegate.
*/    
- (void)setPolicyDelegate: (id<WebControllerPolicyDelegate>)delegate;

/*!
    @method policyDelegate
    @result Return the controller's WebControllerPolicyDelegate.
*/    
- (id<WebControllerPolicyDelegate>)policyDelegate;

/*!
    @method frameNamed:
    @abstract Look for a frame named name, recursively from the main frame.
    @param name The name of the frame to find.
    @result Returns the frame with the supplied name, or nil if not found.
*/    
- (WebFrame *)frameNamed: (NSString *)name;

/*!
    @method mainFrame
    @abstract Return the top level frame.  
    @discussion Note that even document that are not framesets will have a
    mainFrame.
    @result The main frame.
*/    
- (WebFrame *)mainFrame;

/*!
    @method frameForDataSource:
    @abstract Return the frame associated with the data source.  
    @disucssion Traverses the frame tree to find the frame associated
    with a datasource.
    @param datasource The datasource to  match against each frame.
    @result The frame that has the associated datasource.
*/    
- (WebFrame *)frameForDataSource: (WebDataSource *)dataSource;

/*!
    @method frameForView:
    @abstract Return the frame associated with the view.  
    @discussion Traverses the frame tree to find the view. 
    @param aView The view to match against each frame.
    @result The frame that has the associated view.
*/    
- (WebFrame *)frameForView: (WebView *)aView;

/*!
    @method backForwardList
    @result The backforward list for this controller.
*/    
- (WebBackForwardList *)backForwardList;

/*!
    @method setUseBackForwardList:
    @abstract Enable or disable the use of a backforward list for this controller.
    @param flag turns use of the back forward list on or off
*/    
- (void)setUseBackForwardList: (BOOL)flag;

/*!
    @method useBackForwardList
    @result Returns YES if a backforward list is being used by this controller, NO otherwise.
*/    
- (BOOL)useBackForwardList;

/*!
    @method goBack
    @abstract Go back to the last URL in the backforward list.
    @result Returns YES if able to go back in the backforward list, NO otherwise.
*/    
- (BOOL)goBack;

/*!
    @method goForward
    @abstract Go forward to the next URL in the backforward list.
    @result Returns YES if able to go forward in the backforward list, NO otherwise.
*/    
- (BOOL)goForward;

/*!
    @method setTextSizeMultiplier:
    @abstract Change the size of the text rendering in views managed by this controller.
    @param multiplier A fractional percentage value, 1.0 is 100%.
*/    
- (void)setTextSizeMultiplier:(float)multiplier;

/*!
    @method textSizeMultiplier
    @result Returns the text size multipler.
*/    
- (float)textSizeMultiplier;

/*!
    @method setApplicationNameForUserAgent:
    @abstract Set the application name. 
    @discussion This name will be used in user-agent strings
    that are chosen for best results in rendering web pages.
    @param applicationName the application name
*/
- (void)setApplicationNameForUserAgent:(NSString *)applicationName;

/*!
    @method applicationNameForUserAgent
    @result Returns the name of the application as used in the user-agent string.
*/
- (NSString *)applicationNameForUserAgent;

/*!
    @method setUserAgent:
    @abstract Set the user agent. 
    @discussion Setting this means that the controller should use this user-agent string
    instead of constructing a user-agent string for each URL.
    @param userAgentString the user agent description
*/
- (void)setCustomUserAgent:(NSString *)userAgentString;

/*!
    @method resetUserAgent
    @abstract Reset the user agent. 
    @discussion Causes the controller to construct the user-agent string for each URL
    for best results rendering web pages.
    @param userAgentString the user agent description
*/
- (void)resetUserAgent;

/*!
    @method hasCustomUserAgent
    @abstract Determine whether or not a custom user-agent string is in use.
    @discussion It's an error to call customUserAgent if hasCustomUserAgent is NO.
    @reselt Returns YES if a custom encoding has been set, NO otherwise.
*/
- (BOOL)hasCustomUserAgent;

/*!
    @method customUserAgent
    @result customUserAgent Returns the custom user-agent string. Should only be called
    if hasCustomUserAgent returns YES.
*/
- (NSString *)customUserAgent;

/*!
    @method userAgentForURL:
    @abstract Get the appropriate user-agent string for a particular URL.
    @param URL The URL.
    @result Returns the user-agent string for the supplied URL.
*/
- (NSString *)userAgentForURL:(NSURL *)URL;

/*!
    @method supportsTextEncoding
    @abstract Find out if the current web page supports text encodings.
    @result Returns YES if the document view of the current web page can
    support different text encodings.
*/
- (BOOL)supportsTextEncoding;

/*!
    @method setCustomTextEncodingName:
    @discussion Make the page display with a different text encoding; stops any load in progress.
    The text encoding passed in overrides the normal text encoding smarts including
    what's specified in a web page's header or HTTP response.
    The text encoding automatically goes back to the default when the top level frame
    changes to a new location.
    @param encoding
*/
- (void)setCustomTextEncodingName:(NSString *)encoding;

/*!
    @method resetTextEncoding
    @abstract Remove any custom encodings that have been applied and use the default encoding.
*/
- (void)resetTextEncoding;

/*!
    @method hasCustomTextEncoding
    @abstract Determine whether or not a custom text encoding is in use.
    @discussion It's an error to call customTextEncoding if hasCustomTextEncoding is NO.
    @reselt Returns YES if a custom encoding has been set, NO otherwise.
*/
- (BOOL)hasCustomTextEncoding;

/*!
    @method customTextEncoding
    @result Returns the custom text encoding.
*/
- (NSString *)customTextEncodingName;

/*!
    @method stringByEvaluatingJavaScriptFromString:
    @param script The text of the JavaScript.
    @result Returns the result of the script, converted to a string, or nil for failure.
*/
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script;

@end
