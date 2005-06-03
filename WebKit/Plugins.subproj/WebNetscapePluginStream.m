/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNetscapePluginStream.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/WebBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLResponsePrivate.h>

@interface WebNetscapePluginConnectionDelegate : WebBaseResourceHandleDelegate
{
    WebNetscapePluginStream *stream;
    WebBaseNetscapePluginView *view;
}
- initWithStream:(WebNetscapePluginStream *)theStream view:(WebBaseNetscapePluginView *)theView;
- (BOOL)isDone;
@end

@implementation WebNetscapePluginStream

- initWithRequest:(NSURLRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData 
 sendNotification:(BOOL)flag
{   
    WebBaseNetscapePluginView *view = (WebBaseNetscapePluginView *)thePluginPointer->ndata;

    WebBridge *bridge = [[view webFrame] _bridge];
    BOOL hideReferrer;
    if (![bridge canLoadURL:[theRequest URL] fromReferrer:[bridge referrer] hideReferrer:&hideReferrer])
        return nil;

    if ([self initWithRequestURL:[theRequest URL]
                    pluginPointer:thePluginPointer
                       notifyData:theNotifyData
                 sendNotification:flag] == nil) {
        return nil;
    }
    
    // Temporarily set isTerminated to YES to avoid assertion failure in dealloc in case we are released in this method.
    isTerminated = YES;
    
    if (![WebView _canHandleRequest:theRequest]) {
        [self release];
        return nil;
    }
        
    request = [theRequest mutableCopy];
    if (hideReferrer) {
        [(NSMutableURLRequest *)request _web_setHTTPReferrer:nil];
    }

    _loader = [[WebNetscapePluginConnectionDelegate alloc] initWithStream:self view:view]; 
    [_loader setDataSource:[view dataSource]];
    
    isTerminated = NO;

    return self;
}

- (void)dealloc
{
    [_loader release];
    [request release];
    [super dealloc];
}

- (void)start
{
    ASSERT(request);

    [[_loader dataSource] _addPlugInStreamClient:_loader];

    BOOL succeeded = [_loader loadWithRequest:request];
    if (!succeeded) {
        [[_loader dataSource] _removePlugInStreamClient:_loader];
    }
}

- (void)cancelLoadWithError:(NSError *)error
{
    if (![_loader isDone]) {
        [_loader cancelWithError:error];
    }
}

- (void)stop
{
    [self cancelLoadAndDestroyStreamWithError:[_loader cancelledError]];
}

@end

@implementation WebNetscapePluginConnectionDelegate

- initWithStream:(WebNetscapePluginStream *)theStream view:(WebBaseNetscapePluginView *)theView
{
    [super init];
    stream = [theStream retain];
    view = [theView retain];
    return self;
}

- (BOOL)isDone
{
    return stream == nil;
}

- (void)releaseResources
{
    [stream release];
    stream = nil;
    [view release];
    view = nil;
    [super releaseResources];
}

- (void)didReceiveResponse:(NSURLResponse *)theResponse
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [stream startStreamWithResponse:theResponse];
    
    // Don't continue if the stream is cancelled in startStreamWithResponse or didReceiveResponse.
    if (stream) {
        [super didReceiveResponse:theResponse];
        if (stream) {
            if ([theResponse isKindOfClass:[NSHTTPURLResponse class]] &&
                [NSHTTPURLResponse isErrorStatusCode:[(NSHTTPURLResponse *)theResponse statusCode]]) {
                NSError *error = [NSError _webKitErrorWithDomain:NSURLErrorDomain
                                                            code:NSURLErrorFileDoesNotExist
                                                            URL:[theResponse URL]];
                [stream cancelLoadAndDestroyStreamWithError:error];
            }
        }
    }
    [self release];
}

- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [stream receivedData:data];
    [super didReceiveData:data lengthReceived:lengthReceived];
    [self release];
}

- (void)didFinishLoading
{
    // Calling _removePlugInStreamClient will likely result in a call to release, so we must retain.
    [self retain];

    [[self dataSource] _removePlugInStreamClient:self];
    [[view webView] _finishedLoadingResourceFromDataSource:[self dataSource]];
    [stream finishedLoadingWithData:[self resourceData]];
    [super didFinishLoading];

    [self release];
}

- (void)didFailWithError:(NSError *)error
{
    // Calling _removePlugInStreamClient will likely result in a call to release, so we must retain.
    // The other additional processing can do anything including possibly releasing self;
    // one example of this is 3266216
    [self retain];

    [[self dataSource] _removePlugInStreamClient:self];
    [[view webView] _receivedError:error fromDataSource:[self dataSource]];
    [stream destroyStreamWithError:error];
    [super didFailWithError:error];

    [self release];
}

- (void)cancelWithError:(NSError *)error
{
    // Calling _removePlugInStreamClient will likely result in a call to release, so we must retain.
    [self retain];

    [[self dataSource] _removePlugInStreamClient:self];
    [super cancelWithError:error];

    [self release];
}

@end
