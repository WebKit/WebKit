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

#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "AuthenticationChallenge.h"
#include "Timer.h"

#if USE(CFNETWORK)
#include <CFNetwork/CFURLConnectionPriv.h>
#endif

#if USE(WININET)
#include <winsock2.h>
#include <windows.h>
#endif

#if USE(CURL)
#include <curl/curl.h>
#include "FormDataStreamCurl.h"
#endif

#if USE(SOUP)
#include <libsoup/soup.h>
class Frame;
#endif

#if PLATFORM(QT)
class QWebFrame;
class QWebNetworkJob;
namespace WebCore {
class QNetworkReplyHandler;
}
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSURLAuthenticationChallenge;
@class NSURLConnection;
#else
class NSURLAuthenticationChallenge;
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
            , m_customHeaders(0)
            , m_cancelled(false)
            , m_formDataStream(loader)
#endif
#if USE(SOUP)
            , m_msg(0)
            , m_cancelled(false)
            , m_reportedHeaders(false)
            , m_gfile(0)
            , m_inputStream(0)
            , m_cancellable(0)
            , m_buffer(0)
            , m_bufferSize(0)
            , m_total(0)
            , m_idleHandler(0)
            , m_frame(0)
#endif
#if PLATFORM(QT)
            , m_job(0)
            , m_frame(0)
#endif
#if PLATFORM(MAC)
            , m_startWhenScheduled(false)
            , m_needsSiteSpecificQuirks(false)
            , m_currentMacChallenge(nil)
#elif USE(CFNETWORK)
            , m_currentCFChallenge(0)
#endif
            , m_failureTimer(loader, &ResourceHandle::fireFailure)
        {
            const KURL& url = m_request.url();
            m_user = url.user();
            m_pass = url.pass();
            m_request.removeCredentials();
        }
        
        ~ResourceHandleInternal();

        ResourceHandleClient* client() { return m_client; }
        ResourceHandleClient* m_client;
        
        ResourceRequest m_request;

        // Suggested credentials for the current redirection step.
        String m_user;
        String m_pass;
        
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
        bool m_startWhenScheduled;
        bool m_needsSiteSpecificQuirks;
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
        struct curl_slist* m_customHeaders;
        ResourceResponse m_response;
        bool m_cancelled;

        FormDataStream m_formDataStream;
        Vector<char> m_postBytes;
#endif
#if USE(SOUP)
        SoupMessage* m_msg;
        ResourceResponse m_response;
        bool m_cancelled;
        bool m_reportedHeaders;
        GFile* m_gfile;
        GInputStream* m_inputStream;
        GCancellable* m_cancellable;
        char* m_buffer;
        gsize m_bufferSize, m_total;
        guint m_idleHandler;
        Frame* m_frame;
#endif
#if PLATFORM(QT)
#if QT_VERSION < 0x040400
        QWebNetworkJob* m_job;
#else
        QNetworkReplyHandler* m_job;
#endif
        QWebFrame* m_frame;
#endif

        // FIXME: The platform challenge is almost identical to the one stored in m_currentWebChallenge, but it has a different sender. We only need to store a sender reference here.
#if PLATFORM(MAC)
        NSURLAuthenticationChallenge *m_currentMacChallenge;
#endif
#if USE(CFNETWORK)
        CFURLAuthChallengeRef m_currentCFChallenge;
#endif
        AuthenticationChallenge m_currentWebChallenge;

        ResourceHandle::FailureType m_failureType;
        Timer<ResourceHandle> m_failureTimer;
    };

} // namespace WebCore

#endif // ResourceHandleInternal_h
