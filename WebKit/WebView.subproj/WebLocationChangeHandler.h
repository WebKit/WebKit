/*	
        WebLocationChangeHandler.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebError;
@class WebDataSource;

/*
   ============================================================================= 
   
    WebLocationChangeHandlers track changes to a frames location.  This includes 
    changes that may result in a download, or a resource being opened externally.
    rather than a change to the frame document.
    
    Handlers are created by WebController's provideLocationChangeHandlerForFrame:andURL:
    method.
    
    A location change that results in changing a frame's document will trigger the
    following messages, sent in order:
   
        - (void)locationChangeStarted;
        - (void)locationChangeCommitted;  // Only sent for the WebContentPolicyShow policy.
        - (void)locationChangeDone: (WebError *)error;
   
   None of the WebLocationChangeHandler methods should block for any extended period
   of time.
   
   ============================================================================= 
*/

@protocol WebLocationChangeHandler <NSObject>

- (void)locationChangeStartedForDataSource: (WebDataSource *)dataSource;

- (void)locationChangeCommittedForDataSource: (WebDataSource *)dataSource;

- (void)locationChangeDone: (WebError *)error forDataSource: (WebDataSource *)dataSource;

- (void)receivedPageTitle: (NSString *)title forDataSource: (WebDataSource *)dataSource;

- (void)serverRedirectTo: (NSURL *)url forDataSource: (WebDataSource *)dataSource;

@end
