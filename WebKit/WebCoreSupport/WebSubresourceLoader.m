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

#import <WebKit/WebSubresourceLoader.h>

#import <WebKit/WebAssertions.h>
#import <WebKit/WebFrameBridge.h>
#import <WebKit/WebDataSourcePrivate.h>
#import <WebKit/WebFormDataStream.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebKitErrorsPrivate.h>
#import <WebKit/WebNSURLRequestExtras.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSURLResponse.h>

#import <WebCore/WebCoreResourceLoader.h>
#import <WebKitSystemInterface.h>

@implementation WebSubresourceLoader

- initWithLoader:(id <WebCoreResourceLoader>)l dataSource:(WebDataSource *)s
{
    [super init];
    
    coreLoader = [l retain];

    [self setDataSource: s];
    
    return self;
}

- (void)dealloc
{
    [coreLoader release];
    [super dealloc];
}

+ (WebSubresourceLoader *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
				   withRequest:(NSMutableURLRequest *)newRequest
                                 customHeaders:(NSDictionary *)customHeaders
				      referrer:(NSString *)referrer 
				 forDataSource:(WebDataSource *)source
{
    WebSubresourceLoader *loader = [[[self alloc] initWithLoader:rLoader dataSource:source] autorelease];
    
    [loader setSupportsMultipartContent:WKSupportsMultipartXMixedReplace(newRequest)];
    [source _addSubresourceLoader:loader];

    NSEnumerator *e = [customHeaders keyEnumerator];
    NSString *key;
    while ((key = (NSString *)[e nextObject]) != nil) {
	[newRequest addValue:[customHeaders objectForKey:key] forHTTPHeaderField:key];
    }

    // Use the original request's cache policy for two reasons:
    // 1. For POST requests, we mutate the cache policy for the main resource,
    //    but we do not want this to apply to subresources
    // 2. Delegates that modify the cache policy using willSendRequest: should
    //    not affect any other resources. Such changes need to be done
    //    per request.
    [newRequest setCachePolicy:[[source _originalRequest] cachePolicy]];
    [newRequest _web_setHTTPReferrer:referrer];
    
    WebView *_webView = [source _webView];
    [newRequest setMainDocumentURL:[[[[_webView mainFrame] dataSource] request] URL]];
    [newRequest _web_setHTTPUserAgent:[_webView userAgentForURL:[newRequest URL]]];
            
    if (![loader loadWithRequest:newRequest]) {
        loader = nil;
    }
    
    return loader;
}

+ (WebSubresourceLoader *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
				       withURL:(NSURL *)URL
				 customHeaders:(NSDictionary *)customHeaders
				      referrer:(NSString *)referrer
				 forDataSource:(WebDataSource *)source
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];
    WebSubresourceLoader *loader = [self startLoadingResource:rLoader withRequest:newRequest customHeaders:customHeaders referrer:referrer forDataSource:source];
    [newRequest release];

    return loader;
}

+ (WebSubresourceLoader *)startLoadingResource:(id <WebCoreResourceLoader>)rLoader
				       withURL:(NSURL *)URL
				 customHeaders:(NSDictionary *)customHeaders
				      postData:(NSArray *)postData
				      referrer:(NSString *)referrer
				 forDataSource:(WebDataSource *)source
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];

    [newRequest setHTTPMethod:@"POST"];
    webSetHTTPBody(newRequest, postData);

    WebSubresourceLoader *loader = [self startLoadingResource:rLoader withRequest:newRequest customHeaders:customHeaders referrer:referrer forDataSource:source];
    [newRequest release];

    return loader;

}

- (void)receivedError:(NSError *)error
{
    [[dataSource _webView] _receivedError:error fromDataSource:dataSource];
}

- (NSURLRequest *)willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse;
{
    NSURL *oldURL = [request URL];
    NSURLRequest *clientRequest = [super willSendRequest:newRequest redirectResponse:redirectResponse];
    
    if (clientRequest != nil && oldURL != [clientRequest URL] && ![oldURL isEqual:[clientRequest URL]]) {
	[coreLoader redirectedToURL:[clientRequest URL]];
    }

    return clientRequest;
}

- (void)didReceiveResponse:(NSURLResponse *)r
{
    ASSERT(r);

    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"]) {
        if (!supportsMultipartContent) {
            [dataSource _removeSubresourceLoader:self];
            [[[dataSource _webView] mainFrame] _checkLoadComplete];
            [self cancelWithError:[NSError _webKitErrorWithDomain:NSURLErrorDomain
                                                             code:NSURLErrorUnsupportedURL
                                                              URL:[r URL]]];
            return;
        }   
        loadingMultipartContent = YES;
    }

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
        // Tells the dataSource to save the just completed section, necessary for saving/dragging multipart images
        [self saveResource];
        // Clears the data to make way for the next multipart section
        [self clearResourceData];
        
        // After the first multipart section is complete, signal to delegates that this load is "finished" 
        if (!signalledFinish)
            [self signalFinish];
    }
}

- (void)didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    // retain/release self in this delegate method since the additional processing can do
    // anything including possibly releasing self; one example of this is 3266216
    [self retain];
    // A subresource loader does not load multipart sections progressively, don't deliver any data to the coreLoader yet
    if (!loadingMultipartContent)
        [coreLoader addData:data];
    [super didReceiveData:data lengthReceived:lengthReceived];
    [self release];
}

- (void)signalFinish
{
    [dataSource _removeSubresourceLoader:self];
    [[dataSource _webView] _finishedLoadingResourceFromDataSource:dataSource];
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
    [dataSource _removeSubresourceLoader:self];
    [self receivedError:error];
    [super didFailWithError:error];

    [self release];
}

- (void)cancel
{
    // Calling _removeSubresourceLoader will likely result in a call to release, so we must retain.
    [self retain];
        
    [coreLoader cancel];
    [dataSource _removeSubresourceLoader:self];
    [self receivedError:[self cancelledError]];
    [super cancel];

    [self release];
}

@end
