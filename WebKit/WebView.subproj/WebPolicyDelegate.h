/*	
        WebControllerPolicyHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebPolicyPrivate;
@class WebFrame;
@class WebError;

/*!
    @enum WebPolicyAction
    @constant WebPolicyNone
    @constant WebPolicyUse
    @constant WebPolicyRevealInFinder
    @constant WebPolicySave
    @constant WebPolicyOpenURL
    @constant WebPolicySaveAndOpen
    @constant WebPolicyOpenNewWindow
    @constant WebPolicyIgnore
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
    @constant WebURLPolicyUseContentPolicy,
    @constant WebURLPolicyOpenExternally,
    @constant WebURLPolicyIgnore
*/
typedef enum {
    WebURLPolicyUseContentPolicy = WebPolicyUse,
    WebURLPolicyOpenExternally = WebPolicyOpenURL,
    WebURLPolicyIgnore = WebPolicyIgnore
} WebURLAction;

/*!
    @enum WebFileAction
    @constant WebFileURLPolicyUseContentPolicy,
    @constant WebFileURLPolicyOpenExternally,
    @constant WebFileURLPolicyRevealInFinder,
    @constant WebFileURLPolicyIgnore
*/
typedef enum {
    WebFileURLPolicyUseContentPolicy = WebPolicyUse,
    WebFileURLPolicyOpenExternally = WebPolicyOpenURL,
    WebFileURLPolicyRevealInFinder = WebPolicyRevealInFinder,
    WebFileURLPolicyIgnore = WebPolicyIgnore
} WebFileAction;

/*!
    @enum WebContentAction
    @constant WebContentPolicyNone,
    @constant WebContentPolicyShow,
    @constant WebContentPolicySave,
    @constant WebContentPolicySaveAndOpenExternally,
    @constant WebContentPolicyIgnore
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
    @constant WebClickPolicyShow,
    @constant WebClickPolicyOpenNewWindow,
    @constant WebClickPolicySave,
    @constant WebClickPolicySaveAndOpenExternally,
    @constant WebClickPolicyIgnore
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
*/
@interface WebURLPolicy : WebPolicy
/*!
    @method webPolicyWithURLAction:
    @param action
*/
+ webPolicyWithURLAction: (WebURLAction)action;
@end


/*!
    @class WebFileURLPolicy
*/
@interface WebFileURLPolicy : WebPolicy
/*!
    @method webPolicyWithFileAction:
    @param action
*/
+ webPolicyWithFileAction: (WebFileAction)action;
@end


/*!
    @class WebContentPolicy
*/
@interface WebContentPolicy : WebPolicy
/*!
    @method webPolicyWithContentAction:andPath:
    @param action
    @param thePath
*/
+ webPolicyWithContentAction: (WebContentAction)action andPath: (NSString *)thePath;
@end


/*!
    @class WebClickPolicy
*/
@interface WebClickPolicy : WebPolicy
/*!
    @method webPolicyWithClickAction:andPath:
    @param action
    @param thePath
*/
+ webPolicyWithClickAction: (WebClickAction)action URL:(NSURL *)URL andPath: (NSString *)thePath;
@end


/*!
    @protocol WebControllerPolicyHandler
*/
@protocol WebControllerPolicyHandler <NSObject>

/*!
    @method URLPolicyForURL:inFrame:
    @discussion URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
    before it is clicked or loaded via a URL bar.  Clients can choose to handle the
    URL normally, hand the URL off to launch services, or
    ignore the URL.  The default implementation could return +defaultURLPolicyForURL:.
    @param URL
    @param frame
*/
- (WebURLPolicy *)URLPolicyForURL:(NSURL *)URL inFrame:(WebFrame *)frame;

/*!
    @method contentPolicyForMIMEType:URL:inFrame:
    @discussion Sent after locationChangeStarted.
    Implementations typically call haveContentPolicy:forLocationChangeHandler: on WebController
    after determining the appropriate policy, perhaps by presenting a non-blocking dialog to the user.
    @param type
    @param URL
    @param frame
*/
- (WebContentPolicy *)contentPolicyForMIMEType: (NSString *)type URL:(NSURL *)URL inFrame:(WebFrame *)frame;

/*!
    @method fileURLPolicyForMIMEType:inFrame:isDirectory:
    @discussion Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
    a file URL. This allows clients to special-case WebKit's behavior for file URLs.
    @param type
    @param frame
    @param isDirectory
*/
- (WebFileURLPolicy *)fileURLPolicyForMIMEType: (NSString *)type inFrame:(WebFrame *)frame isDirectory: (BOOL)isDirectory;

/*!
    @method clickPolicyForElement:button:modifierMask:
    @param elementInformation
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