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

- initWithRequest:(WebResourceRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData
{
    [super init];

    if(!theRequest || !thePluginPointer || ![WebResourceHandle canInitWithRequest:theRequest]){
        return nil;
    }

    request = [theRequest retain];

    [self setPluginPointer:thePluginPointer];

    view = [(WebNetscapePluginEmbeddedView *)instance->ndata retain];

    [self setDataSource: [view dataSource]];

    notifyData = theNotifyData;
    resourceData = [[NSMutableData alloc] init];

    return self;
}

- (void)dealloc
{
    [resourceData release];
    [super dealloc];
}

- (void)start
{
    handle = [[WebResourceHandle alloc] initWithRequest:request];
    [handle loadWithDelegate:self];
}

- (void)stop
{
    [view release];
    view = nil;
    
    if (!handle) {
        return;
    }

    // Stopped while loading.
    [handle cancel];

    WebController *controller = [view controller];

    WebError *cancelError = [[WebError alloc] initWithErrorCode:WebErrorCodeCancelled
                                                       inDomain:WebErrorDomainWebFoundation
                                                     failingURL:nil];
    [controller _receivedError:cancelError fromDataSource:[view dataSource]];

    [cancelError release];

    [handle release];
    handle = nil;

    if(URL){
        // Send error only if the response has been set (the URL is set with the response).
        [self receivedError:NPRES_USER_BREAK];
    }
}

- (WebResourceRequest *)handle:(WebResourceHandle *)h willSendRequest:(WebResourceRequest *)theRequest
{
    return [super handle: h willSendRequest: theRequest];
}

- (void)handle:(WebResourceHandle *)h didReceiveResponse:(WebResourceResponse *)theResponse
{
    ASSERT(handle == h);
    
    [self setResponse:theResponse];
    [super handle: h didReceiveResponse: theResponse];    
}

- (void)handle:(WebResourceHandle *)h didReceiveData:(NSData *)data
{
    ASSERT(handle == h);

    if(transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        [resourceData appendData:data];
    }
    
    [self receivedData:data];

    [super handle: handle didReceiveData: data];
}

- (void)handleDidFinishLoading:(WebResourceHandle *)h
{
    ASSERT(handle == h);

    WebController *controller = [view controller];

    [controller _finsishedLoadingResourceFromDataSource: [view dataSource]];

    [self finishedLoadingWithData:resourceData];

    [view release];
    view = nil;
    
    [super handleDidFinishLoading: h];
}

- (void)handle:(WebResourceHandle *)h didFailLoadingWithError:(WebError *)result
{
    ASSERT(handle == h);

    WebController *controller = [view controller];

    [controller _receivedError: result fromDataSource: [view dataSource]];

    [self receivedError:NPRES_NETWORK_ERR];

    [view release];
    view = nil;
    
    [super handle: h didFailLoadingWithError: result];
}

@end
