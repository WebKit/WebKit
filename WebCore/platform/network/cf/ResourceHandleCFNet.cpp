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

#include "config.h"

#if USE(CFNETWORK)

#include "DocLoader.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"
#include "ResourceRequestCFNet.h"
#include "ResourceResponse.h"
#include "ResourceResponseCFNet.h"

#include <WTF/HashMap.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <process.h> // for _beginthread()

#include <CFNetwork/CFNetwork.h>
#include <CFNetwork/CFNetworkPriv.h>

//#define LOG_RESOURCELOADER_EVENTS 1

namespace WebCore {

CFURLRequestRef willSendRequest(CFURLConnectionRef conn, CFURLRequestRef cfRequest, CFURLResponseRef cfRedirectResponse, const void* clientInfo)
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;
#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("willSendRequest(conn=%p, job = %p)\n"), conn, handle);
    CFShow(str);
    CFRelease(str);
#endif

    if (ResourceHandleClient* c = handle->client()) {
        ResourceRequest request;
        getResourceRequest(request, cfRequest);
        ResourceResponse redirectResponse;
        getResourceResponse(redirectResponse, cfRedirectResponse);
        return cfURLRequest(request);
    }

    return cfRequest;
}

void didReceiveResponse(CFURLConnectionRef conn, CFURLResponseRef cfResponse, const void* clientInfo) 
{
    ResourceHandle* handle = (ResourceHandle*)clientInfo;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didReceiveResponse(conn=%p, job = %p)\n"), conn, handle);
    CFShow(str);
    CFRelease(str);
#endif

    if (ResourceHandleClient* client = handle->client()) {
      client->receivedResponse(handle, cfResponse);
      ResourceResponse response;
      getResourceResponse(response, cfResponse);
      client->didReceiveResponse(handle, response);
    }
}

void didReceiveData(CFURLConnectionRef conn, CFDataRef data, CFIndex originalLength, const void* clientInfo) 
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;
    const UInt8* bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didReceiveData(conn=%p, job = %p, numBytes = %d)\n"), conn, job, length);
    CFShow(str);
    CFRelease(str);
#endif

    job->client()->didReceiveData(job, (const char*)bytes, length);
}

void didFinishLoading(CFURLConnectionRef conn, const void* clientInfo) 
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didFinishLoading(conn=%p, job = %p)\n"), conn, job);
    CFShow(str);
    CFRelease(str);
#endif

    job->client()->receivedAllData(job, 0);
    job->client()->didFinishLoading(job);
    job->kill();
}

void didFail(CFURLConnectionRef conn, CFStreamError error, const void* clientInfo) 
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didFail(conn=%p, job = %p, error = {%d, %d})\n"), conn, job, error.domain, error.error);
    CFShow(str);
    CFRelease(str);
#endif

    job->setError(1);
    job->client()->receivedAllData(job, 0);
    job->client()->didFinishLoading(job);
    job->kill();
}

CFCachedURLResponseRef willCacheResponse(CFURLConnectionRef conn, CFCachedURLResponseRef cachedResponse, const void* clientInfo) 
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;
    return cachedResponse;
}

void didReceiveChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo)
{
    ResourceHandle* job = (ResourceHandle*)clientInfo;

    // Do nothing right now
}

void addHeadersFromHashMap(CFMutableURLRequestRef request, const HTTPHeaderMap& requestHeaders) 
{
    if (!requestHeaders.size())
        return;

    HTTPHeaderMap::const_iterator end = requestHeaders.end();
    for (HTTPHeaderMap::const_iterator it = requestHeaders.begin(); it != end; ++it) {
        CFStringRef key = it->first.createCFString();
        CFStringRef value = it->second.createCFString();
        CFURLRequestSetHTTPHeaderFieldValue(request, key, value);
        CFRelease(key);
        CFRelease(value);
    }
}

ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_connection) {

#if defined(LOG_RESOURCELOADER_EVENTS)
        CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("Cancelling connection %p\n"), m_connection);
        CFShow(str);
        CFRelease(str);
