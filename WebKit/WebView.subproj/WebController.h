/*	
        WebController.h
	Copyright 2001, 2002, Apple Computer, Inc.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebBackForwardList;
@class WebHistoryItem;
@class WebController;
@class WebControllerPrivate;
@class WebDataSource;
@class WebDownload;
@class WebError;
@class WebFrame;
@class WebResource;
@class WebPreferences;
@class WebView;

@interface WebContentTypes : NSObject
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
    @method suggestedFileExtensionForMIMEType:
    @param MIMEType The MIME type to check.
    @result The extension based on the MIME type
*/
+ (NSString *)suggestedFileExtensionForMIMEType: (NSString *)MIMEType;


@end

// These strings are keys into the element dictionary provided in
// the WebContextMenuDelegate's contextMenuItemsForElement and the WebControllerPolicyDelegate's clickPolicyForElement.

extern NSString *WebElementFrameKey;		// WebFrame of the element
extern NSString *WebElementImageAltStringKey;	// NSString of the ALT attribute of the image element
extern NSString *WebElementImageKey;		// NSImage of the image element
extern NSString *WebElementImageRectKey;	// NSValue of an NSRect, the rect of the image element
extern NSString *WebElementImageURLKey;		// NSURL of the image element
extern NSString *WebElementIsSelectedTextKey; 	// NSNumber of BOOL indicating whether the element is selected text or not 
extern NSString *WebElementLinkURLKey;		// NSURL of the link if the element is within an anchor
extern NSString *WebElementLinkTargetFrameKey;	// NSString of the target of the anchor
extern NSString *WebElementLinkTitleKey;	// NSString of the title of the anchor
extern NSString *WebElementLinkLabelKey;	// NSString of the text within the anchor

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
    WebFrame *mainFrame;
    
    webController  = [[WebController alloc] initWithView: webView];
    mainFrame = [webController mainFrame];
    [mainFrame loadRequest:request];
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
    @method initWithView:
    @abstract This method is a convenience equivalent to initWithView:view frameName:nil setName:nil
    @param view The view to use.
*/
- initWithView: (WebView *)view;

/*!
    @method initWithView:frameName:setName:
    @abstract The designated initializer for WebController.
    @discussion Initialize a WebController with the supplied parameters. This method will 
    create a main WebFrame with the view. Passing a top level frame name is useful if you
    handle a targetted frame navigation that would normally open a window in some other 
    way that still ends up creating a new WebController.
    @param view The main view to be associated with the controller.  May be nil.
    @param frameName The name to use for the top level frame. May be nil.
    @param setName The name of the controller set to which this controller will be added.  May be nil.
    @result Returns an initialized WebController.
*/
- initWithView: (WebView *)view frameName: (NSString *)frameName setName: (NSString *)name ;

/*!
    @method setWindowOperationsDelegate:
    @abstract Set the controller's WebWindowOperationsDelegate.
    @param delegate The WebWindowOperationsDelegate to set as the delegate.
*/    
- (void)setWindowOperationsDelegate: (id)delegate;

/*!
    @method windowOperationsDelegate
    @abstract Return the controller's WebWindowOperationsDelegate.
    @result The controller's WebWindowOperationsDelegate.
*/
- (id)windowOperationsDelegate;

/*!
    @method setResourceLoadDelegate:
    @abstract Set the controller's WebResourceLoadDelegate load delegate.
    @param delegate The WebResourceLoadDelegate to set as the load delegate.
*/
- (void)setResourceLoadDelegate: (id)delegate;

/*!
    @method resourceLoadDelegate
    @result Return the controller's WebResourceLoadDelegate.
*/    
- (id)resourceLoadDelegate;

/*!
    @method setDownloadDelegate:
    @abstract Set the controller's WebDownloadDelegate.
    @param delegate The WebDownloadDelegate to set as the download delegate.
*/    
- (void)setDownloadDelegate: (id)delegate;

/*!
    @method downloadDelegate
    @abstract Return the controller's WebDownloadDelegate.
    @result The controller's WebDownloadDelegate.
*/    
- (id)downloadDelegate;

/*!
    @method setContextMenuDelegate:
    @abstract Set the controller's WebContextMenuDelegate.
    @param delegate The WebContextMenuDelegate to set as the delegate.
*/    
- (void)setContextMenuDelegate: (id)delegate;

/*!
    @method contextMenuDelegate
    @abstract Return the controller's WebContextMenuDelegate.
    @result The controller's WebContextMenuDelegate.
*/    
- (id)contextMenuDelegate;

/*!
    @method setLocationChangeDelegate:
    @abstract Set the controller's WebLocationChangeDelegate delegate.
    @param delegate The WebLocationChangeDelegate to set as the delegate.
*/    
- (void)setLocationChangeDelegate: (id)delegate;

