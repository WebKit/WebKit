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

#include "ResourceLoader.h"
#include "ResourceLoaderInternal.h"
#include "DocLoader.h"
#include "Frame.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <process.h> // for _beginthread()

#include <CFNetwork/CFNetwork.h>
#include <CFNetwork/CFNetworkPriv.h>

//#define LOG_RESOURCELOADER_EVENTS 1

namespace WebCore {

CFURLRequestRef willSendRequest(CFURLConnectionRef conn, CFURLRequestRef request, CFURLResponseRef redirectionResponse, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;
    CFURLRef url = CFURLRequestGetURL(request);
    CFStringRef urlString = CFURLGetString(url);
    const char *bytes = CFStringGetCStringPtr(urlString, kCFStringEncodingUTF8);
    bool freeBytes = false;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("willSendRequest(conn=%p, job = %p)\n"), conn, job);
    CFShow(str);
    CFRelease(str);
#endif

    if (!bytes) {
        CFIndex numBytes, urlLength = CFStringGetLength(urlString);
        UInt8* newBytes;
        CFStringGetBytes(urlString, CFRangeMake(0, urlLength), kCFStringEncodingUTF8, 0, FALSE, 0, 0, &numBytes);
        newBytes = (UInt8*)malloc(numBytes + 1);
        CFStringGetBytes(urlString, CFRangeMake(0, urlLength), kCFStringEncodingUTF8, 0, FALSE, newBytes, numBytes, &numBytes);
        newBytes[numBytes] = 0;
        freeBytes = true;
        bytes = (char*)newBytes;
    }
    ASSERT(bytes);
    KURL newURL(bytes);
    if (!(newURL == job->url()))
        job->client()->receivedRedirect(job, newURL);
    if (freeBytes) 
        free((void*)bytes);
    return request;
}

void didReceiveChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;

    // Do nothing right now
}

void didCancelChallenge(CFURLConnectionRef conn, CFURLAuthChallengeRef challenge, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;
    
    // Do nothing right now
}

void didReceiveResponse(CFURLConnectionRef conn, CFURLResponseRef response, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didReceiveResponse(conn=%p, job = %p)\n"), conn, job);
    CFShow(str);
    CFRelease(str);
#endif

    job->client()->receivedResponse(job, response);
}

void didReceiveData(CFURLConnectionRef conn, CFDataRef data, CFIndex originalLength, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;
    const UInt8* bytes = CFDataGetBytePtr(data);
    CFIndex length = CFDataGetLength(data);

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didReceiveData(conn=%p, job = %p, numBytes = %d)\n"), conn, job, length);
    CFShow(str);
    CFRelease(str);
#endif

    job->client()->receivedData(job, (const char*)bytes, length);
}

void didFinishLoading(CFURLConnectionRef conn, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didFinishLoading(conn=%p, job = %p)\n"), conn, job);
    CFShow(str);
    CFRelease(str);
#endif

    job->client()->receivedAllData(job, 0);
    job->client()->receivedAllData(job);
}

void didFail(CFURLConnectionRef conn, CFStreamError error, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef str = CFStringCreateWithFormat(0, 0, CFSTR("didFail(conn=%p, job = %p, error = {%d, %d})\n"), conn, job, error.domain, error.error);
    CFShow(str);
    CFRelease(str);
#endif

    job->setError(1);
    job->client()->receivedAllData(job, 0);
    job->client()->receivedAllData(job);
}

CFURLCachedResponseRef willCacheResponse(CFURLConnectionRef conn, CFURLCachedResponseRef cachedResponse, const void* clientInfo) 
{
    ResourceLoader* job = (ResourceLoader*)clientInfo;
    return cachedResponse;
}

