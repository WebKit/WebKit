/*
        WebResourceProgressDelegate.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/

@class WebResourceHandle;

/*!
    @protocol  WebResourceProgressDelegate
    @discussion Implementors of this protocol will receive messages indicating
    data has been received for resources loaded by a data source.
*/
@protocol WebResourceProgressDelegate <NSObject>

/*!
    @method receivedProgress:forResourceHandle:fromDataSource:complete:
    @abstract Notify that a new chunk of data has been received for a resource.
    @param progress An object representing the amount of progress so far
    @param resourceHandle The resource handle loading the resource
    @param dataSource The data source containing the resource
    @param isComplete YES if the load is now complete, otherwise NO
    @discussion A new chunk of data has been received.  This could be a partial load
    of a URL. It may be useful to do incremental layout, although
    typically for non-base URLs this should be done after a URL (i.e. image)
    has been completely downloaded.
*/
- (void)receivedProgress: (WebLoadProgress *)progress forResourceHandle: (WebResourceHandle *)resourceHandle fromDataSource: (WebDataSource *)dataSource complete: (BOOL)isComplete;

/*!
    @method receivedError:forResourceHandle:partialProgress:fromDataSource:
    @abstract Notify that an error took place loading a resource.
    @param error An object representing the error that occurred
    @param resourceHandle The resource handle that was loading the resource
    @param progress The amoung of progress made before the error
    @param dataSource The data source responsible for the resource
*/
- (void)receivedError: (WebError *)error forResourceHandle: (WebResourceHandle *)resourceHandle partialProgress: (WebLoadProgress *)progress fromDataSource: (WebDataSource *)dataSource;

@end


