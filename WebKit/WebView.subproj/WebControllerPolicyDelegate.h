/*	
        WebControllerPolicyDelegate.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebPolicyPrivate;
@class WebFrame;
@class WebError;

/*!
    @enum WebPolicyAction
    @constant WebPolicyNone Unitialized state.
    @constant WebPolicyUse Have WebKit use the resource.
    @constant WebPolicyRevealInFinder Reveal the file in the Finder.
    @constant WebPolicySave Save the resource to disk.
    @constant WebPolicyOpenURL Open the URL in another application.
    @constant WebPolicySaveAndOpen Save and open the resource in another application.
    @constant WebPolicyOpenNewWindow Open the resource in another window.
    @constant WebPolicyIgnore Do nothing with the resource.
*/
typedef enum {
    WebPolicyNone,
    WebPolicyUse,
    WebPolicyRevealInFinder,
    WebPolicySave,
    WebPolicyOpenURL,
    WebPolicySaveAndOpen,
    WebPolicyOpenNewWindow,
    WebPolicyIgnore
} WebPolicyAction;

/*!
@enum WebClickAction
 @constant WebClickPolicyShow Have WebKit show the clicked URL.
 @constant WebClickPolicyOpenNewWindow Open the clicked URL in another window.
 @constant WebClickPolicySave Save the clicked URL to disk.
 @constant WebClickPolicySaveAndOpenExternally Save the clicked URL to disk and open the file in another application.
 @constant WebClickPolicyIgnore Do nothing with the clicked URL.
 */
typedef enum {
    WebClickPolicyShow = WebPolicyUse,
    WebClickPolicyOpenNewWindow = WebPolicyOpenNewWindow,
    WebClickPolicySave = WebPolicySave,
    WebClickPolicySaveAndOpenExternally = WebPolicySaveAndOpen,
    WebClickPolicyIgnore = WebPolicyIgnore
} WebClickAction;

/*!
    @enum WebURLAction
    @constant WebURLPolicyUseContentPolicy Continue processing URL, ask for content policy.
    @constant WebURLPolicyOpenExternally Open the URL in another application. 
    @constant WebURLPolicyIgnore Do nothing with the URL.
*/
typedef enum {
    WebURLPolicyUseContentPolicy = WebPolicyUse,
    WebURLPolicyOpenExternally = WebPolicyOpenURL,
    WebURLPolicyIgnore = WebPolicyIgnore
} WebURLAction;

/*!
    @enum WebFileAction
    @constant WebFileURLPolicyUseContentPolicy Continue processing the file, ask for content policy.
    @constant WebFileURLPolicyOpenExternally Open the file in another application.
    @constant WebFileURLPolicyRevealInFinder Reveal the file in the Finder.
    @constant WebFileURLPolicyIgnore Do nothing with the file.
*/
typedef enum {
    WebFileURLPolicyUseContentPolicy = WebPolicyUse,
    WebFileURLPolicyOpenExternally = WebPolicyOpenURL,
    WebFileURLPolicyRevealInFinder = WebPolicyRevealInFinder,
    WebFileURLPolicyIgnore = WebPolicyIgnore
} WebFileAction;

/*!
    @enum WebContentAction
    @constant WebContentPolicyNone Unitialized state.
    @constant WebContentPolicyShow Show the content in WebKit.
    @constant WebContentPolicySave Save the content to disk.
    @constant WebContentPolicySaveAndOpenExternally Save the content to disk and open it in another application.
    @constant WebContentPolicyIgnore Do nothing with the content.
*/
typedef enum {
    WebContentPolicyNone = WebPolicyNone,
    WebContentPolicyShow = WebPolicyUse,
    WebContentPolicySave = WebPolicySave,
    WebContentPolicySaveAndOpenExternally = WebPolicySaveAndOpen,
    WebContentPolicyIgnore = WebPolicyIgnore
} WebContentAction;


/*!
    @class WebPolicy
    Base class that describes the action that should take place when the WebControllerPolicyDelegate
    is asked for the policy for a URL, file, clicked URL or loaded content.
*/
@interface WebPolicy : NSObject
{
@private
    WebPolicyPrivate *_private;
}

/*!
    @method policyAction
    @abstract The policy action of the WebPolicy.
*/
- (WebPolicyAction)policyAction;

/*!
    @method path
    @abstract The path for the saved file.
*/
- (NSString *)path;

/*!
    @method URL
    @abstract The URL of the WebPolicy.
*/
- (NSURL *)URL;
@end


/*!
    @class WebURLPolicy
    Describes the action for a URL that WebKit has been asked to load.
*/
@interface WebURLPolicy : WebPolicy
/*!
    @method webPolicyWithURLAction:
    @abstract WebURLPolicy constructor.
    @param action The policy action of the WebURLPolicy.
*/
+ webPolicyWithURLAction: (WebURLAction)action;
@end


