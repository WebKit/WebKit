/*
        WebNetscapePluginStream.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNetscapePluginStream.h>

#import <WebKit/WebBaseResourceHandleDelegate.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSError_NSURLExtras.h>
#import <Foundation/NSURLConnection.h>
#import <Foundation/NSURLResponsePrivate.h>
#import <Foundation/NSURLRequest.h>

@interface WebNetscapePluginConnectionDelegate : WebBaseResourceHandleDelegate
{
    WebNetscapePluginStream *stream;
    WebBaseNetscapePluginView *view;
}
- initWithStream:(WebNetscapePluginStream *)theStream view:(WebBaseNetscapePluginView *)theView;
@end

@implementation WebNetscapePluginStream

- initWithRequest:(NSURLRequest *)theRequest
    pluginPointer:(NPP)thePluginPointer
       notifyData:(void *)theNotifyData
{
    [super init];

    if (!theRequest || !thePluginPointer || ![WebView _canHandleRequest:theRequest]) {
        [self release];
        return nil;
    }

    _startingRequest = [theRequest copy];

    [self setPluginPointer:thePluginPointer];
    [self setNotifyData:theNotifyData];

    WebBaseNetscapePluginView *view = (WebBaseNetscapePluginView *)instance->ndata;
    _loader = [[WebNetscapePluginConnectionDelegate alloc] initWithStream:self view:view]; 
    [_loader setDataSource:[view dataSource]];

    return self;
}

- (void)dealloc
{
    [_loader release];
    [_startingRequest release];
    [super dealloc];
}

- (void)start
{
    ASSERT(_startingRequest);

    [[_loader dataSource] _addPlugInStreamClient:_loader];

    BOOL succeeded = [_loader loadWithRequest:_startingRequest];
    [_startingRequest release];
    _startingRequest = nil;

    if (!succeeded) {
        [[_loader dataSource] _removePlugInStreamClient:_loader];
    }
}

- (void)cancelWithReason:(NPReason)theReason
{
    if (theReason == WEB_REASON_PLUGIN_CANCELLED) {
        NSURLResponse *response = [_loader response];
        NSError *error = [[NSError alloc] _initWithPluginErrorCode:WebKitErrorPlugInCancelledConnection
                                                        contentURL:[response URL]
                                                     pluginPageURL:nil
                                                        pluginName:[plugin name]
                                                          MIMEType:[response MIMEType]];
        [_loader cancelWithError:error];
        [error release];
    } else {
        [_loader cancel];
    }
    [super cancelWithReason:theReason];
}

- (void)stop
{
    [self cancelWithReason:NPRES_USER_BREAK];
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
                [stream receivedError:error];
                [self cancelWithError:error];
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
    [stream receivedError:error];
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