const unsigned BUF_LENGTH = 500;
const char* dummyBytes = "GET / HTTP/1.1\r\n";
void addHeadersFromString(CFHTTPMessageRef request, CFStringRef headerString) 
{
    CFIndex headerLength = CFStringGetLength(headerString);
    if (headerLength == 0) 
        return;

    CFHTTPMessageRef dummy = CFHTTPMessageCreateEmpty(0, TRUE);
    CFHTTPMessageAppendBytes(dummy, (const UInt8*)dummyBytes, strlen(dummyBytes));
    
    UInt8 buffer[BUF_LENGTH];
    UInt8* bytes = buffer;
    CFIndex numBytes;
    if (headerLength != CFStringGetBytes(headerString, CFRangeMake(0, headerLength), kCFStringEncodingUTF8, 0, FALSE, bytes, BUF_LENGTH, &numBytes)) {
        CFStringGetBytes(headerString, CFRangeMake(0, headerLength), kCFStringEncodingUTF8, 0, FALSE, 0, 0, &numBytes);
        bytes = (UInt8 *)malloc(numBytes);
        CFStringGetBytes(headerString, CFRangeMake(0, headerLength), kCFStringEncodingUTF8, 0, FALSE, bytes, numBytes, &numBytes);
    }
    
    CFHTTPMessageAppendBytes(dummy, bytes, numBytes);
    if (bytes != buffer) 
        free(bytes);
    
    CFDictionaryRef allHeaders = CFHTTPMessageCopyAllHeaderFields(dummy);
    CFRelease(dummy);
    
    _CFHTTPMessageSetMultipleHeaderFields(request, allHeaders);
    CFRelease(allHeaders);
}

ResourceLoaderInternal::~ResourceLoaderInternal()
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

ResourceLoader::~ResourceLoader()
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
    CFMutableArrayRef a = CFArrayCreateMutable(0, d.count(), &kCFTypeArrayCallBacks);
    for (DeprecatedValueListConstIterator<FormDataElement> it = d.begin(); it != d.end(); ++it) {
        const FormDataElement& e = *it;
        if (e.m_type == FormDataElement::data) {
            CFDataRef data = CFDataCreate(0, (const UInt8*)e.m_data.data(), e.m_data.size());
            CFArrayAppendValue(a, data);
            CFRelease(data);
        } else {
            ASSERT(e.m_type == FormDataElement::encodedFile);
            int len;
            const char* bytes = e.m_filename.ascii(); // !!! Not a good assumption.  We need to yank the DeprecatedString to CFString conversions from the Mac-only code
            CFStringRef filename = CFStringCreateWithBytes(0, (const UInt8*)bytes, e.m_filename.length(), kCFStringEncodingUTF8, false);
            CFArrayAppendValue(a, filename);
            CFRelease(filename);
        }
    }
    return a;
}

void emptyPerform(void* unused) 
{
}

#if defined(LOADER_THREAD)
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
#endif