/*!
    @class WebFileURLPolicy
    Describes the action for a file that WebKit has been asked to load.
*/
@interface WebFileURLPolicy : WebPolicy
/*!
    @method webPolicyWithFileAction:
    @abstract WebFileURLPolicy constructor
    @param action The policy action of the WebFileURLPolicy.
*/
+ webPolicyWithFileAction: (WebFileAction)action;
@end


/*!
    @class WebContentPolicy
    Describes the action for content which has been partially loaded.
*/
@interface WebContentPolicy : WebPolicy
/*!
    @method webPolicyWithContentAction:andPath:
    @abstract WebContentPolicy constructor
    @param action The policy action of the WebContentPolicy.
    @param thePath Path to where the file should be saved. Only applicable for
    WebContentPolicySave and WebContentPolicySaveAndOpenExternally WebContentActions.
*/
+ webPolicyWithContentAction: (WebContentAction)action andPath: (NSString *)thePath;
@end


/*!
    @class WebClickPolicy
    Describes the action for content which has been partially loaded.
*/
@interface WebClickPolicy : WebPolicy
/*!
    @method webPolicyWithClickAction:andPath:
    @abstract WebClickPolicy constructor
    @param action The policy action of the WebClickPolicy.
    @param thePath Path to where the file should be saved. Only applicable for
    WebClickPolicySave and WebClickPolicySaveAndOpenExternally WebClickActions.
*/
+ webPolicyWithClickAction: (WebClickAction)action URL:(NSURL *)URL andPath: (NSString *)thePath;
@end


/*!
    @protocol WebControllerPolicyDelegate
    @discussion While loading a URL, WebKit asks the WebControllerPolicyDelegate for
    policies that determine the action of what to do with the URL or the data that
    the URL represents. Typically, the policy handler methods are called in this order:

    clickPolicyForElement:button:modifierFlags:<BR>
    URLPolicyForURL:inFrame:<BR>
    fileURLPolicyForMIMEType:inFrame:isDirectory:<BR>
    contentPolicyForMIMEType:URL:inFrame:<BR>
*/
@protocol WebControllerPolicyDelegate <NSObject>

/*!
     @method clickPolicyForElement:button:modifierFlags:
     @discussion Called right after the user clicks on a link.
     @param elementInformation Dictionary that describes the clicked element.
     @param eventType The type of event.
     @param modifierFlags The modifier flags as described in NSEvent.h.
     @result The WebClickPolicy for WebKit to implement
*/
- (WebClickPolicy *)clickPolicyForElement: (NSDictionary *)elementInformation button: (NSEventType)eventType modifierFlags: (unsigned int)modifierFlags;

/*!
    @method URLPolicyForURL:inFrame:
    @discussion URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
    before it is clicked or loaded via a URL bar.  Clients can choose to handle the
    URL normally, hand the URL off to launch services, or
    ignore the URL.  The default implementation could return +defaultURLPolicyForURL:.
    @param URL The URL that WebKit has been asked to load.
    @param frame The frame which will load the URL.
*/
- (WebURLPolicy *)URLPolicyForURL:(NSURL *)URL inFrame:(WebFrame *)frame;

/*!
     @method fileURLPolicyForMIMEType:inFrame:isDirectory:
     @discussion Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
     a file URL. This allows clients to special-case WebKit's behavior for file URLs.
     @param type MIME type for the file.
     @param frame The frame which will load the file.
     @param isDirectory YES if the file is a directory.
*/
- (WebFileURLPolicy *)fileURLPolicyForMIMEType: (NSString *)type inFrame:(WebFrame *)frame isDirectory: (BOOL)isDirectory;

/*!
    @method contentPolicyForMIMEType:URL:inFrame:
    @discussion Returns the policy for content which has been partially loaded. Sent after locationChangeStarted. 
    Implementations typically call haveContentPolicy:forLocationChangeHandler: on WebController
    after determining the appropriate policy, perhaps by presenting a non-blocking dialog to the user.
    @param type MIME type of the partially loaded content.
    @param URL URL of the partially loaded content.
    @param frame The frame which is loading the URL.
*/
- (WebContentPolicy *)contentPolicyForMIMEType: (NSString *)type URL:(NSURL *)URL inFrame:(WebFrame *)frame;

/*!
    @method unableToImplementPolicy:error:forURL:inFrame:
    @discussion Called when a WebPolicy could not be implemented. It is up to the client to display appropriate feedback.
    @param policy The policy that could not be implemented.
    @param error The error that caused the policy to not be implemented.
    @param frame The frame in the which the policy could not be implemented.
*/
- (void)unableToImplementPolicy:(WebPolicy *)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame;

/*!
    @method pluginNotFoundForMIMEType:pluginPageURL:
    @discussion Called when a plug-in for a certain mime type is not installed.
    @param mime The MIME type that no installed plug-in supports.
    @param URL The web page for the plug-in that supports the MIME type. Can be nil.
*/
- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)URL;

@end
