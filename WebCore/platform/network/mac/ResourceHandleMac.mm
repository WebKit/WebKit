/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ResourceHandle.h"
#import "ResourceHandleInternal.h"

#import "BlockExceptions.h"
#import "DocLoader.h"
#import "FrameLoader.h"
#import "FrameMac.h"
#import "ResourceError.h"
#import "ResourceResponse.h"
#import "SubresourceLoader.h"
#import "WebCoreSystemInterface.h"

using namespace WebCore;

@interface WebCoreResourceHandleAsDelegate : NSObject <NSURLAuthenticationChallengeSender>
{
    ResourceHandle* m_handle;
}
- (id)initWithHandle:(ResourceHandle*)handle;
- (void)detachHandle;
@end

@interface NSURLConnection (NSURLConnectionTigerPrivate)
- (NSData *)_bufferedData;
@end

@interface NSURLProtocol (WebFoundationSecret) 
+ (void)_removePropertyForKey:(NSString *)key inRequest:(NSMutableURLRequest *)request;
@end

namespace WebCore {
   
static unsigned inNSURLConnectionCallback;
static bool NSURLConnectionSupportsBufferedData;
    
#ifndef NDEBUG
static bool isInitializingConnection;
#endif
    
ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    releaseDelegate();
}

bool ResourceHandle::start(Frame* frame)
{
    if (!frame)
        return false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    // If we are no longer attached to a Page, this must be an attempted load from an
    // onUnload handler, so let's just block it.
    if (!frame->page())
        return false;
  
#ifndef NDEBUG
    isInitializingConnection = YES;
#endif
    id delegate;
    
    if (d->m_mightDownloadFromHandle) {
        ASSERT(!d->m_proxy);
        d->m_proxy = wkCreateNSURLConnectionDelegateProxy();
        [d->m_proxy.get() setDelegate:ResourceHandle::delegate()];
        [d->m_proxy.get() release];
        
        delegate = d->m_proxy.get();
    } else 
        delegate = ResourceHandle::delegate();
    
    NSURLConnection *connection = [[NSURLConnection alloc] initWithRequest:d->m_request.nsURLRequest() delegate:delegate];
#ifndef NDEBUG
    isInitializingConnection = NO;
#endif
    d->m_connection = connection;
    [connection release];
    if (d->m_defersLoading)
        wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), YES);
    
    if (d->m_connection)
        return true;

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

void ResourceHandle::cancel()
{
    [d->m_connection.get() cancel];
}

void ResourceHandle::setDefersLoading(bool defers)
{
    d->m_defersLoading = defers;
    wkSetNSURLConnectionDefersCallbacks(d->m_connection.get(), defers);
}

WebCoreResourceHandleAsDelegate *ResourceHandle::delegate()
{
    if (!d->m_delegate) {
        WebCoreResourceHandleAsDelegate *delegate = [[WebCoreResourceHandleAsDelegate alloc] initWithHandle:this];
        d->m_delegate = delegate;
        [delegate release];
    }
    return d->m_delegate.get();
}

void ResourceHandle::releaseDelegate()
{
    if (!d->m_delegate)
        return;
    [d->m_delegate.get() detachHandle];
    d->m_delegate = nil;
}

bool ResourceHandle::supportsBufferedData()
{
    static bool initialized = false;
    if (!initialized) {
        NSURLConnectionSupportsBufferedData = [NSURLConnection instancesRespondToSelector:@selector(_bufferedData)];
        initialized = true;
    }

    return NSURLConnectionSupportsBufferedData;
}

NSData* ResourceHandle::bufferedData()
{
    if (ResourceHandle::supportsBufferedData())
        return [d->m_connection.get() _bufferedData];

    return nil;
}

id ResourceHandle::releaseProxy()
{
    id proxy = [[d->m_proxy.get() retain] autorelease];
    d->m_proxy = nil;
    [proxy setDelegate:nil];
    return proxy;
}

