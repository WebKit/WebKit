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

#import <kio/jobclasses.h>
#import <misc/loader.h>

#import <WebFoundation/IFError.h>
#import <WebFoundation/IFURLHandle.h>

#import <KWQKHTMLPartImpl.h>
#import <WebCoreBridge.h>
#import <kwqdebug.h>

using khtml::DocLoader;
using khtml::Loader;
using khtml::Request;

@interface WebCoreResourceLoader : NSObject <WebCoreResourceLoader>
{
    khtml::Loader *loader;
    KIO::TransferJob *job;
}

-(id)initWithLoader:(khtml::Loader *)loader job:(KIO::TransferJob *)job;

@end

@implementation WebCoreResourceLoader

-(id)initWithLoader:(khtml::Loader *)l job:(KIO::TransferJob *)j;
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

KWQLoaderImpl::KWQLoaderImpl(Loader *l)
    : loader(l)
{
}

void KWQLoaderImpl::serveRequest(Request *req, KIO::TransferJob *job)
{
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "Serving request for base %s, url %s", 
          req->m_docLoader->part()->baseURL().url().latin1(), req->object->url().string().latin1());
    
    WebCoreResourceLoader *resourceLoader = [[WebCoreResourceLoader alloc] initWithLoader:loader job:job];
    
    WebCoreBridge *bridge = ((KHTMLPart *)req->m_docLoader->part())->impl->getBridge();
    job->setHandle([bridge startLoadingResource:resourceLoader withURL:job->url()]);
    
    [resourceLoader release];
}

void KWQLoaderImpl::objectFinished(khtml::CachedObject *object)
{
    NSString *urlString;
    
    urlString = [NSString stringWithCString:object->url().string().latin1()];
    if ([urlString hasSuffix:@"/"]) {
        urlString = [urlString substringToIndex:([urlString length] - 1)];
    }
    
    // FIXME: Are we sure that we can globally notify with the URL as the notification
    // name without conflicting with other frameworks? No "IF" prefix needed?
    // Perhaps the notification should have a distinctive name, and the URL should be the object.
    [[NSNotificationCenter defaultCenter] postNotificationName:urlString object:nil];
}
