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

#import <kwqdebug.h>

using khtml::CachedObject;
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

- (void)addData:(NSData *)data
{
    loader->slotData(job, (const char *)[data bytes], [data length]);
}

- (void)cancel
{
    job->setError(1);
    [self finish];
}

- (void)finish
{
    loader->slotFinished(job);
    job->setHandle(0);
}

@end

void KWQServeRequest(Loader *loader, Request *request, TransferJob *job)
{
    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "Serving request for base %s, url %s", 
        request->m_docLoader->part()->baseURL().url().latin1(),
        request->object->url().string().latin1());
    
    WebCoreResourceLoader *resourceLoader = [[WebCoreResourceLoader alloc] initWithLoader:loader job:job];
    
    WebCoreBridge *bridge = ((KHTMLPart *)request->m_docLoader->part())->impl->getBridge();
    job->setHandle([bridge startLoadingResource:resourceLoader withURL:job->url()]);
    
    [resourceLoader release];
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
    
    // Notify the caller that we "loaded".
    WebCoreBridge *bridge = ((KHTMLPart *)loader->part())->impl->getBridge();
    NSURL *nsURL = [[NSURL alloc] initWithString:cachedObject->url().string().getNSString()];
    // FIXME: cachedObject-size() is not the correct size, see Radar 2965269
    [bridge objectLoadedFromCache:nsURL size:cachedObject->size()];
    [nsURL release];
}