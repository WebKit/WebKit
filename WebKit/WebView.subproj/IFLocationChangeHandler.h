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
@protocol IFLocationChangeHandler

- (BOOL)locationWillChangeTo: (NSURL *)url;

- (void)locationChangeStarted;

- (void)locationChangeCommitted;

- (void)locationChangeDone: (IFError *)error;

- (void)receivedPageTitle: (NSString *)title forDataSource: (IFWebDataSource *)dataSource;

- (void)serverRedirectTo: (NSURL *)url forDataSource: (IFWebDataSource *)dataSource;

// Called when a file download has started
- (void) downloadingWithHandler:(IFDownloadHandler *)downloadHandler;

@end
