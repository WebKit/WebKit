/*	
        WebLocationChangeHandler.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

@class WebError;
@class WebDataSource;
@class WebFrame;

/*
   ============================================================================= 
   
    WebLocationChangeHandlers track changes to a frame's location.  This includes 
    changes that may result in a download, or a resource being opened externally.
    rather than a change to the frame document.
    
    Handlers are created by WebController's provideLocationChangeHandlerForFrame:andURL:
    method.
    
    A location change that results in changing a frame's document will trigger the
    following messages, sent in order:
   
        - (void)locationChangeStartedForDataSource:(WebDataSource *)dataSource;
        - (void)locationChangeCommittedForDataSource:(WebDataSource *)dataSource;
            // Only sent for the WebContentPolicyShow policy.
        - (void)locationChangeDone:(WebError *)error forDataSource:(WebDataSource *)dataSource;
   
   None of the WebLocationChangeHandler methods should block for any extended period
   of time.
   
   ============================================================================= 
*/

@protocol WebLocationChangeHandler <NSObject>

- (void)locationChangeStartedForDataSource:(WebDataSource *)dataSource;
- (void)locationChangeCommittedForDataSource:(WebDataSource *)dataSource;
- (void)locationChangeDone:(WebError *)error forDataSource:(WebDataSource *)dataSource;

- (void)receivedPageTitle:(NSString *)title forDataSource:(WebDataSource *)dataSource;
- (void)receivedPageIcon:(NSImage *)image fromURL:(NSURL *)iconURL forDataSource:(WebDataSource *)dataSource;

- (void)serverRedirectTo:(NSURL *)URL forDataSource:(WebDataSource *)dataSource;

- (void)clientRedirectTo:(NSURL *)URL delay:(NSTimeInterval)seconds fireDate:(NSDate *)date forFrame:(WebFrame *)frame;
- (void)clientRedirectCancelledForFrame:(WebFrame *)frame;

@end

// The WebLocationChangeHandler class responds to all WebLocationChangeHandler protocol
// methods by doing nothing. It's provided for the convenience of clients who only want
// to implement some of the above methods and ignore others.

@interface WebLocationChangeHandler : NSObject <WebLocationChangeHandler>
{
}
@end
