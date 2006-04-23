/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/WebNetscapePluginStream.h>

#import <WebKit/WebLoader.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebNetscapePluginEmbeddedView.h>
#import <WebKit/WebNetscapePluginPackage.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebViewInternal.h>

#import <Foundation/NSURLConnection.h>

@interface WebNetscapePlugInStreamLoader : WebLoader
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

    WebFrameBridge *bridge = [[view webFrame] _bridge];
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

    _loader = [[WebNetscapePlugInStreamLoader alloc] initWithStream:self view:view]; 
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

    [[_loader dataSource] _addPlugInStreamLoader:_loader];

    BOOL succeeded = [_loader loadWithRequest:request];
    if (!succeeded) {
        [[_loader dataSource] _removePlugInStreamLoader:_loader];
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

@implementation WebNetscapePlugInStreamLoader

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
                ([(NSHTTPURLResponse *)theResponse statusCode] >= 400 || [(NSHTTPURLResponse *)theResponse statusCode] < 100)) {
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
    // Calling _removePlugInStreamLoader will likely result in a call to release, so we must retain.
    [self retain];

    [[self dataSource] _removePlugInStreamLoader:self];
    [[view webView] _finishedLoadingResourceFromDataSource:[self dataSource]];
    [stream finishedLoadingWithData:[self resourceData]];
    [super didFinishLoading];

    [self release];
}

- (void)didFailWithError:(NSError *)error
{
    // Calling _removePlugInStreamLoader will likely result in a call to release, so we must retain.
    // The other additional processing can do anything including possibly releasing self;
    // one example of this is 3266216
    [self retain];

    [[self dataSource] _removePlugInStreamLoader:self];
    [[view webView] _receivedError:error fromDataSource:[self dataSource]];
    [stream destroyStreamWithError:error];
    [super didFailWithError:error];

    [self release];
}

- (void)cancelWithError:(NSError *)error
{
    // Calling _removePlugInStreamLoader will likely result in a call to release, so we must retain.
    [self retain];

    [[self dataSource] _removePlugInStreamLoader:self];
    [super cancelWithError:error];

    [self release];
}

@end
