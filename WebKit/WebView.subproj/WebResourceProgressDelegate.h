/*
        WebResourceProgressDelegate.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/

@class WebResourceHandle;

/*!
    @protocol  WebResourceProgressHandler
    @discussion Implementors of this protocol will receive messages indicating
    data has been received for resources loaded by a data source.
*/
@protocol WebResourceProgressDelegate <NSObject>

/*!
    @method receivedProgress:forResourceHandle:fromDataSource:complete:
    @discussion A new chunk of data has been received.  This could be a partial load
    of a URL.  It may be useful to do incremental layout, although
    typically for non-base URLs this should be done after a URL (i.e. image)
    has been completely downloaded.
    @param progress
    @param resourceHandle
    @param dataSource
    @param isComplete
*/
- (void)receivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete: (BOOL)isComplete;

/*!
    @method receivedError:forResourceHandle:partialProgress:fromDataSource:
    @param error
    @param resourceHandle
    @param progress
    @param dataSource
*/
- (void)receivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;

@end


