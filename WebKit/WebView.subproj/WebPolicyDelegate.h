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
    @constant WebContentPolicyShow Show the resource in WebKit.
    @constant WebContentPolicySave Save the resource to disk.
    @constant WebContentPolicySaveAndOpenExternally, Save the resource to disk and open it in another application.
    @constant WebContentPolicyIgnore Do nothing with the resource.
*/
typedef enum {
    WebContentPolicyNone = WebPolicyNone,
    WebContentPolicyShow = WebPolicyUse,
    WebContentPolicySave = WebPolicySave,
    WebContentPolicySaveAndOpenExternally = WebPolicySaveAndOpen,
    WebContentPolicyIgnore = WebPolicyIgnore
} WebContentAction;

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
*/
- (WebPolicyAction)policyAction;

/*!
    @method path
*/
- (NSString *)path;

/*!
    @method URL
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
    @abstract WebURLPolicy constructor
    @param action
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
    @param action
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
    @param action
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
    @param action
    @param thePath Path to where the file should be saved. Only applicable for
    WebClickPolicySave and WebClickPolicySaveAndOpenExternally WebClickActions.
*/
+ webPolicyWithClickAction: (WebClickAction)action URL:(NSURL *)URL andPath: (NSString *)thePath;
@end


/*!
    @protocol WebControllerPolicyHandler
    @discussion A controller's WebControllerPolicyHandler is asked for policies for files,
    URL's, clicked URL's and partially loaded content that WebKit has been asked to load.
*/
@protocol WebControllerPolicyDelegate <NSObject>

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
    @method fileURLPolicyForMIMEType:inFrame:isDirectory:
    @discussion Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
    a file URL. This allows clients to special-case WebKit's behavior for file URLs.
    @param type MIME type for the file.
    @param frame The frame which will load the file.
    @param isDirectory YES if the file is a directory.
*/
- (WebFileURLPolicy *)fileURLPolicyForMIMEType: (NSString *)type inFrame:(WebFrame *)frame isDirectory: (BOOL)isDirectory;

/*!
    @method clickPolicyForElement:button:modifierMask:
    @discussion Returns the policy for a clicked URL.
    @param elementInformation Dictionary that describes the clicked element.
    @param eventType
    @param eventMask
*/
- (WebClickPolicy *)clickPolicyForElement: (NSDictionary *)elementInformation button: (NSEventType)eventType modifierMask: (unsigned int)eventMask;

/*!
    @method unableToImplementPolicy:error:forURL:inFrame:
    @discussion Called when a WebPolicy could not be implemented. It is up to the client to display appropriate feedback.
    @param policy
    @param error
    @param frame
*/
- (void)unableToImplementPolicy:(WebPolicy *)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame;

/*!
    @method pluginNotFoundForMIMEType:pluginPageURL:
    @discussion Called when a plug-in for a certain mime type is not installed
    @param mime
    @param URL
*/
- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)URL;

@end