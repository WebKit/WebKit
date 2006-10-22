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
#import "ResourceLoader.h"
#import "ResourceLoaderInternal.h"

#import "BlockExceptions.h"
#import "DocLoader.h"
#import "FoundationExtras.h"
#import "FrameMac.h"
#import "KURL.h"
#import "FormDataMac.h"
#import "LoaderFunctions.h"
#import "WebCoreResourceLoaderImp.h"
#import "Logging.h"
#import "WebCoreFrameBridge.h"

namespace WebCore {
    
ResourceLoaderInternal::~ResourceLoaderInternal()
{
    HardRelease(response);
    HardRelease(loader);
}

ResourceLoader::~ResourceLoader()
{
    // This will cancel the handle, and who knows what that could do
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [d->loader jobWillBeDeallocated];
    END_BLOCK_OBJC_EXCEPTIONS;
    delete d;
}

bool ResourceLoader::start(DocLoader* docLoader)
{
    ref();
    d->m_loading = true;

    ASSERT(docLoader);
    
    FrameMac* frame = Mac(docLoader->frame());
    if (!frame) {
        kill();
        return false;
    }

    WebCoreFrameBridge* bridge = frame->bridge();

    frame->didTellBridgeAboutLoad(url().url());

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    WebCoreResourceLoaderImp* resourceLoader = [[WebCoreResourceLoaderImp alloc] initWithJob:this];

    id <WebCoreResourceHandle> handle;

    NSDictionary* headerDict = nil;
    
    if (!d->m_requestHeaders.isEmpty())
        headerDict = [NSDictionary _webcore_dictionaryWithHeaderMap:d->m_requestHeaders];

    if (!postData().elements().isEmpty())
        handle = [bridge startLoadingResource:resourceLoader withMethod:method() URL:url().getNSURL() customHeaders:headerDict postData:arrayFromFormData(postData())];
    else
        handle = [bridge startLoadingResource:resourceLoader withMethod:method() URL:url().getNSURL() customHeaders:headerDict];
    [resourceLoader setHandle:handle];
    [resourceLoader release];

    if (handle)
        return true;

    END_BLOCK_OBJC_EXCEPTIONS;

    kill();
    return false;
}

void ResourceLoader::assembleResponseHeaders() const
{
    if (!d->assembledResponseHeaders) {
        if ([d->response isKindOfClass:[NSHTTPURLResponse class]]) {
            NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)d->response;
            NSDictionary *headers = [httpResponse allHeaderFields];
            d->responseHeaders = DeprecatedString::fromNSString(HeaderStringFromDictionary(headers, [httpResponse statusCode]));
        }
        d->assembledResponseHeaders = true;
    }
}

void ResourceLoader::retrieveResponseEncoding() const
{
    if (!d->m_retrievedResponseEncoding) {
        NSString *textEncodingName = [d->response textEncodingName];
        if (textEncodingName)
            d->m_responseEncoding = textEncodingName;
        d->m_retrievedResponseEncoding = true;
    }
}

void ResourceLoader::setLoader(WebCoreResourceLoaderImp *loader)
{
    HardRetain(loader);
    HardRelease(d->loader);
    d->loader = loader;
}

void ResourceLoader::receivedResponse(NSURLResponse* response)
{
    d->assembledResponseHeaders = false;
    d->m_retrievedResponseEncoding = false;
    d->response = response;
    HardRetain(d->response);
    if (d->client)
        d->client->receivedResponse(this, response);
}

void ResourceLoader::cancel()
{
    [d->loader jobCanceledLoad];
}

} // namespace WebCore
