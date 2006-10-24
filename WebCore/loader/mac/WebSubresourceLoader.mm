/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "WebSubresourceLoader.h"

#import "LoaderNSURLExtras.h"
#import "LoaderNSURLRequestExtras.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreResourceLoader.h"
#import "WebCoreSystemInterface.h"
#import "WebFormDataStream.h"
#import "WebFrameLoader.h"
#import <Foundation/NSURLResponse.h>
#import <wtf/Assertions.h>

using namespace WebCore;

@interface WebCoreSubresourceHandle : NSObject <WebCoreResourceHandle>
{
    SubresourceLoader* m_loader;
}
- (id)initWithLoader:(SubresourceLoader*)loader;
@end

namespace WebCore {

SubresourceLoader::SubresourceLoader(Frame* frame, id <WebCoreResourceLoader> l)
    : WebResourceLoader(frame)
    , m_coreLoader(l)
    , m_loadingMultipartContent(false)
{
    frameLoader()->addSubresourceLoader(this);
}

SubresourceLoader::~SubresourceLoader()
{
}

id <WebCoreResourceHandle> SubresourceLoader::create(Frame* frame, id <WebCoreResourceLoader> rLoader,
    NSMutableURLRequest *newRequest, NSString *method, NSDictionary *customHeaders, NSString *referrer)
{
    FrameLoader* fl = [Mac(frame)->bridge() frameLoader];
    if (fl->state() == WebFrameStateProvisional)
        return nil;

    // setHTTPMethod is not called for GET requests to work around <rdar://4464032>.
    if (![method isEqualToString:@"GET"])
        [newRequest setHTTPMethod:method];

    wkSupportsMultipartXMixedReplace(newRequest);

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
        [newRequest setCachePolicy:[fl->originalRequest() cachePolicy]];
    setHTTPReferrer(newRequest, referrer);
    
    fl->addExtraFieldsToRequest(newRequest, false, false);

    RefPtr<SubresourceLoader> loader(new SubresourceLoader(frame, rLoader));
    if (!loader->load(newRequest))
        return nil;
    return loader->handle();
}

id <WebCoreResourceHandle> SubresourceLoader::create(Frame* frame, id <WebCoreResourceLoader> rLoader,
    NSString *method, NSURL *URL, NSDictionary *customHeaders, NSString *referrer)
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];
    id <WebCoreResourceHandle> handle = create(frame, rLoader, newRequest, method, customHeaders, referrer);
    [newRequest release];
    return handle;
}

id <WebCoreResourceHandle> SubresourceLoader::create(Frame* frame, id <WebCoreResourceLoader> rLoader,
    NSString *method, NSURL *URL, NSDictionary *customHeaders, NSArray *postData, NSString *referrer)
{
    NSMutableURLRequest *newRequest = [[NSMutableURLRequest alloc] initWithURL:URL];
    webSetHTTPBody(newRequest, postData);
    id <WebCoreResourceHandle> handle = create(frame, rLoader, newRequest, method, customHeaders, referrer);
    [newRequest release];
    return handle;
}

NSURLRequest *SubresourceLoader::willSendRequest(NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    NSURL *oldURL = [request() URL];
    NSURLRequest *clientRequest = WebResourceLoader::willSendRequest(newRequest, redirectResponse);
    if (clientRequest && oldURL != [clientRequest URL] && ![oldURL isEqual:[clientRequest URL]])
        [m_coreLoader.get() redirectedToURL:[clientRequest URL]];
    return clientRequest;
}

void SubresourceLoader::didReceiveResponse(NSURLResponse *r)
{
    ASSERT(r);

    if ([[r MIMEType] isEqualToString:@"multipart/x-mixed-replace"])
        m_loadingMultipartContent = true;

    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    RefPtr<SubresourceLoader> protect(this);

    [m_coreLoader.get() receivedResponse:r];
    // The coreLoader can cancel a load if it receives a multipart response for a non-image
    if (reachedTerminalState())
        return;
    WebResourceLoader::didReceiveResponse(r);
    
    if (m_loadingMultipartContent && [resourceData() length]) {
        // Since a subresource loader does not load multipart sections progressively,
        // deliver the previously received data to the coreLoader all at once now.
        // Then clear the data to make way for the next multipart section.
        [m_coreLoader.get() addData:resourceData()];
        clearResourceData();
        
        // After the first multipart section is complete, signal to delegates that this load is "finished" 
        didFinishLoadingOnePart();
    }
}

void SubresourceLoader::didReceiveData(NSData *data, long long lengthReceived, bool allAtOnce)
{
    // Reference the object in this method since the additional processing can do
    // anything including removing the last reference to this object; one example of this is 3266216.
    RefPtr<SubresourceLoader> protect(this);

    // A subresource loader does not load multipart sections progressively.
    // So don't deliver any data to the coreLoader yet.
    if (!m_loadingMultipartContent)
        [m_coreLoader.get() addData:data];
    WebResourceLoader::didReceiveData(data, lengthReceived, allAtOnce);
}

void SubresourceLoader::didFinishLoading()
{
    if (cancelled())
        return;
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    [m_coreLoader.get() finishWithData:resourceData()];
    if (cancelled())
        return;
    frameLoader()->removeSubresourceLoader(this);
    WebResourceLoader::didFinishLoading();
}

void SubresourceLoader::didFail(NSError *error)
{
    if (cancelled())
        return;
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    [m_coreLoader.get() reportError];
    if (cancelled())
        return;
    frameLoader()->removeSubresourceLoader(this);
    WebResourceLoader::didFail(error);
}

void SubresourceLoader::didCancel(NSError *error)
{
    ASSERT(!reachedTerminalState());

    // Calling removeSubresourceLoader will likely result in a call to deref, so we must protect ourselves.
    RefPtr<SubresourceLoader> protect(this);

    [m_coreLoader.get() cancel];
    if (cancelled())
        return;
    frameLoader()->removeSubresourceLoader(this);
    WebResourceLoader::didCancel(error);
}

id <WebCoreResourceHandle> SubresourceLoader::handle()
{
    return [[[WebCoreSubresourceHandle alloc] initWithLoader:this] autorelease];
}

}

@implementation WebCoreSubresourceHandle

- (id)initWithLoader:(SubresourceLoader*)loader
{
    self = [self init];
    if (!self)
        return nil;
    loader->ref();
    m_loader = loader;
    return self;
}

- (void)dealloc
{
    m_loader->deref();
    [super dealloc];
}

- (void)finalize
{
    m_loader->deref();
    [super finalize];
}

- (void)cancel
{
    m_loader->cancel();
}

@end
