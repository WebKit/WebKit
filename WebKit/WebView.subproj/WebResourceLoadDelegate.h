/*
        WebResourceLoadDelegate.h
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
    Implementors are also given the opportunity to mutate requests before they are sent.
    The various progrss methods of this protocol all receive an identifier as the
    parameter.  This identifier can be used to track messages associated with a single
    resource.  For example, a single resource may generate multiple 
    resource:willSendRequest:fromDataSource messages as it's URL is redirected.
*/
@protocol WebResourceLoadDelegate <NSObject>

/*!
    @method provideIdentifierForInitialRequest:
    @discussion An implementor of WebResourceLoadDelegate should provide an identifier
    that can be used to track the load of a single resource.  This identifier will be
    passed as the first argument for all of the other WebResourceLoadDelegate.  The
    identifier is useful to track changes to a resources request, which will be
    provided by one or more calls to resource:willSendRequest:fromDataSource:.
    @result A identifier that will be passed back to the implementor for each callback.
    The identifier will be retained.
*/
- identifierForInitialRequest: (WebResourceRequest *)request fromDataSource: (WebDataSource *)dataSource;

/*!
	@method resourceRequest:willSendRequest:fromDataSource:
	@discussion This message is sent before a load is initiated.  The request may be modified
	as necessary by the receiver.
	@param identifier An identifier that can be used to track the progress of a resource load across
        multiple call backs.
	@param request The request about to be sent.
	@param dataSource The dataSource that initiated the load.
        @result Returns the request, which may be mutated by the implementor, although typically
        will be request.
*/
-(WebResourceRequest *)resource:identifier willSendRequest: (WebResourceRequest *)request fromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceRequest:didReceiveResponse:fromDataSource:
	@discussion This message is sent after a response has been received for this load.
	@param identifier An identifier that can be used to track the progress of a resource load across
        multiple call backs.
        @param response The response for the request.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resource:identifier didReceiveResponse: (WebResourceResponse *)response fromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceIdentifier:didReceiveContentLength:fromDataSource:
	@discussion Multiple of these messages may be sent as data arrives.
	@param identifier An identifier that can be used to track the progress of a resource load across
        multiple call backs.
	@param length The amount of new data received.  This is not the total amount, just the new amount received.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceRequest: didFinishLoadingFromDataSource:
	@discussion This message is sent after a load has successfully completed.
	@param identifier An identifier that can be used to track the progress of a resource load across
        multiple call backs.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource;

/*!
	@method resourceRequest:didFailLoadingWithError:fromDataSource:
	@discussion This message is sent after a load has successfully completed.
	@param identifier An identifier that can be used to track the progress of a resource load across
        multiple call backs.
	@param error The error associated with this load.
	@param dataSource The dataSource that initiated the load.
*/
-(void)resource:identifier didFailLoadingWithError:(WebError *)error fromDataSource:(WebDataSource *)dataSource;
@end


/*!
    @class WebResourceLoadDelegate
    @discussion The WebResourceLoadDelegate class responds to all WebResourceLoadDelegate protocol
    methods by doing nothing, except for resourceRequest:willSendRequest:fromDataSource:, which
    will return the newRequest. It's provided for the convenience of clients who only want
    to implement some of the above methods and ignore others.
*/
@interface WebResourceLoadDelegate : NSObject <WebResourceLoadDelegate>
{
}
@end

