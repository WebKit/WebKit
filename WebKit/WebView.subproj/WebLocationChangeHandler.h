/*	
        IFLocationChangeHandler.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class IFError;
@class IFWebDataSource;
@class IFDownloadHandler;

/*
   ============================================================================= 
   
    IFLocationChangeHandlers track changes to a frames location.  This includes 
    changes that may result in a download, or a resource being opened externally.
    rather than a change to the frame document.
    
    Handlers are created by IFWebController's provideLocationChangeHandlerForFrame:andURL:
    method.
    
    A location change that results in changing a frame's document will trigger the
    following messages, sent in order:
   
        - (void)locationChangeStarted;
        - (void)requestContentPolicyForMIMEType: (NSString *)type;
        - (void)locationChangeCommitted;  // Only sent for the IFContentPolicyShow policy.
        - (void)locationChangeDone: (IFError *)error;
   
   None of the IFLocationChangeHandler methods should block for any extended period
   of time.
   
   ============================================================================= 
*/

typedef enum {
	IFContentPolicyNone,
	IFContentPolicyShow,
	IFContentPolicySave,
	IFContentPolicyOpenExternally,
	IFContentPolicyIgnore
} IFContentPolicy;


@protocol IFLocationChangeHandler <NSObject>

- (void)locationChangeStarted;

// Sent after locationChangeStarted.
// Implementations typically call haveContentPolicy:forLocationChangeHandler: on IFWebController
// after determining the appropriate policy, perhaps by presenting a non-blocking dialog to the user.
- (void)requestContentPolicyForMIMEType: (NSString *)type;

- (void)locationChangeCommitted;

- (void)locationChangeDone: (IFError *)error;

- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource;

- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource;

// Sent when errors are encountered with an un-implementable policy, i.e.
// file i/o failure, launch services failure, type mismatches, etc.
- (void)unableToImplementContentPolicy: (IFError *)error;

@end