bool ResourceLoader::start(DocLoader* docLoader)
{
    CFURLRef url = d->URL.createCFURL();
    String str = d->method;
    CFStringRef requestMethod = CFStringCreateWithCharacters(0, (const UniChar *)str.characters(), str.length());
    Boolean isPost = CFStringCompare(requestMethod, CFSTR("POST"), kCFCompareCaseInsensitive);
    CFHTTPMessageRef httpRequest = CFHTTPMessageCreateRequest(0, requestMethod, url, kCFHTTPVersion1_1);
    CFRelease(requestMethod);
    
    str = queryMetaData("customHTTPHeader");
    CFStringRef headerString = CFStringCreateWithCharacters(0, (const UniChar *)str.characters(), str.length());
    if (headerString) {
        addHeadersFromString(httpRequest, headerString);
        CFRelease(headerString);
    }

    DeprecatedString referrer = docLoader->frame()->referrer();
    if (!referrer.isEmpty() && referrer.isAllASCII()) {
        int len;
        const char* bytes = referrer.ascii();
        CFStringRef str = CFStringCreateWithBytes(0, (const UInt8*)bytes, referrer.length(), kCFStringEncodingUTF8, false);
        CFHTTPMessageSetHeaderFieldValue(httpRequest, CFSTR("Referer"),str);
        CFRelease(str);
    }
    
    CFReadStreamRef bodyStream = 0;
    if (postData().count() > 0) {
        CFArrayRef formArray = arrayFromFormData(postData());
        bool done = false;
        CFIndex count = CFArrayGetCount(formArray);

        if (count == 1) {
            // Handle the common special case of one piece of form data, with no files.
            CFTypeRef d = CFArrayGetValueAtIndex(formArray, 0);
            if (CFGetTypeID(d) == CFDataGetTypeID()) {
                CFHTTPMessageSetBody(httpRequest, (CFDataRef)d);
                done = true;
            }
        }

        if (!done) {
            // Precompute the content length so NSURLConnection doesn't use chunked mode.
            long long length = 0;
            unsigned i;
            bool success = true;
            for (i = 0; success && i < count; ++i) {
                CFTypeRef data = CFArrayGetValueAtIndex(formArray, i);
                CFIndex typeID = CFGetTypeID(data);
                if (typeID == CFDataGetTypeID()) {
                    CFDataRef d = (CFDataRef)data;
                    length += CFDataGetLength(d);
                } else {
                    // data is a CFStringRef
                    CFStringRef s = (CFStringRef)data;
                    CFIndex bufLen = CFStringGetMaximumSizeOfFileSystemRepresentation(s);
                    char* buf = (char*)malloc(bufLen);
                    if (CFStringGetFileSystemRepresentation(s, buf, bufLen)) {
                        struct _stat64i32 sb;
                        int statResult = _stat(buf, &sb);
                        if (statResult == 0 && (sb.st_mode & S_IFMT) == S_IFREG)
                            length += sb.st_size;
                        else
                            success = false;
                    } else {
                        success = false;
                    }
                    free(buf);
                }
            }
            if (success) {
                CFStringRef lengthStr = CFStringCreateWithFormat(0, 0, CFSTR("%lld"), length);
                CFHTTPMessageSetHeaderFieldValue(httpRequest, CFSTR("Content-Length"), lengthStr);
                CFRelease(lengthStr);
            }
            bodyStream = CFReadStreamCreateWithFormArray(0, formArray);
        }
        CFRelease(formArray);
    }

    CFURLRequestRef request = CFURLRequestCreateHTTPRequest(0, httpRequest, bodyStream, kCFURLRequestCachePolicyProtocolDefault, 30.0, 0);
    CFURLConnectionClient client = {0, this, 0, 0, 0, willSendRequest, didReceiveChallenge, didCancelChallenge, didReceiveResponse, didReceiveData, didFinishLoading, didFail};
    d->m_connection = CFURLConnectionCreate(0, request, &client);
    CFRelease(request);
    CFURLConnectionSetMaximumBufferSize(d->m_connection, 32*1024); // Buffer up to 32K at a time

#if defined(LOADER_THREAD)
    if (!loaderRL) {
        _beginthread(runLoaderThread, 0, 0);
        while (loaderRL == 0) {
            Sleep(10);
        }
    }
#endif

    CFURLConnectionScheduleWithRunLoop(d->m_connection, CFRunLoopGetCurrent(), kCFRunLoopCommonModes);
#if defined(LOADER_THREAD)
    CFURLConnectionScheduleDownloadWithRunLoop(d->m_connection, loaderRL, kCFRunLoopDefaultMode);
#endif
    CFURLConnectionStart(d->m_connection);

#if defined(LOG_RESOURCELOADER_EVENTS)
    CFStringRef outStr = CFStringCreateWithFormat(0, 0, CFSTR("Starting URL %@ (job = %p, connection = %p)\n"), CFURLGetString(url), this, d->m_connection);
    CFShow(outStr);
    CFRelease(outStr);
#endif
    CFRelease(url);
    return true;
}

void ResourceLoader::cancel()
{
    if (d->m_connection) {
        CFURLConnectionCancel(d->m_connection);
        CFRelease(d->m_connection);
        d->m_connection = 0;
    }
}

} // namespace WebCore

#endif // USE(CFNETWORK)
