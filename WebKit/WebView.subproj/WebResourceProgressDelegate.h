/*
        WebResourceProgressDelegate.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/
@class WebResourceResponse;
@class WebResourceRequest;

/*!
    @protocol  WebResourceLoadDelegate
    @discussion Implementors of this protocol will receive messages indicating
    that a resource is about to be loaded, data has been received for a resource,
    an error has been received for a resource, and completion of a resource load.
    Implementors are also given the opportunity to mutate a request before it is sent.
*/
@protocol WebResourceLoadDelegate <NSObject>

/*!
	@method resourceRequest:willSendRequest:fromDataSource:
	@discussion This message is sent before a load is initiated.  The request may be modified
	as necessary by the receiver.
	@param oldRequest The request about to be sent.
	@param newRequest The request about to be sent.
	@param dataSource The dataSource that initiated the load.
*/
-(WebResourceRequest *)resourceRequest:(WebResourceRequest *)oldRequest willSendRequest: (WebResourceRequest *)newRequest fromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceRequest:didReceiveResponse:fromDataSource:
	@discussion This message is sent after a response has been received for this load.
        @param request The request that triggered the response.
        @param response The response for the request.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resourceRequest:(WebResourceRequest *)request didReceiveResponse: (WebResourceResponse *)response fromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceIdentifier:didReceiveContentLength:fromDataSource:
	@discussion Multiple of these messages may be sent as data arrives.
        @param request The request that represents the resource receiving data.
	@param length The amount of new data received.  This is not the total amount, just the new amount received.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resourceRequest:(WebResourceRequest *)request didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceRequest: didFinishLoadingFromDataSource:
	@discussion This message is sent after a load has successfully completed.
	@param request The request that has successfully loaded.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resourceRequest:(WebResourceRequest *)request didFinishLoadingFromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceRequest:didFailLoadingWithError:fromDataSource:
	@discussion This message is sent after a load has successfully completed.
	@param request The request that failed to load.
	@param error The error associated with this load.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resourceRequest:(WebResourceRequest *)request didFailLoadingWithError:(WebError *)error fromDataSource:(WebDataSource *)dataSource;
@end