NSURLConnection *ResourceHandle::connection() const
{
    return d->m_connection.get();
}

bool ResourceHandle::loadsBlocked()
{
    return inNSURLConnectionCallback != 0;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    NSError *nsError = nil;
    
    NSURLResponse *nsURLResponse = nil;
    NSData *result = nil;

    ASSERT(!request.isEmpty());
    
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    result = [NSURLConnection sendSynchronousRequest:request.nsURLRequest() returningResponse:&nsURLResponse error:&nsError];
    
    END_BLOCK_OBJC_EXCEPTIONS;

    if (nsError == nil)
        response = nsURLResponse;
    else {
        response = ResourceResponse(request.url(), String(), 0, String(), String());
        if ([nsError domain] == NSURLErrorDomain)
            response.setHTTPStatusCode([nsError code]);
        else
            response.setHTTPStatusCode(404);       
    }
    
    data.resize([result length]);
    memcpy(data.data(), [result bytes], [result length]);
    
    error = nsError;
}

} // namespace WebCore

@implementation WebCoreResourceHandleAsDelegate

- (id)initWithHandle:(ResourceHandle*)handle
{
    self = [self init];
    if (!self)
        return nil;
    m_handle = handle;
    return self;
}

- (void)detachHandle
{
    m_handle = 0;
}

- (NSURLRequest *)connection:(NSURLConnection *)con willSendRequest:(NSURLRequest *)newRequest redirectResponse:(NSURLResponse *)redirectResponse
{
    if (!m_handle)
        return nil;
    ++inNSURLConnectionCallback;
    ResourceRequest request = newRequest;
    m_handle->client()->willSendRequest(m_handle, request, redirectResponse);
    --inNSURLConnectionCallback;
    return request.nsURLRequest();
}

- (void)connection:(NSURLConnection *)con didReceiveAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->didReceiveAuthenticationChallenge(m_handle, challenge);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didCancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->didCancelAuthenticationChallenge(m_handle, challenge);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didReceiveResponse:(NSURLResponse *)r
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->didReceiveResponse(m_handle, r);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didReceiveData:(NSData *)data lengthReceived:(long long)lengthReceived
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->didReceiveData(m_handle, (const char*)[data bytes], [data length], lengthReceived);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con willStopBufferingData:(NSData *)data
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->willStopBufferingData(m_handle, data);
    --inNSURLConnectionCallback;
}

- (void)connectionDidFinishLoading:(NSURLConnection *)con
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->didFinishLoading(m_handle);
    --inNSURLConnectionCallback;
}

- (void)connection:(NSURLConnection *)con didFailWithError:(NSError *)error
{
    if (!m_handle)
        return;
    ++inNSURLConnectionCallback;
    m_handle->client()->didFail(m_handle, error);
    --inNSURLConnectionCallback;
}

- (NSCachedURLResponse *)connection:(NSURLConnection *)connection willCacheResponse:(NSCachedURLResponse *)cachedResponse
{
#ifndef NDEBUG
    if (isInitializingConnection)
        LOG_ERROR("connection:willCacheResponse: was called inside of [NSURLConnection initWithRequest:delegate:] (40676250)");
#endif
    if (!m_handle)
        return nil;
    ++inNSURLConnectionCallback;
    NSCachedURLResponse *result = m_handle->client()->willCacheResponse(m_handle, cachedResponse);
    --inNSURLConnectionCallback;
    return result;
}

- (void)useCredential:(NSURLCredential *)credential forAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->client()->receivedCredential(m_handle, challenge, credential);
}

- (void)continueWithoutCredentialForAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->client()->receivedRequestToContinueWithoutCredential(m_handle, challenge);
}

- (void)cancelAuthenticationChallenge:(NSURLAuthenticationChallenge *)challenge
{
    if (!m_handle)
        return;
    m_handle->client()->receivedCancellation(m_handle, challenge);
}

@end
