/*	
        WebControllerPolicyHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebPolicyPrivate;
@class WebFrame;
@class WebError;

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

typedef enum {
    WebURLPolicyUseContentPolicy = WebPolicyUse,
    WebURLPolicyOpenExternally = WebPolicyOpenURL,
    WebURLPolicyIgnore = WebPolicyIgnore
} WebURLAction;

typedef enum {
    WebFileURLPolicyUseContentPolicy = WebPolicyUse,
    WebFileURLPolicyOpenExternally = WebPolicyOpenURL,
    WebFileURLPolicyRevealInFinder = WebPolicyRevealInFinder,
    WebFileURLPolicyIgnore = WebPolicyIgnore
} WebFileAction;

typedef enum {
    WebContentPolicyNone = WebPolicyNone,
    WebContentPolicyShow = WebPolicyUse,
    WebContentPolicySave = WebPolicySave,
    WebContentPolicySaveAndOpenExternally = WebPolicySaveAndOpen,
    WebContentPolicyIgnore = WebPolicyIgnore
} WebContentAction;

typedef enum {
    WebClickPolicyShow = WebPolicyUse,
    WebClickPolicyOpenNewWindow = WebPolicyOpenNewWindow,
    WebClickPolicySave = WebPolicySave,
    WebClickPolicySaveAndOpenExternally = WebPolicySaveAndOpen,
    WebClickPolicyIgnore = WebPolicyIgnore
} WebClickAction;


@interface WebPolicy : NSObject
{
@private
    WebPolicyPrivate *_private;
}
- (WebPolicyAction)policyAction;
- (NSString *)path;
- (NSURL *)URL;
@end


@interface WebURLPolicy : WebPolicy
+ webPolicyWithURLAction: (WebURLAction)action;
@end

@interface WebFileURLPolicy : WebPolicy
+ webPolicyWithFileAction: (WebFileAction)action;
@end

@interface WebContentPolicy : WebPolicy
+ webPolicyWithContentAction: (WebContentAction)action andPath: (NSString *)thePath;
@end

@interface WebClickPolicy : WebPolicy
+ webPolicyWithClickAction: (WebClickAction)action URL:(NSURL *)URL andPath: (NSString *)thePath;
@end


@protocol WebControllerPolicyHandler <NSObject>

// URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
// before it is clicked or loaded via a URL bar.  Clients can choose to handle the
// URL normally, hand the URL off to launch services, or
// ignore the URL.  The default implementation could return +defaultURLPolicyForURL:.
- (WebURLPolicy *)URLPolicyForURL:(NSURL *)URL inFrame:(WebFrame *)frame;

// Sent after locationChangeStarted.
// Implementations typically call haveContentPolicy:forLocationChangeHandler: on WebController
// after determining the appropriate policy, perhaps by presenting a non-blocking dialog to the user.
- (WebContentPolicy *)contentPolicyForMIMEType: (NSString *)type URL:(NSURL *)URL inFrame:(WebFrame *)frame;

// Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
// a file URL. This allows clients to special-case WebKit's behavior for file URLs.
- (WebFileURLPolicy *)fileURLPolicyForMIMEType: (NSString *)type inFrame:(WebFrame *)frame isDirectory: (BOOL)isDirectory;

- (WebClickPolicy *)clickPolicyForElement: (NSDictionary *)elementInformation button: (NSEventType)eventType modifierMask: (unsigned int)eventMask;

// Called when a WebPolicy could not be implemented. It is up to the client to display appropriate feedback.
- (void)unableToImplementPolicy:(WebPolicy *)policy error:(WebError *)error forURL:(NSURL *)URL inFrame:(WebFrame *)frame;

// Called when a plug-in for a certain mime type is not installed
- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)URL;

@end