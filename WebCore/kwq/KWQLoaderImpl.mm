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

@interface KWQURLLoadClient : NSObject <IFURLHandleClient>
{
    khtml::Loader *m_loader;
    WebCoreBridge *m_bridge;
    NSURL *m_currentURL;
}

-(id)initWithLoader:(khtml::Loader *)loader bridge:(WebCoreBridge *)bridge;

@end

@implementation KWQURLLoadClient

-(id)initWithLoader:(Loader *)loader bridge:(WebCoreBridge *)bridge
{
    if ((self = [super init])) {
        m_loader = loader;
        m_bridge = [bridge retain];
        return self;
    }
    
    return nil;
}

- (void)dealloc
{
    // FIXME Radar 2954901: Changed this not to leak because cancel messages are sometimes sent before begin.
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED    
    KWQ_ASSERT(m_currentURL == nil);
#else
    [m_currentURL release];
#endif    
    [m_bridge release];
    [super dealloc];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)handle
{
    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "bridge = %p for URL %s", m_bridge, DEBUG_OBJECT([handle url]));

    KWQ_ASSERT(m_currentURL == nil);

    m_currentURL = [[handle url] retain];
    [m_bridge didStartLoadingWithHandle:handle];
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDataDidBecomeAvailable:(NSData *)data
{
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "bridge = %p for URL %s data at %p, length %d, contentLength %d, contentLengthReceived %d", m_bridge, DEBUG_OBJECT([handle url]), data, [data length], [handle contentLength], [handle contentLengthReceived]);

    KWQ_ASSERT([m_currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);

    void *userData = [[handle attributeForKey:IFURLHandleUserData] pointerValue];
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    m_loader->slotData(job, (const char *)[data bytes], [data length]);
    
    [m_bridge receivedProgressWithHandle:handle];
}

- (void)doneWithHandle:(IFURLHandle *)handle error:(BOOL)error
{
    // FIXME Radar 2954901: Replaced the assertion below with a more lenient one,
    // since cancel messages are sometimes sent before begin.
#ifdef WEBFOUNDATION_LOAD_MESSAGES_FIXED    
    KWQ_ASSERT([m_currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
#else
    KWQ_ASSERT(m_currentURL == nil || [m_currentURL isEqual:[handle redirectedURL] ? [handle redirectedURL] : [handle url]]);
#endif    

    [m_bridge removeHandle:handle];
    
    void *userData = [[handle attributeForKey:IFURLHandleUserData] pointerValue];
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    if (error) {
        job->setError(1);
    }
    m_loader->slotFinished(job);
    delete job;
    
    [m_currentURL release];
    m_currentURL = nil;
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)handle
{
    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "bridge = %p for URL %s", m_bridge, DEBUG_OBJECT([handle url]));

    [m_bridge didCancelLoadingWithHandle:handle];
    [self doneWithHandle:handle error:YES];
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)handle data:(NSData *)data
{
    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "bridge = %p for URL %s data at %p, length %d", m_bridge, DEBUG_OBJECT([handle url]), data, [data length]);
    
    KWQ_ASSERT([handle statusCode] == IFURLHandleStatusLoadComplete);
    KWQ_ASSERT((int)[data length] == [handle contentLengthReceived]);

    [m_bridge didFinishLoadingWithHandle:handle];
    [self doneWithHandle:handle error:NO];
}

- (void)IFURLHandle:(IFURLHandle *)handle resourceDidFailLoadingWithResult:(IFError *)result
{
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "bridge = %p, result = %s, URL = %s", m_bridge, DEBUG_OBJECT([result errorDescription]), DEBUG_OBJECT([handle url]));

    [m_bridge didFailToLoadWithHandle:handle error:result];
    [self doneWithHandle:handle error:YES];
}

- (void)IFURLHandle:(IFURLHandle *)handle didRedirectToURL:(NSURL *)URL
{
    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "url = %s", DEBUG_OBJECT(URL));
    
    void *userData = [[handle attributeForKey:IFURLHandleUserData] pointerValue];
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    KWQ_ASSERT(handle == job->handle());
    KWQ_ASSERT(m_currentURL != nil);

    [m_bridge didRedirectWithHandle:handle fromURL:m_currentURL];
    [URL retain];
    [m_currentURL release];
    m_currentURL = URL;
}

@end

KWQLoaderImpl::KWQLoaderImpl(Loader *l)
    : loader(l)
{
}

void KWQLoaderImpl::setClient(Request *req)
{
    WebCoreBridge *bridge = ((KHTMLPart *)req->m_docLoader->part())->impl->getBridge();
    req->client = [[[KWQURLLoadClient alloc] initWithLoader:loader bridge:bridge] autorelease];
}

void KWQLoaderImpl::serveRequest(Request *req, KIO::TransferJob *job)
{
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "Serving request for base %s, url %s", 
          req->m_docLoader->part()->baseURL().url().latin1(), req->object->url().string().latin1());
    
    job->begin(req->client, job);
    if (job->handle() == nil) {
        // The only error that prevents us from making a handle is a malformed URL.
        IFError *error = [IFError errorWithCode:IFURLHandleResultBadURLError inDomain:IFErrorCodeDomainWebFoundation isTerminal:YES];
        [req->client->m_bridge didFailBeforeLoadingWithError:error];
    } else {
        [req->client->m_bridge addHandle:job->handle()];
    }
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
    // Maybe the notification should have a distinctive name, and the URL should be the object.
    [[NSNotificationCenter defaultCenter] postNotificationName:urlString object:nil];
}
