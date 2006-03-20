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
#import "TransferJob.h"

#import "TransferJobInternal.h"

#import "FoundationExtras.h"
#import "KURL.h"
#import "BlockExceptions.h"
#import "KWQLoader.h"
#import "Logging.h"
#import "KWQResourceLoader.h"
#import "formdata.h"
#import "FrameMac.h"
#import "WebCoreFrameBridge.h"
#import "DocLoader.h"
#import "KWQFormData.h"
#import "KWQLoader.h"

namespace WebCore {
    
TransferJobInternal::~TransferJobInternal()
{
    KWQRelease(response);
    KWQRelease(loader);
}

TransferJob::~TransferJob()
{
    // This will cancel the handle, and who knows what that could do
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [d->loader jobWillBeDeallocated];
    END_BLOCK_OBJC_EXCEPTIONS;
    delete d;
}

bool TransferJob::start(DocLoader* docLoader)
{
    FrameMac *frame = Mac(docLoader->frame());
    
    if (!frame) {
        delete this;
        return false;
    }
    
    WebCoreFrameBridge* bridge = frame->bridge();

    frame->didTellBridgeAboutLoad(url().url());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    KWQResourceLoader* resourceLoader = [[KWQResourceLoader alloc] initWithJob:this];

    id <WebCoreResourceHandle> handle;

    NSDictionary* headerDict = nil;
    String headerString = queryMetaData("customHTTPHeader");

    if (!headerString.isEmpty())
        headerDict = [NSDictionary _webcore_dictionaryWithHeaderString:headerString];

    if (postData().count() > 0)
        handle = [bridge startLoadingResource:resourceLoader withMethod:method() URL:url().getNSURL() customHeaders:headerDict postData:arrayFromFormData(postData())];
    else
        handle = [bridge startLoadingResource:resourceLoader withMethod:method() URL:url().getNSURL() customHeaders:headerDict];
    [resourceLoader setHandle:handle];
    [resourceLoader release];
    return handle != nil;
    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

void TransferJob::assembleResponseHeaders() const
{
    if (!d->assembledResponseHeaders) {
        if ([d->response isKindOfClass:[NSHTTPURLResponse class]]) {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)d->response;
            NSDictionary *headers = [httpResponse allHeaderFields];
            d->responseHeaders = DeprecatedString::fromNSString(KWQHeaderStringFromDictionary(headers, [httpResponse statusCode]));
        }
        d->assembledResponseHeaders = true;
    }
}

void TransferJob::retrieveCharset() const
{
    if (!d->retrievedCharset) {
        NSString *charset = [d->response textEncodingName];
        if (charset)
            d->metaData.set("charset", charset);
        d->retrievedCharset = true;
    }
}

void TransferJob::setLoader(KWQResourceLoader *loader)
{
    KWQRetain(loader);
    KWQRelease(d->loader);
    d->loader = loader;
}

void TransferJob::receivedResponse(NSURLResponse* response)
{
    d->assembledResponseHeaders = false;
    d->retrievedCharset = false;
    d->response = response;
    KWQRetain(d->response);
    if (d->client)
        d->client->receivedResponse(this, response);
}

void TransferJob::cancel()
{
    [d->loader jobCanceledLoad];
}

} // namespace WebCore
