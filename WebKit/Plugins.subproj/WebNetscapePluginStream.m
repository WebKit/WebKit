/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebControllerPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginStream.h>

#import <WebFoundation/WebFoundation.h>

@implementation WebNetscapePluginStream

- (void)setCurrentURL:(NSURL *)theCurrentURL
{
    [currentURL release];
    currentURL = [theCurrentURL retain];
}

- initWithRequest:(WebResourceRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData
{
    [super init];

    if(!theRequest || !thePluginPointer || ![WebResourceHandle canInitWithRequest:theRequest]){
        return nil;
    }

    request = [theRequest retain];

    [self setCurrentURL:[theRequest URL]];
    
    [self setPluginPointer:thePluginPointer];

    view = [(WebNetscapePluginEmbeddedView *)instance->ndata retain];

    notifyData = theNotifyData;
    resourceData = [[NSMutableData alloc] init];

    return self;
}

- (void)dealloc
{
    [resourceData release];
    [request release];
    [currentURL release];
    [super dealloc];
}

- (void)start
{
    resource = [[WebResourceHandle alloc] initWithRequest:request];
    [resource loadWithDelegate:self];
    [[view controller] _didStartLoading:currentURL];
}

- (void)stop
{
    [view release];
    view = nil;
    
    if (!resource) {
        return;
    }

    // Stopped while loading.

    [resource cancel];

    WebController *controller = [view controller];

    WebError *cancelError = [[WebError alloc] initWithErrorCode:WebErrorCodeCancelled
                                                        inDomain:WebErrorDomainWebFoundation
                                                        failingURL:nil];
    [controller _receivedError:cancelError
                forResourceHandle:resource
                fromDataSource:[view dataSource]];

    [cancelError release];

    [resource release];
    resource = nil;

    if(URL){
        // Send error only if the response has been set (the URL is set with the response).
        [self receivedError:NPRES_USER_BREAK];
    }

    [controller _didStopLoading:currentURL];
}

- (void)loadEnded
{
    [resource release];
    resource = nil;

    [self stop];
}

- (WebResourceRequest *)handle:(WebResourceHandle *)handle willSendRequest:(WebResourceRequest *)theRequest
{
    WebController *webController = [view controller];

    [webController _didStopLoading:currentURL];

    [self setCurrentURL:[theRequest URL]];

    [webController _didStartLoading:currentURL];
    
    return theRequest;
}

- (void)handle:(WebResourceHandle *)handle didReceiveResponse:(WebResourceResponse *)theResponse
{
    [self setResponse:theResponse];
}

- (void)handle:(WebResourceHandle *)handle didReceiveData:(NSData *)data
{
    ASSERT(resource == handle);

    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        [resourceData appendData:data];
    }
    
    [self receivedData:data];

    [[view controller] _receivedProgressForResourceHandle: handle fromDataSource: [view dataSource] complete: NO];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)handle
{
    ASSERT(resource == handle);

    WebController *controller = [view controller];

    [controller _receivedProgressForResourceHandle: handle fromDataSource: [view dataSource] complete: YES];

    [self finishedLoadingWithData:resourceData];

    [controller _didStopLoading:currentURL];

    [self loadEnded];
}

- (void)handle:(WebResourceHandle *)handle didFailLoadingWithError:(WebError *)result
{
    ASSERT(resource == handle);

    WebController *controller = [view controller];

    [controller _receivedError: result forResourceHandle: handle
                fromDataSource: [view dataSource]];

    [self receivedError:NPRES_NETWORK_ERR];

    [controller _didStopLoading:currentURL];

    [self loadEnded];
}

@end
