/*
        WebResourceLoadDelegate.h
        Copyright 2001, 2002, Apple Computer, Inc.
        
        Public header file.
*/
@class WebView;
@class WebDataSource;
@class WebPluginError;
@class WebResponse;
@class NSURLRequest;

/*!
    @category  WebResourceLoadDelegate
    @discussion Implementors of this protocol will receive messages indicating
    that a resource is about to be loaded, data has been received for a resource,
    an error has been received for a resource, and completion of a resource load.
    Implementors are also given the opportunity to mutate requests before they are sent.
    The various progress methods of this protocol all receive an identifier as the
    parameter.  This identifier can be used to track messages associated with a single
    resource.  For example, a single resource may generate multiple 
    resource:willSendRequest:fromDataSource: messages as it's URL is redirected.
*/
@interface NSObject (WebResourceLoadDelegate)

/*!
    @method identifierForInitialRequest:fromDataSource:
    @param webView The WebView sending the message.
    @param request The request about to be sent.
    @param dataSource The datasource that initiated the load.
    @discussion An implementor of WebResourceLoadDelegate should provide an identifier
    that can be used to track the load of a single resource.  This identifier will be
    passed as the first argument for all of the other WebResourceLoadDelegate methods.  The
    identifier is useful to track changes to a resources request, which will be
    provided by one or more calls to resource:willSendRequest:fromDataSource:.
    @result An identifier that will be passed back to the implementor for each callback.
    The identifier will be retained.
*/
- webView:(WebView *)sender identifierForInitialRequest: (NSURLRequest *)request fromDataSource: (WebDataSource *)dataSource;

/*!
    @method resource:willSendRequest:fromDataSource:
    @discussion This message is sent before a load is initiated.  The request may be modified
    as necessary by the receiver.
    @param webView The WebView sending the message.
    @param identifier An identifier that can be used to track the progress of a resource load across
    multiple call backs.
    @param request The request about to be sent.
    @param dataSource The dataSource that initiated the load.
    @result Returns the request, which may be mutated by the implementor, although typically
    will be request.
*/
-(NSURLRequest *)webView:(WebView *)sender resource:identifier willSendRequest: (NSURLRequest *)request fromDataSource:(WebDataSource *)dataSource;

/*!
    @method resource:didReceiveResponse:fromDataSource:
    @discussion This message is sent after a response has been received for this load.
    @param webView The WebView sending the message.
    @param identifier An identifier that can be used to track the progress of a resource load across
    multiple call backs.
    @param response The response for the request.
    @param dataSource The dataSource that initiated the load.
*/
-(void)webView:(WebView *)sender resource:identifier didReceiveResponse: (WebResponse *)response fromDataSource:(WebDataSource *)dataSource;

/*!
    @method resource:didReceiveContentLength:fromDataSource:
    @discussion Multiple of these messages may be sent as data arrives.
    @param webView The WebView sending the message.
    @param identifier An identifier that can be used to track the progress of a resource load across
    multiple call backs.
    @param length The amount of new data received.  This is not the total amount, just the new amount received.
    @param dataSource The dataSource that initiated the load.
*/
-(void)webView:(WebView *)sender resource:identifier didReceiveContentLength: (unsigned)length fromDataSource:(WebDataSource *)dataSource;

/*!
    @method resource:didFinishLoadingFromDataSource:
    @discussion This message is sent after a load has successfully completed.
    @param webView The WebView sending the message.
    @param identifier An identifier that can be used to track the progress of a resource load across
    multiple call backs.
    @param dataSource The dataSource that initiated the load.
*/
-(void)webView:(WebView *)sender resource:identifier didFinishLoadingFromDataSource:(WebDataSource *)dataSource;

/*!
    @method resource:didFailLoadingWithError:fromDataSource:
    @discussion This message is sent after a load has failed to load due to an error.
    @param webView The WebView sending the message.
    @param identifier An identifier that can be used to track the progress of a resource load across
    multiple call backs.
    @param error The error associated with this load.
    @param dataSource The dataSource that initiated the load.
*/
-(void)webView:(WebView *)sender resource:identifier didFailLoadingWithError:(WebError *)error fromDataSource:(WebDataSource *)dataSource;

/*!
     @method pluginFailedWithError:dataSource:
     @discussion Called when a plug-in is not found, fails to load or is not available for some reason.
     @param webView The WebView sending the message.
     @param error The plug-in error.
     @param dataSource The dataSource that contains the plug-in.
*/
- (void)webView:(WebView *)sender pluginFailedWithError:(WebPluginError *)error dataSource:(WebDataSource *)dataSource;

@end


