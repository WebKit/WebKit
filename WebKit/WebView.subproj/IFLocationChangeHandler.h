/*	
        IFLocationChangeHandler.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class IFError;
@class IFWebDataSource;

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
        - (void)locationChangeCommitted;  // Only sent for the IFContentPolicyShow policy.
        - (void)locationChangeDone: (IFError *)error;
   
   None of the IFLocationChangeHandler methods should block for any extended period
   of time.
   
   ============================================================================= 
*/

@protocol IFLocationChangeHandler <NSObject>

- (void)locationChangeStartedForDataSource: (IFWebDataSource *)dataSource;

- (void)locationChangeCommittedForDataSource: (IFWebDataSource *)dataSource;

- (void)locationChangeDone: (IFError *)error forDataSource: (IFWebDataSource *)dataSource;

- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource;

- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource;

@end
