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
    a change that may result in a download rather than a change to the frame
    document.
    
    Handlers are created by IFWebController's provideLocationChangeHandlerForFrame:
    method.
    
    Handlers may veto a change by return NO from
    
        - (BOOL)locationWillChangeTo: (NSURL *)url;

    A location change that results in changing a frame's document will trigger the
    following messages, sent in order:
   
        - (void)locationChangeStarted;
        - (void)locationChangeCommitted;
        - (void)locationChangeDone: (IFError *)error;

    A location change that results in a download will trigger the following messages,
    sent in order:
    
        - (void)locationChangeStarted;
        - (void) downloadingWithHandler:(IFDownloadHandler *)downloadHandler;
   
   ============================================================================= 
*/

typedef enum {
	IFContentPolicyNone,
	IFContentPolicyShow,
	IFContentPolicySave,
	IFContentPolicyOpenExternally,
	IFContentPolicyIgnore
} IFContentPolicy;


@protocol IFLocationChangeHandler

// DEPRECATED
- (BOOL)locationWillChangeTo: (NSURL *)url;
// DEPRECATED 
- (void) downloadingWithHandler:(IFDownloadHandler *)downloadHandler;

- (void)locationChangeStarted;

- (void)locationChangeCommitted;

- (void)locationChangeDone: (IFError *)error;

- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource;

- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource;

// Sent once the IFContentType of the location handler
// has been determined.  Should not block.
// Implementations typically call haveContentPolicy:forLocationChangeHandler: immediately, although
// may call it later after showing a user dialog.
- (void)requestContentPolicyForMIMEType: (NSString *)type;

// We may have different errors that cause the the policy to be un-implementable, i.e.
// file i/o failure, launch services failure, type mismatches, etc.
- (void)unableToImplementContentPolicy: (IFError *)error;


@end
