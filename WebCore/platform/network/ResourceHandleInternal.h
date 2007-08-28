// -*- mode: c++; c-basic-offset: 4 -*-
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

#ifndef ResourceHandleInternal_h
#define ResourceHandleInternal_h

#include "ResourceRequest.h"
#include "AuthenticationChallenge.h"

#if USE(CFNETWORK)
#include <CFNetwork/CFURLConnectionPriv.h>
#endif

#if USE(WININET)
#include <winsock2.h>
#include <windows.h>
#include "Timer.h"
#endif

#if USE(CURL)
#include <curl/curl.h>
#endif

#if PLATFORM(QT)
class QWebFrame;
class QWebNetworkJob;
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURLConnection;
#else
class NSURLConnection;
#endif
#endif

// The allocations and releases in ResourceHandleInternal are
// Cocoa-exception-free (either simple Foundation classes or
// WebCoreResourceLoaderImp which avoids doing work in dealloc).

namespace WebCore {
    class ResourceHandleClient;

    class ResourceHandleInternal : Noncopyable {
    public:
        ResourceHandleInternal(ResourceHandle* loader, const ResourceRequest& request, ResourceHandleClient* c, bool defersLoading, bool shouldContentSniff, bool mightDownloadFromHandle)
            : m_client(c)
            , m_request(request)
            , status(0)
            , m_defersLoading(defersLoading)
            , m_shouldContentSniff(shouldContentSniff)
            , m_mightDownloadFromHandle(mightDownloadFromHandle)
#if USE(CFNETWORK)
            , m_connection(0)
#endif
#if USE(WININET)
            , m_fileHandle(INVALID_HANDLE_VALUE)
            , m_fileLoadTimer(loader, &ResourceHandle::fileLoadTimer)
            , m_resourceHandle(0)
            , m_secondaryHandle(0)
            , m_jobId(0)
            , m_threadId(0)
            , m_writing(false)
            , m_formDataString(0)
            , m_formDataLength(0)
            , m_bytesRemainingToWrite(0)
            , m_hasReceivedResponse(false)
            , m_resend(false)
#endif
#if USE(CURL)
            , m_handle(0)
            , m_url(0)
            , m_fileName(0)
            , m_customHeaders(0)
#endif
#if PLATFORM(QT)
            , m_job(0)
            , m_frame(0)
#endif
#if PLATFORM(MAC)
            , m_currentMacChallenge(nil)
#elif USE(CFNETWORK)
            , m_currentCFChallenge(0)
#endif
        {
        }
        
        ~ResourceHandleInternal();

        ResourceHandleClient* client() { return m_client; }
        ResourceHandleClient* m_client;
        
        ResourceRequest m_request;
        
        int status;

        bool m_defersLoading;
        bool m_shouldContentSniff;
        bool m_mightDownloadFromHandle;
#if USE(CFNETWORK)
        RetainPtr<CFURLConnectionRef> m_connection;
#elif PLATFORM(MAC)
        RetainPtr<NSURLConnection> m_connection;
        RetainPtr<WebCoreResourceHandleAsDelegate> m_delegate;
        RetainPtr<id> m_proxy;
#endif
#if USE(WININET)
        HANDLE m_fileHandle;
        Timer<ResourceHandle> m_fileLoadTimer;
        HINTERNET m_resourceHandle;
        HINTERNET m_secondaryHandle;
        unsigned m_jobId;
        DWORD m_threadId;
        bool m_writing;
        char* m_formDataString;
        int m_formDataLength;
        int m_bytesRemainingToWrite;
        String m_postReferrer;
        bool m_hasReceivedResponse;
        bool m_resend;
#endif
#if USE(CURL)
        CURL* m_handle;
        char* m_url;
        char* m_fileName;
        struct curl_slist* m_customHeaders;        
        Vector<char> m_postBytes;
        ResourceResponse m_response;
#endif
#if PLATFORM(QT)
        QWebNetworkJob *m_job;
        QWebFrame *m_frame;
#endif
#if PLATFORM(MAC)
        NSURLAuthenticationChallenge *m_currentMacChallenge;
#endif
#if USE(CFNETWORK)
        CFURLAuthChallengeRef m_currentCFChallenge;
#endif
        AuthenticationChallenge m_currentWebChallenge;
    };

} // namespace WebCore

#endif // ResourceHandleInternal_h
