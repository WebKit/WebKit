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

#include <KWQLoaderImpl.h>

#include <kio/jobclasses.h>
#include <misc/loader.h>
#include <KWQKHTMLPartImpl.h>

#include <external.h>
#include <WCLoadProgress.h>

#include <kwqdebug.h>

using khtml::DocLoader;
using khtml::Loader;
using khtml::Request;

WCIFLoadProgressMakeFunc WCIFLoadProgressMake;

void WCSetIFLoadProgressMakeFunc(WCIFLoadProgressMakeFunc func)
{
    WCIFLoadProgressMake = func;
}

@implementation URLLoadClient

-(id)initWithLoader:(Loader *)loader dataSource: dataSource
{
    if ((self = [super init])) {
        m_loader = loader;
        m_dataSource = [dataSource retain];
        return self;
    }
    
    return nil;
}

- (void)dealloc
{
    [m_dataSource autorelease];
    [super dealloc];
}

- (void)IFURLHandleResourceDidBeginLoading:(IFURLHandle *)sender
{
    id controller;
	int contentLength = [sender contentLength];
	int contentLengthReceived = [sender contentLengthReceived];
    void *userData;

    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);

    KWQDEBUGLEVEL(KWQ_LOG_LOADING, "dataSource = %p for URL %s\n", m_dataSource, DEBUG_OBJECT(job->url()));

    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = contentLength;
    loadProgress->bytesSoFar = contentLengthReceived;
    
    controller = [m_dataSource controller];
    [controller _receivedProgress: loadProgress forResource: [job->url() absoluteString] fromDataSource: m_dataSource];
    [controller _didStartLoading:job->url()];
}

- (void)IFURLHandleResourceDidCancelLoading:(IFURLHandle *)sender
{
    id controller;
    void *userData;
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);

    [m_dataSource _removeURLHandle: job->handle()];
    
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p for URL %s\n", m_dataSource, DEBUG_OBJECT(job->url()));

    job->setError(1);
    m_loader->slotFinished(job);
    
    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = -1;
    loadProgress->bytesSoFar = -1;

    controller = [m_dataSource controller];
    [controller _receivedProgress: loadProgress forResource: [job->url() absoluteString] fromDataSource: m_dataSource];

    [controller _didStopLoading:job->url()];

    delete job;
}

- (void)IFURLHandleResourceDidFinishLoading:(IFURLHandle *)sender data: (NSData *)data
{
    id controller;
    void *userData;
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);

    [m_dataSource _removeURLHandle: job->handle()];
    
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p for URL %s data at %p, length %d\n", m_dataSource, DEBUG_OBJECT(job->url()), data, [data length]);

    m_loader->slotFinished(job);
    
    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = [data length];
    loadProgress->bytesSoFar = [data length];

    controller = [m_dataSource controller];
    [controller _receivedProgress: loadProgress forResource: [job->url() absoluteString] fromDataSource: m_dataSource];

    [controller _didStopLoading:job->url()];

    delete job;
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDataDidBecomeAvailable:(NSData *)data
{
    void *userData;
    int contentLength = [sender contentLength];
    int contentLengthReceived = [sender contentLengthReceived];
    
    userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p for URL %s data at %p, length %d\n", m_dataSource, DEBUG_OBJECT(job->url()), data, [data length]);

    m_loader->slotData(job, (const char *)[data bytes], [data length]);    

    // Don't send the last progress message, it will be sent via
    // IFURLHandleResourceDidFinishLoading
    if (contentLength == contentLengthReceived &&
    	contentLength != -1){
    	return;
    }
    
    id controller;

    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = contentLength;
    loadProgress->bytesSoFar = contentLengthReceived;
    
    controller = [m_dataSource controller];
    [controller _receivedProgress: loadProgress forResource: [job->url() absoluteString] fromDataSource: m_dataSource];
}

- (void)IFURLHandle:(IFURLHandle *)sender resourceDidFailLoadingWithResult:(IFError *)result
{
    void *userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "dataSource = %p, result = %s, URL = %s\n", m_dataSource, DEBUG_OBJECT([result errorDescription]), DEBUG_OBJECT(job->url()));

    [m_dataSource _removeURLHandle: job->handle()];

    id <IFLoadHandler> controller = [m_dataSource controller];
    
    IFLoadProgress *loadProgress = WCIFLoadProgressMake();
    loadProgress->totalToLoad = [sender contentLength];
    loadProgress->bytesSoFar = [sender contentLengthReceived];

    job->setError(1);
    m_loader->slotFinished(job);

    [(IFBaseWebController *)controller _receivedError: result forResource: [job->url() absoluteString] partialProgress: loadProgress fromDataSource: m_dataSource];

    [(IFBaseWebController *)controller _didStopLoading:job->url()];

    delete job;
}

- (void)IFURLHandle:(IFURLHandle *)sender didRedirectToURL:(NSURL *)url
{
    void *userData = [[sender attributeForKey:IFURLHandleUserData] pointerValue];
    KIO::TransferJob *job = static_cast<KIO::TransferJob *>(userData);
    NSURL *oldURL = job->url();

    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "url = %s\n", DEBUG_OBJECT(url));
    [m_dataSource _part]->impl->setBaseURL([[url absoluteString] cString]);
    
    [m_dataSource _setFinalURL: url];
    
    [(id <IFLocationChangeHandler>)[m_dataSource controller] serverRedirectTo: url forDataSource: m_dataSource];
    [(IFBaseWebController *)[m_dataSource controller] _didStopLoading:oldURL];
    [(IFBaseWebController *)[m_dataSource controller] _didStartLoading:url];
}

@end

KWQLoaderImpl::KWQLoaderImpl(Loader *l)
    : loader(l)
{
}

KWQLoaderImpl::~KWQLoaderImpl()
{
}

void KWQLoaderImpl::setClient(Request *req)
{
    IFWebDataSource *dataSource = ((KHTMLPart *)((DocLoader *)req->object->loader())->part())->impl->getDataSource();
    req->client = [[[URLLoadClient alloc] initWithLoader:loader dataSource: dataSource] autorelease];
}

void KWQLoaderImpl::serveRequest(Request *req, KIO::TransferJob *job)
{
    KWQDEBUGLEVEL (KWQ_LOG_LOADING, "Serving request for base %s, url %s\n", 
          req->m_docLoader->part()->baseURL().url().latin1(), req->object->url().string().latin1());
    //job->begin(d->m_recv, job);
    
    job->begin((URLLoadClient *)req->client, job);
    if (job->handle() == nil){
        // Must be a malformed URL.
        NSString *urlString = QSTRING_TO_NSSTRING(req->object->url().string());
        IFError *error = [IFError errorWithCode:IFURLHandleResultBadURLError inDomain:IFErrorCodeDomainWebFoundation isTerminal:YES];

        id <IFLoadHandler> controller = [((URLLoadClient *)req->client)->m_dataSource controller];
        [(IFBaseWebController *)controller _receivedError: error forResource: urlString partialProgress: nil fromDataSource: ((URLLoadClient *)req->client)->m_dataSource];
    }
    else {
        [((URLLoadClient *)req->client)->m_dataSource _addURLHandle: job->handle()];
    }
}

void KWQLoaderImpl::objectFinished(khtml::CachedObject *object)
{
    NSString *urlString;
    
    urlString = [NSString stringWithCString:object->url().string().latin1()];
    if ([urlString hasSuffix:@"/"]) {
        urlString = [urlString substringToIndex:([urlString length] - 1)];
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:urlString object:nil];
}
