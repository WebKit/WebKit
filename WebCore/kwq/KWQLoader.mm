/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQLoader.h"

#import "KWQExceptions.h"
#import "KWQKJobClasses.h"
#import "KWQLogging.h"
#import "KWQResourceLoader.h"
#import "KWQFoundationExtras.h"
#import "WebCoreBridge.h"
#import "khtml_part.h"
#import "loader.h"

#import <Foundation/NSURLResponse.h>

using khtml::Cache;
using khtml::CachedObject;
using khtml::CachedImage;
using khtml::DocLoader;
using khtml::Loader;
using khtml::Request;
using KIO::TransferJob;

bool KWQServeRequest(Loader *loader, Request *request, TransferJob *job)
{
    LOG(Loading, "Serving request for base %s, url %s", 
        request->m_docLoader->part()->baseURL().url().latin1(),
        request->object->url().string().latin1());
    
    return KWQServeRequest(loader, request->m_docLoader, job);
}

@interface NSDictionary (WebCore_Extras)
- (id)_webcore_initWithHeaderString:(NSString *)string;
@end

@implementation NSDictionary (WebCore_Extras)
- (id)_webcore_initWithHeaderString:(NSString *)string
{
    NSMutableDictionary *headers = [[NSMutableDictionary alloc] init];

    NSArray *lines = [string componentsSeparatedByString:@"\r\n"];

    NSEnumerator *e = [lines objectEnumerator];
    NSString *line;

    NSString *lastHeaderName = nil;

    while ((line = (NSString *)[e nextObject]) != nil) {
	if (([line characterAtIndex:0] == ' ' || [line characterAtIndex:0] == '\t')
	    && lastHeaderName != nil) {
	    // lines that start with space or tab continue the previous header value
	    NSString *oldVal = [headers objectForKey:lastHeaderName];
	    ASSERT(oldVal);
	    [headers setObject:[NSString stringWithFormat:@"%@ %@", oldVal, [line stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" \t"]]]
	                forKey:lastHeaderName];
	    continue;
	}

	NSRange colonRange = [line rangeOfString:@":"];
	if (colonRange.location != NSNotFound) {
	    // don't worry about case, assume lower levels will take care of it

	    NSString *headerName = [[line substringToIndex:colonRange.location] stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" \t"]];
	    NSString *headerValue = [[line substringFromIndex:colonRange.location + 1] stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" \t"]];
	    
	    NSString *oldVal = [headers objectForKey:headerName];
	    if (oldVal) {
		headerValue = [NSString stringWithFormat:@"%@, %@", oldVal, headerValue];
	    }

	    [headers setObject:headerValue forKey:headerName];
	    
	    lastHeaderName = headerName;
	}
    }

    self = [self initWithDictionary:headers];
    [headers release];
    return self;
}

@end

bool KWQServeRequest(Loader *loader, DocLoader *docLoader, TransferJob *job)
{
    KWQKHTMLPart *part = static_cast<KWQKHTMLPart *>(docLoader->part());
    WebCoreBridge *bridge = part->bridge();

    part->didTellBridgeAboutLoad(job->url().url());

    KWQ_BLOCK_EXCEPTIONS;
    KWQResourceLoader *resourceLoader = [[KWQResourceLoader alloc] initWithJob:job];

    id <WebCoreResourceHandle> handle;

    NSDictionary *headerDict = nil;
    QString headerString = job->queryMetaData("customHTTPHeader");

    if (!headerString.isEmpty()) {
	headerDict = [[NSDictionary alloc] _webcore_initWithHeaderString:headerString.getNSString()];
    }

    if (job->method() == "POST") {
	NSData *postData = [NSData dataWithBytesNoCopy:job->postData().data() length:job->postData().size() freeWhenDone:NO];
	handle = [bridge startLoadingResource:resourceLoader withURL:job->url().getNSURL() customHeaders:headerDict postData:postData];
    } else {
	handle = [bridge startLoadingResource:resourceLoader withURL:job->url().getNSURL() customHeaders:headerDict];
    }
    [resourceLoader setHandle:handle];
    [resourceLoader release];
    return handle != nil;
    KWQ_UNBLOCK_EXCEPTIONS;

    return true;
}

static NSString *KWQHeaderStringFromDictionary(NSDictionary *headers, int statusCode)
{
    NSMutableString *headerString = [[NSMutableString alloc] init];
    [headerString appendString:[NSString stringWithFormat:@"HTTP/1.0 %d OK\n", statusCode]];
    
    NSEnumerator *e = [headers keyEnumerator];
    NSString *key;
    
    bool first = true;
    
    while ((key = [e nextObject]) != nil) {
	if (first) {
	    first = false;
	} else {
	    [headerString appendString:@"\n"];
	}
	[headerString appendString:key];
	[headerString appendString:@": "];
	[headerString appendString:[headers objectForKey:key]];
    }
	
    return headerString;
}

