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

#import "WebSubresourceLoader.h"

#import "WebFormDataStream.h"
#import "WebFrameLoader.h"
#import <Foundation/NSURLResponse.h>
#import <JavaScriptCore/Assertions.h>
#import <WebCore/WebCoreResourceLoader.h>
#import <WebCore/WebCoreSystemInterface.h>

@implementation WebSubresourceLoader

- initWithLoader:(id <WebCoreResourceLoader>)l frameLoader:(WebFrameLoader *)fl
{
    [super init];
    
    coreLoader = [l retain];

    [self setFrameLoader:fl];
    
    return self;
}

- (void)dealloc
{
    [coreLoader release];
    [super dealloc];
}

static BOOL isConditionalRequest(NSURLRequest *request)
{
    if ([request valueForHTTPHeaderField:@"If-Match"] ||
        [request valueForHTTPHeaderField:@"If-Modified-Since"] ||
        [request valueForHTTPHeaderField:@"If-None-Match"] ||
        [request valueForHTTPHeaderField:@"If-Range"] ||
        [request valueForHTTPHeaderField:@"If-Unmodified-Since"])
        return YES;
    return NO;
}

static BOOL hasCaseInsensitivePrefix(NSString *str, NSString *prefix)
{
    return str && ([str rangeOfString:prefix options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location != NSNotFound);
}

static BOOL isFileURLString(NSString *URL)
{
    return hasCaseInsensitivePrefix(URL, @"file:");
}

#define WebReferrer     (@"Referer")

static void setHTTPReferrer(NSMutableURLRequest *request, NSString *theReferrer)
{
    // Do not set the referrer to a string that refers to a file URL.
    // That is a potential security hole.
    if (isFileURLString(theReferrer))
        return;
    
    // Don't allow empty Referer: headers; some servers refuse them
    if([theReferrer length] == 0)
        theReferrer = nil;
    
    [request setValue:theReferrer forHTTPHeaderField:WebReferrer];
}

+ (WebSubresourceLoader *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
                                   withRequest:(NSMutableURLRequest *)newRequest
                                 customHeaders:(NSDictionary *)customHeaders
                                      referrer:(NSString *)referrer 
                                forFrameLoader:(WebFrameLoader *)fl
{
    if ([fl state] == WebFrameStateProvisional)
        return nil;
        
    wkSupportsMultipartXMixedReplace(newRequest);

    WebSubresourceLoader *loader = [[[self alloc] initWithLoader:rLoader frameLoader:fl] autorelease];
    
    [fl addSubresourceLoader:loader];

    NSEnumerator *e = [customHeaders keyEnumerator];
    NSString *key;
    while ((key = [e nextObject]))
        [newRequest addValue:[customHeaders objectForKey:key] forHTTPHeaderField:key];

    // Use the original request's cache policy for two reasons:
    // 1. For POST requests, we mutate the cache policy for the main resource,
    //    but we do not want this to apply to subresources
    // 2. Delegates that modify the cache policy using willSendRequest: should
    //    not affect any other resources. Such changes need to be done
    //    per request.
    if (isConditionalRequest(newRequest))
        [newRequest setCachePolicy:NSURLRequestReloadIgnoringCacheData];
    else
        [newRequest setCachePolicy:[[fl _originalRequest] cachePolicy]];
    setHTTPReferrer(newRequest, referrer);
    
    [fl addExtraFieldsToRequest:newRequest mainResource:NO alwaysFromRequest:NO];
            
    if (![loader loadWithRequest:newRequest])
        loader = nil;
    
    return loader;
}

+ (WebSubresourceLoader *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
                                    withMethod:(NSString *)method 
                                           URL:(NSURL *)URL
                                 customHeaders:(NSDictionary *)customHeaders
                                      referrer:(NSString *)referrer
                                 forFrameLoader:(WebFrameLoader *)fl
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];

    // setHTTPMethod is not called for GET requests to work around <rdar://4464032>.
    if (![method isEqualToString:@"GET"])
        [newRequest setHTTPMethod:method];

    WebSubresourceLoader *loader = [self startLoadingResource:rLoader withRequest:newRequest customHeaders:customHeaders referrer:referrer forFrameLoader:fl];
    [newRequest release];

    return loader;
}

+ (WebSubresourceLoader *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
                                    withMethod:(NSString *)method 
                                           URL:(NSURL *)URL
                                 customHeaders:(NSDictionary *)customHeaders
                                      postData:(NSArray *)postData
                                      referrer:(NSString *)referrer
                                forFrameLoader:(WebFrameLoader *)fl
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];

    // setHTTPMethod is not called for GET requests to work around <rdar://4464032>.
    if (![method isEqualToString:@"GET"])
        [newRequest setHTTPMethod:method];

    webSetHTTPBody(newRequest, postData);

    WebSubresourceLoader *loader = [self startLoadingResource:rLoader withRequest:newRequest customHeaders:customHeaders referrer:referrer forFrameLoader:fl];
    [newRequest release];

    return loader;

}

- (void)receivedError:(NSError *)error
{
    [frameLoader _receivedError:error];
}

- (NSURLRequest *)willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse;
{
    NSURL *oldURL = [request URL];
    NSURLRequest *clientRequest = [super willSendRequest:newRequest redirectResponse:redirectResponse];
    
    if (clientRequest != nil && oldURL != [clientRequest URL] && ![oldURL isEqual:[clientRequest URL]])
        [coreLoader redirectedToURL:[clientRequest URL]];

    return clientRequest;
}

- (void)didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(r);

    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"])
        loadingMultipartContent = YES;

    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    [coreLoader receivedResponse:r];
    // The coreLoader can cancel a load if it receives a multipart response for a non-image
    if (reachedTerminalState) {
        [self release];
        return;
    }
    [super didReceiveResponse:r];
    [self release];
    
    if (loadingMultipartContent && [[self resourceData] length]) {
        // A subresource loader does not load multipart sections progressively, deliver the previously received data to the coreLoader all at once
        [coreLoader addData:[self resourceData]];
        // Clears the data to make way for the next multipart section
        [self clearResourceData];
        
        // After the first multipart section is complete, signal to delegates that this load is "finished" 
        if (!signalledFinish)
            [self signalFinish];
    }
}

- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived allAtOnce:(BOOL)allAtOnce
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    // A subresource loader does not load multipart sections progressively, don't deliver any data to the coreLoader yet
    if (!loadingMultipartContent)
        [coreLoader addData:data];
    [super didReceiveData:data lengthReceived:lengthReceived allAtOnce:allAtOnce];
    [self release];
}

- (void)signalFinish
{
    [frameLoader removeSubresourceLoader:self];
    [frameLoader _finishedLoadingResource];
    [super signalFinish];
}

- (void)didFinishLoading
{
    // Calling _removeSubresourceLoader will likely result in a call to release, so we must retain.
    [self retain];
    
    [coreLoader finishWithData:[self resourceData]];
    
    if (!signalledFinish)
        [self signalFinish];
        
    [super didFinishLoading];

    [self release];    
}

- (void)didFailWithError:(NSError *)error
{
    // Calling _removeSubresourceLoader will likely result in a call to release, so we must retain.
    [self retain];
    
    [coreLoader reportError];
    [frameLoader removeSubresourceLoader:self];
    [self receivedError:error];
    [super didFailWithError:error];

    [self release];
}

- (void)cancel
{
    // Calling _removeSubresourceLoader will likely result in a call to release, so we must retain.
    [self retain];
        
    [coreLoader cancel];
    [frameLoader removeSubresourceLoader:self];
    [self receivedError:[self cancelledError]];
    [super cancel];

    [self release];
}

@end