/*!
    @method locationChangeDelegate
    @abstract Return the controller's WebLocationChangeDelegate delegate.
    @result The controller's WebLocationChangeDelegate delegate.
*/    
- (id)locationChangeDelegate;

/*!
    @method setPolicyDelegate:
    @abstract Set the controller's WebControllerPolicyDelegate delegate.
    @param delegate The WebControllerPolicyDelegate to set as the delegate.
*/    
- (void)setPolicyDelegate: (id)delegate;

/*!
    @method policyDelegate
    @abstract Return the controller's WebControllerPolicyDelegate.
    @result The controller's WebControllerPolicyDelegate.
*/    
- (id)policyDelegate;

/*!
    @method mainFrame
    @abstract Return the top level frame.  
    @discussion Note that even document that are not framesets will have a
    mainFrame.
    @result The main frame.
*/    
- (WebFrame *)mainFrame;

/*!
    @method backForwardList
    @result The backforward list for this controller.
*/    
- (WebBackForwardList *)backForwardList;

/*!
    @method setUseBackForwardList:
    @abstract Enable or disable the use of a backforward list for this controller.
    @param flag Turns use of the back forward list on or off
*/    
- (void)setUsesBackForwardList: (BOOL)flag;

/*!
    @method goBack
    @abstract Go back to the previous URL in the backforward list.
    @result YES if able to go back in the backforward list, NO otherwise.
*/    
- (BOOL)goBack;

/*!
    @method goForward
    @abstract Go forward to the next URL in the backforward list.
    @result YES if able to go forward in the backforward list, NO otherwise.
*/    
- (BOOL)goForward;

/*!
    @method goBackOrForwardToItem:
    @abstract Go back or forward to an item in the backforward list.
    @result YES if able to go to the item, NO otherwise.
*/    
- (BOOL)goBackOrForwardToItem:(WebHistoryItem *)item;

/*!
    @method setTextSizeMultiplier:
    @abstract Change the size of the text rendering in views managed by this controller.
    @param multiplier A fractional percentage value, 1.0 is 100%.
*/    
- (void)setTextSizeMultiplier:(float)multiplier;

/*!
    @method textSizeMultiplier
    @result The text size multipler.
*/    
- (float)textSizeMultiplier;

/*!
    @method setApplicationNameForUserAgent:
    @abstract Set the application name. 
    @discussion This name will be used in user-agent strings
    that are chosen for best results in rendering web pages.
    @param applicationName The application name
*/
- (void)setApplicationNameForUserAgent:(NSString *)applicationName;

/*!
    @method applicationNameForUserAgent
    @result The name of the application as used in the user-agent string.
*/
- (NSString *)applicationNameForUserAgent;

/*!
    @method setCustomUserAgent:
    @abstract Set the user agent. 
    @discussion Setting this means that the controller should use this user-agent string
    instead of constructing a user-agent string for each URL. Setting it to nil
    causes the controller to construct the user-agent string for each URL
    for best results rendering web pages.
    @param userAgentString The user agent description
*/
- (void)setCustomUserAgent:(NSString *)userAgentString;

/*!
    @method customUserAgent
    @result The custom user-agent string or nil if no custom user-agent string has been set.
*/
- (NSString *)customUserAgent;

/*!
    @method userAgentForURL:
    @abstract Get the appropriate user-agent string for a particular URL.
    @param URL The URL.
    @result The user-agent string for the supplied URL.
*/
- (NSString *)userAgentForURL:(NSURL *)URL;


/*!
    @method supportsTextEncoding
    @abstract Find out if the current web page supports text encodings.
    @result YES if the document view of the current web page can
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
    Setting the text encoding name to nil makes the controller use default encoding rules.
    @param encoding The text encoding name to use to display a page or nil.
*/
- (void)setCustomTextEncodingName:(NSString *)encodingName;

/*!
    @method customTextEncodingName
    @result The custom text encoding name or nil if no custom text encoding name has been set.
*/
- (NSString *)customTextEncodingName;

/*!
    @method stringByEvaluatingJavaScriptFromString:
    @param script The text of the JavaScript.
    @result The result of the script, converted to a string, or nil for failure.
*/
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)script;

/*!
    @method setPreferences:
    @param preferences The preferences to use for the controller.
    @abstract Override the standard setting for the controller. 
*/
- (void)setPreferences: (WebPreferences *)prefs;

/*!
    @method preferences
    @result Returns the preferences used by this controller.
    @discussion This method will return [WebPreferences standardPreferences] if no
    other instance of WebPreferences has been set.
*/
- (WebPreferences *)preferences;

@end