QByteArray KWQServeSynchronousRequest(Loader *loader, DocLoader *docLoader, TransferJob *job, KURL &finalURL, QString &responseHeaders)
{
    KWQKHTMLPart *part = static_cast<KWQKHTMLPart *>(docLoader->part());
    WebCoreBridge *bridge = part->bridge();

    part->didTellBridgeAboutLoad(job->url().url());

    KWQ_BLOCK_EXCEPTIONS;

    NSDictionary *headerDict = nil;
    QString headerString = job->queryMetaData("customHTTPHeader");

    if (!headerString.isEmpty()) {
	headerDict = [[NSDictionary alloc] _webcore_initWithHeaderString:headerString.getNSString()];
    }

    NSData *postData = nil;
    

    if (job->method() == "POST") {
	postData = [NSData dataWithBytesNoCopy:job->postData().data() length:job->postData().size() freeWhenDone:NO];
    }

    NSURL *finalNSURL = nil;
    NSDictionary *responseHeaderDict = nil;
    int statusCode = 0;
    NSData *resultData = [bridge syncLoadResourceWithURL:job->url().getNSURL() customHeaders:headerDict postData:postData finalURL:&finalNSURL responseHeaders:&responseHeaderDict statusCode:&statusCode];
    
    job->kill();

    finalURL = finalNSURL;
    responseHeaders = QString::fromNSString(KWQHeaderStringFromDictionary(responseHeaderDict, statusCode));

    QByteArray results([resultData length]);

    memcpy( results.data(), [resultData bytes], [resultData length] );

    return results;

    KWQ_UNBLOCK_EXCEPTIONS;

    return QByteArray();
}

int KWQNumberOfPendingOrLoadingRequests(khtml::DocLoader *dl)
{
    return Cache::loader()->numRequests(dl);
}

bool KWQCheckIfReloading(DocLoader *loader)
{
    KWQ_BLOCK_EXCEPTIONS;
    return [static_cast<KWQKHTMLPart *>(loader->part())->bridge() isReloading];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
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
    KWQKHTMLPart *part = static_cast<KWQKHTMLPart *>(loader->part());

    QString urlString = cachedObject->url().string();

    if (!part->haveToldBridgeAboutLoad(urlString)) {
	WebCoreBridge *bridge = part->bridge();
	CachedImage *cachedImage = dynamic_cast<CachedImage *>(cachedObject);

	KWQ_BLOCK_EXCEPTIONS;
	[bridge objectLoadedFromCacheWithURL:KURL(cachedObject->url().string()).getNSURL()
	        response:(id)cachedObject->response()
	        size:cachedImage ? cachedImage->dataSize() : cachedObject->size()];
	KWQ_UNBLOCK_EXCEPTIONS;

	part->didTellBridgeAboutLoad(urlString);
    }
}

void KWQRetainResponse(void *response)
{
    // There's no way a retain can raise
    KWQRetain((id)response);
}

void KWQReleaseResponse(void *response)
{
    // A release could raise if it deallocs.
    KWQ_BLOCK_EXCEPTIONS;
    KWQRelease((id)response);
    KWQ_UNBLOCK_EXCEPTIONS;
}

void *KWQResponseMIMEType(void *response)
{
    KWQ_BLOCK_EXCEPTIONS;
    return [(NSURLResponse *)response MIMEType];
    KWQ_UNBLOCK_EXCEPTIONS;

    return NULL;
}

void *KWQResponseHeaderString(void *response)
{
    KWQ_BLOCK_EXCEPTIONS;
    NSURLResponse *nsResponse = (NSURLResponse *)response;
    if ([nsResponse isKindOfClass:[NSHTTPURLResponse class]]) {
	NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)nsResponse;
	NSDictionary *headers = [httpResponse allHeaderFields];

	return KWQHeaderStringFromDictionary(headers, [httpResponse statusCode]);
    }

    KWQ_UNBLOCK_EXCEPTIONS;

    return NULL;
}

time_t KWQCacheObjectExpiresTime(khtml::DocLoader *docLoader, void *response)
{
    KWQ_BLOCK_EXCEPTIONS;
    
    KWQKHTMLPart *part = static_cast<KWQKHTMLPart *>(docLoader->part());
    WebCoreBridge *bridge = part->bridge();
    return [bridge expiresTimeForResponse:(NSURLResponse *)response];
    
    KWQ_UNBLOCK_EXCEPTIONS;
    
    return 0;
}

KWQLoader::KWQLoader(Loader *loader)
    : _requestStarted(loader, SIGNAL(requestStarted(khtml::DocLoader *, khtml::CachedObject *)))
    , _requestDone(loader, SIGNAL(requestDone(khtml::DocLoader *, khtml::CachedObject *)))
    , _requestFailed(loader, SIGNAL(requestFailed(khtml::DocLoader *, khtml::CachedObject *)))
{
}
