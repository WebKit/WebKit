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
#import <WebFoundation/NSURLRequest.h>
#import <WebFoundation/NSURLConnection.h>

@implementation WebNetscapePluginStream

- initWithRequest:(NSURLRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData
{
    [super init];

    if(!theRequest || !thePluginPointer || ![NSURLConnection canInitWithRequest:theRequest]){
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

- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)theResponse
{
    [self setResponse:theResponse];
    [super connection:con didReceiveResponse:theResponse];    
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data
{
    if (transferMode == NP_ASFILE || transferMode == NP_ASFILEONLY) {
        [resourceData appendData:data];
    }
    
    [self receivedData:data];

    [super connection:con didReceiveData:data];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    [[view controller] _finishedLoadingResourceFromDataSource:[view dataSource]];
    [self finishedLoadingWithData:resourceData];

    [view release];
    view = nil;
    
    [super connectionDidFinishLoading:con];
}

- (void)connection:(NSURLConnection *)con didFailLoadingWithError:(WebError *)result
{
    [[view controller] _receivedError:result fromDataSource:[view dataSource]];

    [self receivedError:NPRES_NETWORK_ERR];

    [view release];
    view = nil;
    
    [super connection:con didFailLoadingWithError:result];
}

@end