#endif
        CFURLConnectionCancel(m_connection);
        CFRelease(m_connection);
        m_connection = 0;
    }
}

ResourceHandle::~ResourceHandle()
{
#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("Destroying job %p\n"), this);
    CFShow(str);
    CFRelease(str);
#endif
    delete d;
}

CFArrayRef arrayFromFormData(const FormData& d)
{
    size_t size = d.elements().size();
    CFMutableArrayRef a = CFArrayCreateMutable(0, d.elements().size(), &kCFTypeArrayCallBacks);
    for (size_t i = 0; i < size; ++i) {
        const FormDataElement& e = d.elements()[i];
        if (e.m_type == FormDataElement::data) {
            CFDataRef data = CFDataCreate(0, (const UInt8*)e.m_data.data(), e.m_data.size());
            CFArrayAppendValue(a, data);
            CFRelease(data);
        } else {
            ASSERT(e.m_type == FormDataElement::encodedFile);
            CFStringRef filename = e.m_filename.createCFString();
            CFArrayAppendValue(a, filename);
            CFRelease(filename);
        }
    }
    return a;
}

void emptyPerform(void* unused) 
{
}

static CFRunLoopRef loaderRL = 0;
void runLoaderThread(void *unused)
{
    loaderRL = CFRunLoopGetCurrent();

    // Must add a source to the run loop to prevent CFRunLoopRun() from exiting
    CFRunLoopSourceContext ctxt = {0, (void *)1 /*must be non-NULL*/, 0, 0, 0, 0, 0, 0, 0, emptyPerform};
    CFRunLoopSourceRef bogusSource = CFRunLoopSourceCreate(0, 0, &ctxt);
    CFRunLoopAddSource(loaderRL, bogusSource,kCFRunLoopDefaultMode);

    CFRunLoopRun();
}

bool ResourceHandle::start(DocLoader* docLoader)
{
    d->m_request.setHTTPUserAgent(docLoader->frame()->loader()->userAgent());
    String referrer = docLoader->frame()->loader()->referrer();
    if (!referrer.isEmpty() && referrer.find("file:", 0, false) != 0)
        d->m_request.setHTTPReferrer(referrer);

    CFURLRequestRef request = cfURLRequest(d->m_request);

    ref();
    d->m_loading = true;
    
    CFURLConnectionClient client = {0, this, 0, 0, 0, willSendRequest, didReceiveResponse, didReceiveData, NULL, didFinishLoading, didFail, willCacheResponse, didReceiveChallenge};
    d->m_connection = CFURLConnectionCreate(0, request, &client);
    CFRelease(request);

    if (!loaderRL) {
        _beginthread(runLoaderThread, 0, 0);
        while (loaderRL == 0) {
            // FIXME: sleep 10? that can't be right...
            Sleep(10);
        }
    }

    CFURLConnectionScheduleWithCurrentMessageQueue(d->m_connection);
    CFURLConnectionScheduleDownloadWithRunLoop(d->m_connection, loaderRL, kCFRunLoopDefaultMode);
    CFURLConnectionStart(d->m_connection);

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFURLRef url = d->m_request.url().createCFURL();
    CFStringRef outStr = CFStringCreateWithFormat(0, 0, CFSTR("Starting URL %@ (job = %p, connection = %p)\n"), CFURLGetString(url), this, d->m_connection);
    CFShow(outStr);
    CFRelease(outStr);
    CFRelease(url);
#endif
    return true;
}

void ResourceHandle::cancel()
{
    if (d->m_connection) {
        CFURLConnectionCancel(d->m_connection);
        CFRelease(d->m_connection);
        d->m_connection = 0;
    }

    // Copied directly from ResourceHandleWin.cpp
    setError(1);
    d->m_client->receivedAllData(this, 0);
    d->m_client->didFinishLoading(this);
}

} // namespace WebCore

#endif // USE(CFNETWORK)
