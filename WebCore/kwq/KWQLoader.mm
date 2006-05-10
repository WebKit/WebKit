/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQLoader.h"

#import "BlockExceptions.h"
#import "Cache.h"
#import "CachedImage.h"
#import "DocLoader.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "KWQFormData.h"
#import "KWQResourceLoader.h"
#import "Logging.h"
#import "Request.h"
#import "TransferJob.h"
#import "WebCoreFrameBridge.h"
#import "loader.h"
#import <Foundation/NSURLResponse.h>

using namespace WebCore;

@implementation NSDictionary (WebCore_Extras)

+ (id)_webcore_dictionaryWithHeaderString:(NSString *)string
{
    NSMutableDictionary *headers = [[NSMutableDictionary alloc] init];

    NSArray *lines = [string componentsSeparatedByString:@"\r\n"];
    NSEnumerator *e = [lines objectEnumerator];
    NSString *lastHeaderName = nil;

    while (NSString *line = (NSString *)[e nextObject]) {
        if ([line length]) {
            unichar firstChar = [line characterAtIndex:0];
            if ((firstChar == ' ' || firstChar == '\t') && lastHeaderName != nil) {
                // lines that start with space or tab continue the previous header value
                NSString *oldVal = [headers objectForKey:lastHeaderName];
                ASSERT(oldVal);
                NSString *newVal = [line stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" \t"]];
                [headers setObject:[NSString stringWithFormat:@"%@ %@", oldVal, newVal]
                            forKey:lastHeaderName];
                continue;
            }
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

    NSDictionary *dictionary = [NSDictionary dictionaryWithDictionary:headers];
    
    [headers release];
    
    return dictionary;
}

@end

NSString *KWQHeaderStringFromDictionary(NSDictionary *headers, int statusCode)
{
    NSMutableString *headerString = [[[NSMutableString alloc] init] autorelease];
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

DeprecatedByteArray KWQServeSynchronousRequest(Loader *loader, DocLoader *docLoader, TransferJob *job, KURL &finalURL, DeprecatedString &responseHeaders)
{
    FrameMac *frame = static_cast<FrameMac *>(docLoader->frame());
    
    if (!frame)
        return DeprecatedByteArray();
    
    WebCoreFrameBridge *bridge = frame->bridge();

    frame->didTellBridgeAboutLoad(job->url().url());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSDictionary *headerDict = nil;
    String headerString = job->queryMetaData("customHTTPHeader");

    if (!headerString.isEmpty())
        headerDict = [[NSDictionary _webcore_dictionaryWithHeaderString:headerString] retain];

    NSArray *postData = nil;
    if (job->postData().count() > 0)
        postData = arrayFromFormData(job->postData());

    NSURL *finalNSURL = nil;
    NSDictionary *responseHeaderDict = nil;
    int statusCode = 0;
    NSData *resultData = [bridge syncLoadResourceWithMethod:job->method() URL:job->url().getNSURL() customHeaders:headerDict postData:postData finalURL:&finalNSURL responseHeaders:&responseHeaderDict statusCode:&statusCode];
    [headerDict release];
    
    job->kill();

    finalURL = finalNSURL;
    responseHeaders = DeprecatedString::fromNSString(KWQHeaderStringFromDictionary(responseHeaderDict, statusCode));

    DeprecatedByteArray results([resultData length]);

    memcpy( results.data(), [resultData bytes], [resultData length] );

    return results;

    END_BLOCK_OBJC_EXCEPTIONS;

    return DeprecatedByteArray();
}

int KWQNumberOfPendingOrLoadingRequests(WebCore::DocLoader *dl)
{
    return Cache::loader()->numRequests(dl);
}

bool KWQCheckIfReloading(DocLoader *loader)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (FrameMac *frame = static_cast<FrameMac *>(loader->frame()))
        return [frame->bridge() isReloading];
    END_BLOCK_OBJC_EXCEPTIONS;

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
    FrameMac *frame = static_cast<FrameMac *>(loader->frame());

    if (frame && !frame->haveToldBridgeAboutLoad(cachedObject->url())) {
        WebCoreFrameBridge *bridge = frame->bridge();
        
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        [bridge objectLoadedFromCacheWithURL:KURL(cachedObject->url().deprecatedString()).getNSURL()
                                    response:(NSURLResponse *)cachedObject->response()
                                        data:(NSData *)cachedObject->allData()];
        END_BLOCK_OBJC_EXCEPTIONS;

        frame->didTellBridgeAboutLoad(cachedObject->url());
    }
}

bool KWQIsResponseURLEqualToURL(NSURLResponse *response, const WebCore::String& m_url)
{
    NSURL *responseURL = [(NSURLResponse *)response URL];
    NSString *urlString = [responseURL absoluteString];

    size_t length = m_url.length();
    if (length != [urlString length])
        return false;

    Vector<UChar, 1024> buffer(length);
    [urlString getCharacters:buffer.data()];    
    return !memcmp(buffer.data(), m_url.characters(), length * sizeof(UChar));
}

DeprecatedString KWQResponseURL(NSURLResponse *response)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSURL *responseURL = [(NSURLResponse *)response URL];
    NSString *urlString = [responseURL absoluteString];
    
    DeprecatedString string;
    string.setBufferFromCFString((CFStringRef)urlString);
    return string;

    END_BLOCK_OBJC_EXCEPTIONS;
    
    return NULL;
}

DeprecatedString KWQResponseMIMEType(NSURLResponse *response)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return DeprecatedString::fromNSString([(NSURLResponse *)response MIMEType]);
    END_BLOCK_OBJC_EXCEPTIONS;

    return DeprecatedString();
}

bool KWQResponseIsMultipart(NSURLResponse *response)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    return [[response MIMEType] isEqualToString:@"multipart/x-mixed-replace"];
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return false;
}

time_t KWQCacheObjectExpiresTime(WebCore::DocLoader *docLoader, NSURLResponse *response)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    FrameMac *frame = static_cast<FrameMac *>(docLoader->frame());
    if (frame) {
        WebCoreFrameBridge *bridge = frame->bridge();
        return [bridge expiresTimeForResponse:(NSURLResponse *)response];
    }
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return 0;
}

namespace WebCore {
    
void CachedObject::setResponse(NSURLResponse *response)
{
    KWQRetain(response);
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    KWQRelease(m_response);
    END_BLOCK_OBJC_EXCEPTIONS;

    m_response = response;
}

void CachedObject::setAllData(NSData *allData)
{
    KWQRetain(allData);
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    KWQRelease(m_allData);
    END_BLOCK_OBJC_EXCEPTIONS;

    m_allData = allData;
}

} // namespace
