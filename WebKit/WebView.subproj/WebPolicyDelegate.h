/*	
        WebControllerPolicyHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Cocoa/Cocoa.h>

/*
   ============================================================================= 

   ============================================================================= 
*/

typedef enum {
    WebURLPolicyUseContentPolicy,
    WebURLPolicyOpenExternally,
    WebURLPolicyIgnore
} WebURLPolicy;

typedef enum {
    WebFileURLPolicyUseContentPolicy,
    WebFileURLPolicyOpenExternally,
    WebFileURLPolicyReveal,
    WebFileURLPolicyIgnore
} WebFileURLPolicy;

typedef enum {
    WebContentPolicyNone,
    WebContentPolicyShow,
    WebContentPolicySave,
    WebContentPolicySaveAndOpenExternally,
    WebContentPolicyIgnore
} WebContentPolicy;

@protocol WebControllerPolicyHandler <NSObject>

- (id <WebLocationChangeHandler>)provideLocationChangeHandlerForDataSource: (WebDataSource *)dataSource;

// URLPolicyForURL: is used to determine what to do BEFORE a URL is loaded, i.e.
// before it is clicked or loaded via a URL bar.  Clients can choose to handle the
// URL normally, hand the URL off to launch services, or
// ignore the URL.  The default implementation could return +defaultURLPolicyForURL:.
- (WebURLPolicy)URLPolicyForURL: (NSURL *)url;

// We may have different errors that cause the the policy to be un-implementable, i.e.
// launch services failure, etc.
- (void)unableToImplementURLPolicyForURL: (NSURL *)url error: (WebError *)error;

// Called when the response to URLPolicyForURL is WebURLPolicyUseContentPolicy and the URL is
// a file URL. This allows clients to special-case WebKit's behavior for file URLs.
- (WebFileURLPolicy)fileURLPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource isDirectory: (BOOL)isDirectory;

// Called when a WebFileURLPolicy could not be completed. This is usually caused by files not
// existing or not readable.
- (void)unableToImplementFileURLPolicy: (WebError *)error forDataSource: (WebDataSource *)dataSource;

// Sent after locationChangeStarted.
// Implementations typically call haveContentPolicy:forLocationChangeHandler: on WebController
// after determining the appropriate policy, perhaps by presenting a non-blocking dialog to the user.
- (void)requestContentPolicyForMIMEType: (NSString *)type dataSource: (WebDataSource *)dataSource;

// Sent when errors are encountered with an un-implementable policy, i.e.
// file i/o failure, launch services failure, type mismatches, etc.
- (void)unableToImplementContentPolicy: (WebError *)error forDataSource: (WebDataSource *)dataSource;

// Called when a plug-in for a certain mime type is not installed
- (void)pluginNotFoundForMIMEType:(NSString *)mime pluginPageURL:(NSURL *)url;

@end