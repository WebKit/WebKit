/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <KWQLoaderImpl.h>

#import <jobclasses.h>
#import <loader.h>

#import <khtml_part.h>

#import <WebCoreBridge.h>
#import <WebCoreResourceLoader.h>

#import <KWQLogging.h>

using khtml::CachedObject;
using khtml::CachedImage;
using khtml::DocLoader;
using khtml::Loader;
using khtml::Request;
using KIO::TransferJob;

@interface WebCoreResourceLoader : NSObject <WebCoreResourceLoader>
{
    Loader *loader;
    TransferJob *job;
}

-(id)initWithLoader:(Loader *)loader job:(TransferJob *)job;

@end

@implementation WebCoreResourceLoader

-(id)initWithLoader:(Loader *)l job:(TransferJob *)j;
{
    [super init];
    
    loader = l;
    job = j;
    
    return self;
}

- (void)dealloc
{
    delete job;
    [super dealloc];
}

- (void)receivedResponse:(id)response
{
    ASSERT(response);
    loader->receivedResponse(job, response);
}

- (void)addData:(NSData *)data
{
    loader->slotData(job, (const char *)[data bytes], [data length]);
}

- (void)cancel
{
    job->setError(1);
    job->setHandle(0);
    job = 0;
}

- (void)finish
{
    loader->slotFinished(job);
    job->setHandle(0);
}

@end

bool KWQServeRequest(Loader *loader, Request *request, TransferJob *job)
{
    LOG(Loading, "Serving request for base %s, url %s", 
        request->m_docLoader->part()->baseURL().url().latin1(),
        request->object->url().string().latin1());
    
    WebCoreBridge *bridge = ((KHTMLPart *)request->m_docLoader->part())->impl->bridge();

    NSURL *URL = job->url().getNSURL();
    if (URL == nil) {
        [bridge reportBadURL:job->url().url().getNSString()];
        delete job;
        return false;
    }
    
    WebCoreResourceLoader *resourceLoader = [[WebCoreResourceLoader alloc] initWithLoader:loader job:job];
    id <WebCoreResourceHandle> handle = [bridge startLoadingResource:resourceLoader withURL:URL];
    [resourceLoader release];

    if (handle == nil) {
        return false;
    }
    
    job->setHandle(handle);
    return true;
}

bool KWQCheckIfReloading(DocLoader *loader)
{
    return [((KHTMLPart *)loader->part())->impl->bridge() isReloading];
}

void KWQCheckCacheObjectStatus(DocLoader *loader, CachedObject *cachedObject)
{
    // Return from the function for objects that we didn't load from the cache.
    if (!cachedObject)
        return;
    switch (cachedObject->status()) {
    case CachedObject::Persistent:
    case CachedObject::Cached:
    case CachedObject::Uncacheable:
        break;
    case CachedObject::NotCached:
    case CachedObject::Unknown:
    case CachedObject::New:
    case CachedObject::Pending:
        return;
    }
    
    ASSERT(cachedObject->response());
    
    // Notify the caller that we "loaded".
    WebCoreBridge *bridge = ((KHTMLPart *)loader->part())->impl->bridge();
    NSURL *URL = [[NSURL alloc] initWithString:cachedObject->url().string().getNSString()];
    ASSERT(URL);
    CachedImage *cachedImage = dynamic_cast<CachedImage *>(cachedObject);
    [bridge objectLoadedFromCache:URL response:(id)cachedObject->response() size:cachedImage ? cachedImage->dataSize() : cachedObject->size()];
    [URL release];
}

void KWQRetainResponse(void *response)
{
    [(id)response retain];
}

void KWQReleaseResponse(void *response)
{
    [(id)response release];
}


