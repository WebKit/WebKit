/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginStream.h>
#import <WebKit/WebViewPrivate.h>

#import <WebFoundation/WebFoundation.h>

@implementation WebNetscapePluginStream

- initWithRequest:(WebRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData
{
    [super init];

    if(!theRequest || !thePluginPointer || ![WebResource canInitWithRequest:theRequest]){
        return nil;
    }

    _startingRequest = [theRequest copy];

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
    [_startingRequest release];
    [super dealloc];
}

- (void)start
{
    ASSERT(_startingRequest);
    [self loadWithRequest:_startingRequest];
    [_startingRequest release];
    _startingRequest = nil;
}

- (void)stop
{
    if (view) {
        [self cancel];
    }
}

- (void)cancel
{
    [view release];
    view = nil;

    [super cancel];

    // Send error only if the response has been set (the URL is set with the response).
    if (URL) {
        [self receivedError:NPRES_USER_BREAK];
    }
}

- (void)resource:(WebResource *)h didReceiveResponse:(WebResponse *)theResponse
{
    [self setResponse:theResponse];
    [super resource:h didReceiveResponse:theResponse];    
}

- (void)resource:(WebResource *)h didReceiveData:(NSData *)data
{
    if (transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        [resourceData appendData:data];
    }
    
    [self receivedData:data];

    [super resource:h didReceiveData:data];
}

- (void)resourceDidFinishLoading:(WebResource *)h
{
    [[view controller] _finishedLoadingResourceFromDataSource:[view dataSource]];
    [self finishedLoadingWithData:resourceData];

    [view release];
    view = nil;
    
    [super resourceDidFinishLoading: h];
}

- (void)resource:(WebResource *)h didFailLoadingWithError:(WebError *)result
{
    [[view controller] _receivedError:result fromDataSource:[view dataSource]];

    [self receivedError:NPRES_NETWORK_ERR];

    [view release];
    view = nil;
    
    [super resource:h didFailLoadingWithError:result];
}

@end
