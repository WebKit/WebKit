/*	
        WebControllerPolicyHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

@class WebPolicyPrivate;
@class WebDataSource;
@class WebError;

typedef enum {
    WebPolicyNone,
    WebPolicyUse,
    WebPolicyRevealInFinder,
    WebPolicySave,
    WebPolicyOpenURL,
    WebPolicySaveAndOpen,
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
    WebClickPolicyNone = WebPolicyNone,
    WebClickPolicyShow = WebPolicyUse,
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
+ webPolicyWithClickAction: (WebClickAction)action andPath: (NSString *)thePath;
@end


@protocol WebControllerPolicyHandler <NSObject>

// URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
// before it is clicked or loaded via a URL bar.  Clients can choose to handle the
// URL normally, hand the URL off to launch services, or
// ignore the URL.  The default implementation could return +defaultURLPolicyForURL:.
- (WebURLPolicy *)URLPolicyForURL: (NSURL *)url;

// Sent after locationChangeStarted.
// Implementations typically call haveContentPolicy:forLocationChangeHandler: on WebController
// after determining the appropriate policy, perhaps by presenting a non-blocking dialog to the user.
- (WebContentPolicy *)contentPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource;

// Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
// a file URL. This allows clients to special-case WebKit's behavior for file URLs.
- (WebFileURLPolicy *)fileURLPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource isDirectory: (BOOL)isDirectory;

- (WebClickPolicy *)clickPolicyForElement: (NSDictionary *)elementInformation button: (NSEventType)eventType modifierMask: (unsigned int)eventMask;

// We may have different errors that cause the the policy to be un-implementable, i.e.
// launch services failure, etc.
- (void)unableToImplementURLPolicy: (WebPolicy *)policy error: (WebError *)error forURL: (NSURL *)url;

// Called when a WebFileURLPolicy could not be completed. This is usually caused by files not
// existing or not readable.
- (void)unableToImplementFileURLPolicy: (WebPolicy *)policy error: (WebError *)error forDataSource: (WebDataSource *)dataSource;


// Sent when errors are encountered with an un-implementable policy, i.e.
// file i/o failure, launch services failure, type mismatches, etc.
- (void)unableToImplementContentPolicy: (WebPolicy *)policy error: (WebError *)error forDataSource: (WebDataSource *)dataSource;

// Called when a plug-in for a certain mime type is not installed
- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)url;

@end