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

#import <WebKit/WebNetscapePlugInStreamLoader.h>

#import <WebKit/WebFrameLoader.h>

@implementation WebNetscapePlugInStreamLoader

- (id)initWithDelegate:(NSObject<WebPlugInStreamLoaderDelegate> *)theStream frameLoader:(WebFrameLoader *)fl;
{
    [super init];
    stream = [theStream retain];
    [self setFrameLoader:fl];
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
                NSError *error = [frameLoader fileDoesNotExistErrorWithResponse:theResponse];
                [stream cancelLoadAndDestroyStreamWithError:error];
            }
        }
    }
    [self release];
}

- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived allAtOnce:(BOOL)allAtOnce
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [stream receivedData:data];
    [super didReceiveData:data lengthReceived:lengthReceived allAtOnce:(BOOL)allAtOnce];
    [self release];
}

- (void)didFinishLoading
{
    // Calling _removePlugInStreamLoader will likely result in a call to release, so we must retain.
    [self retain];

    [frameLoader removePlugInStreamLoader:self];
    [frameLoader _finishedLoadingResource];
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

    [[self frameLoader] removePlugInStreamLoader:self];
    [[self frameLoader] _receivedError:error];
    [stream destroyStreamWithError:error];
    [super didFailWithError:error];

    [self release];
}

- (void)cancelWithError:(NSError *)error
{
    // Calling _removePlugInStreamLoader will likely result in a call to release, so we must retain.
    [self retain];

    [[self frameLoader] removePlugInStreamLoader:self];
    [stream destroyStreamWithError:error];
    [super cancelWithError:error];

    [self release];
}

@end
