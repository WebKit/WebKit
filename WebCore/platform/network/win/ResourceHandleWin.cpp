/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
#include "ResourceHandle.h"

#include "CachedResourceLoader.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "ResourceHandleWin.h"
#include "Timer.h"
#include "WebCoreInstanceHandle.h"
#include <wtf/text/CString.h>
#include <windows.h>
#include <wininet.h>

namespace WebCore {

static unsigned transferJobId = 0;
static HashMap<int, ResourceHandle*>* jobIdMap = 0;

static HWND transferJobWindowHandle = 0;
const LPCWSTR kResourceHandleWindowClassName = L"ResourceHandleWindowClass";

// Message types for internal use (keep in sync with kMessageHandlers)
enum {
  handleCreatedMessage = WM_USER,
  requestRedirectedMessage,
  requestCompleteMessage
};

typedef void (ResourceHandle:: *ResourceHandleEventHandler)(LPARAM);
static const ResourceHandleEventHandler messageHandlers[] = {
    &ResourceHandle::onHandleCreated,
    &ResourceHandle::onRequestRedirected,
    &ResourceHandle::onRequestComplete
};

static String queryHTTPHeader(HINTERNET requestHandle, DWORD infoLevel)
{
    DWORD bufferSize = 0;
    HttpQueryInfoW(requestHandle, infoLevel, 0, &bufferSize, 0);

    Vector<UChar> characters(bufferSize / sizeof(UChar));

    if (!HttpQueryInfoW(requestHandle, infoLevel, characters.data(), &bufferSize, 0))
        return String();

    characters.removeLast(); // Remove NullTermination.
    return String::adopt(characters);
}

static int addToOutstandingJobs(ResourceHandle* job)
{
    if (!jobIdMap)
        jobIdMap = new HashMap<int, ResourceHandle*>;
    transferJobId++;
    jobIdMap->set(transferJobId, job);
    return transferJobId;
}

static void removeFromOutstandingJobs(int jobId)
{
    if (!jobIdMap)
        return;
    jobIdMap->remove(jobId);
}

static ResourceHandle* lookupResourceHandle(int jobId)
{
    if (!jobIdMap)
        return 0;
    return jobIdMap->get(jobId);
}

static LRESULT CALLBACK ResourceHandleWndProc(HWND hWnd, UINT message,
                                              WPARAM wParam, LPARAM lParam)
{
    if (message >= handleCreatedMessage) {
        UINT index = message - handleCreatedMessage;
        if (index < _countof(messageHandlers)) {
            unsigned jobId = (unsigned) wParam;
            ResourceHandle* job = lookupResourceHandle(jobId);
            if (job) {
                ASSERT(job->d->m_jobId == jobId);
                ASSERT(job->d->m_threadId == GetCurrentThreadId());
                (job->*(messageHandlers[index]))(lParam);
            }
            return 0;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

static void initializeOffScreenResourceHandleWindow()
{
    if (transferJobWindowHandle)
        return;

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = ResourceHandleWndProc;
    wcex.hInstance      = WebCore::instanceHandle();
    wcex.lpszClassName  = kResourceHandleWindowClassName;
    RegisterClassEx(&wcex);

    transferJobWindowHandle = CreateWindow(kResourceHandleWindowClassName, 0, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        HWND_MESSAGE, 0, WebCore::instanceHandle(), 0);
}


class WebCoreSynchronousLoader : public ResourceHandleClient, public Noncopyable {
public:
    WebCoreSynchronousLoader(ResourceError&, ResourceResponse&, Vector<char>&, const String& userAgent);

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*);
    virtual void didFail(ResourceHandle*, const ResourceError&);

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<char>& m_data;
};

WebCoreSynchronousLoader::WebCoreSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<char>& data, const String& userAgent)
    : m_error(error)
    , m_response(response)
    , m_data(data)
{
}

void WebCoreSynchronousLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    m_response = response;
}

void WebCoreSynchronousLoader::didReceiveData(ResourceHandle*, const char* data, int length, int)
{
    m_data.append(data, length);
}

void WebCoreSynchronousLoader::didFinishLoading(ResourceHandle*)
{
}

void WebCoreSynchronousLoader::didFail(ResourceHandle*, const ResourceError& error)
{
    m_error = error;
}


ResourceHandleInternal::~ResourceHandleInternal()
{
    if (m_fileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(m_fileHandle);
}

ResourceHandle::~ResourceHandle()
{
    if (d->m_jobId)
        removeFromOutstandingJobs(d->m_jobId);
}

void ResourceHandle::onHandleCreated(LPARAM lParam)
{
    if (!d->m_resourceHandle) {
        d->m_resourceHandle = HINTERNET(lParam);
        if (d->status != 0) {
            // We were canceled before Windows actually created a handle for us, close and delete now.
            InternetCloseHandle(d->m_resourceHandle);
            delete this;
            return;
        }

        if (request().httpMethod() == "POST") {
            // FIXME: Too late to set referrer properly.
            String urlStr = request().url().path();
            int fragmentIndex = urlStr.find('#');
            if (fragmentIndex != -1)
                urlStr = urlStr.left(fragmentIndex);
            static LPCSTR accept[2]={"*/*", NULL};
            HINTERNET urlHandle = HttpOpenRequestA(d->m_resourceHandle, 
                                                   "POST", urlStr.latin1().data(), 0, 0, accept,
                                                   INTERNET_FLAG_KEEP_CONNECTION | 
                                                   INTERNET_FLAG_FORMS_SUBMIT |
                                                   INTERNET_FLAG_RELOAD |
                                                   INTERNET_FLAG_NO_CACHE_WRITE |
                                                   INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
                                                   INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP,
                                                   (DWORD_PTR)d->m_jobId);
            if (urlHandle == INVALID_HANDLE_VALUE) {
                InternetCloseHandle(d->m_resourceHandle);
                delete this;
            }
        }
    } else if (!d->m_secondaryHandle) {
        assert(request().httpMethod() == "POST");
        d->m_secondaryHandle = HINTERNET(lParam);
        
        // Need to actually send the request now.
        String headers = "Content-Type: application/x-www-form-urlencoded\n";
        headers += "Referer: ";
        headers += d->m_postReferrer;
        headers += "\n";
        const CString& headersLatin1 = headers.latin1();
        String formData = request().httpBody()->flattenToString();
        INTERNET_BUFFERSA buffers;
        memset(&buffers, 0, sizeof(buffers));
        buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
        buffers.lpcszHeader = headersLatin1.data();
        buffers.dwHeadersLength = headers.length();
        buffers.dwBufferTotal = formData.length();
        
        d->m_bytesRemainingToWrite = formData.length();
        d->m_formDataString = (char*)malloc(formData.length());
        d->m_formDataLength = formData.length();
        strncpy(d->m_formDataString, formData.latin1().data(), formData.length());
        d->m_writing = true;
        HttpSendRequestExA(d->m_secondaryHandle, &buffers, 0, 0, (DWORD_PTR)d->m_jobId);
        // FIXME: add proper error handling
    }
}

void ResourceHandle::onRequestRedirected(LPARAM lParam)
{
    // If already canceled, then ignore this event.
    if (d->status != 0)
        return;

    ResourceRequest request((StringImpl*) lParam);
    ResourceResponse redirectResponse;
    client()->willSendRequest(this, request, redirectResponse);
}

void ResourceHandle::onRequestComplete(LPARAM lParam)
{
    if (d->m_writing) {
        DWORD bytesWritten;
        InternetWriteFile(d->m_secondaryHandle,
                          d->m_formDataString + (d->m_formDataLength - d->m_bytesRemainingToWrite),
                          d->m_bytesRemainingToWrite,
                          &bytesWritten);
        d->m_bytesRemainingToWrite -= bytesWritten;
        if (!d->m_bytesRemainingToWrite) {
            // End the request.
            d->m_writing = false;
            HttpEndRequest(d->m_secondaryHandle, 0, 0, (DWORD_PTR)d->m_jobId);
            free(d->m_formDataString);
            d->m_formDataString = 0;
        }
        return;
    }

    HINTERNET handle = (request().httpMethod() == "POST") ? d->m_secondaryHandle : d->m_resourceHandle;

    static const int bufferSize = 32768;
    char buffer[bufferSize];
    INTERNET_BUFFERSA buffers;
    buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
    buffers.lpvBuffer = buffer;
    buffers.dwBufferLength = bufferSize;

    BOOL ok = FALSE;
    while ((ok = InternetReadFileExA(handle, &buffers, IRF_NO_WAIT, (DWORD_PTR)this)) && buffers.dwBufferLength) {
        if (!hasReceivedResponse()) {
            setHasReceivedResponse();
            ResourceResponse response;
            response.setURL(firstRequest().url());

            String httpStatusText = queryHTTPHeader(d->m_requestHandle, HTTP_QUERY_STATUS_TEXT);
            if (!httpStatusText.isNull())
                response.setHTTPStatusText(httpStatusText);

            String httpStatusCode = queryHTTPHeader(d->m_requestHandle, HTTP_QUERY_STATUS_CODE);
            if (!httpStatusCode.isNull())
                response.setHTTPStatusCode(httpStatusCode.toInt());

            String httpContentLength = queryHTTPHeader(d->m_requestHandle, HTTP_QUERY_CONTENT_LENGTH);
            if (!httpContentLength.isNull())
                response.setExpectedContentLength(httpContentLength.toInt());

            String httpContentType = queryHTTPHeader(d->m_requestHandle, HTTP_QUERY_CONTENT_TYPE);
            if (!httpContentType.isNull()) {
                response.setMimeType(extractMIMETypeFromMediaType(httpContentType));
                response.setTextEncodingName(extractCharsetFromMediaType(httpContentType));
            }

            client()->didReceiveResponse(this, response);
        }
        client()->didReceiveData(this, buffer, buffers.dwBufferLength, 0);
        buffers.dwBufferLength = bufferSize;
    }

    PlatformDataStruct platformData;
    platformData.errorString = 0;
    platformData.error = 0;
    platformData.loaded = ok;

    if (!ok) {
        int error = GetLastError();
        if (error == ERROR_IO_PENDING)
            return;
        DWORD errorStringChars = 0;
        if (!InternetGetLastResponseInfo(&platformData.error, 0, &errorStringChars)) {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                platformData.errorString = new TCHAR[errorStringChars];
                InternetGetLastResponseInfo(&platformData.error, platformData.errorString, &errorStringChars);
            }
        }
#ifdef RESOURCE_LOADER_DEBUG
        char buf[64];
        _snprintf(buf, sizeof(buf), "Load error: %i\n", error);
        OutputDebugStringA(buf);
#endif 
    }
    
    if (d->m_secondaryHandle)
        InternetCloseHandle(d->m_secondaryHandle);
    InternetCloseHandle(d->m_resourceHandle);

    client()->didFinishLoading(this);
    delete this;
}

static void __stdcall transferJobStatusCallback(HINTERNET internetHandle,
                                                DWORD_PTR jobId,
                                                DWORD internetStatus,
                                                LPVOID statusInformation,
                                                DWORD statusInformationLength)
{
#ifdef RESOURCE_LOADER_DEBUG
    char buf[64];
    _snprintf(buf, sizeof(buf), "status-callback: status=%u, job=%p\n",
              internetStatus, jobId);
    OutputDebugStringA(buf);
#endif

    UINT msg;
    LPARAM lParam;

    switch (internetStatus) {
    case INTERNET_STATUS_HANDLE_CREATED:
        // tell the main thread about the newly created handle
        msg = handleCreatedMessage;
        lParam = (LPARAM) LPINTERNET_ASYNC_RESULT(statusInformation)->dwResult;
        break;
    case INTERNET_STATUS_REQUEST_COMPLETE:
#ifdef RESOURCE_LOADER_DEBUG
        _snprintf(buf, sizeof(buf), "request-complete: result=%p, error=%u\n",
            LPINTERNET_ASYNC_RESULT(statusInformation)->dwResult,
            LPINTERNET_ASYNC_RESULT(statusInformation)->dwError);
        OutputDebugStringA(buf);
#endif
        // tell the main thread that the request is done
        msg = requestCompleteMessage;
        lParam = 0;
        break;
    case INTERNET_STATUS_REDIRECT:
        // tell the main thread to observe this redirect (FIXME: we probably
        // need to block the redirect at this point so the application can
        // decide whether or not to follow the redirect)
        msg = requestRedirectedMessage;
        lParam = (LPARAM) StringImpl::create((const UChar*) statusInformation,
                                             statusInformationLength).releaseRef();
        break;
    case INTERNET_STATUS_USER_INPUT_REQUIRED:
        // FIXME: prompt the user if necessary
        ResumeSuspendedDownload(internetHandle, 0);
    case INTERNET_STATUS_STATE_CHANGE:
        // may need to call ResumeSuspendedDownload here as well
    default:
        return;
    }

    PostMessage(transferJobWindowHandle, msg, (WPARAM) jobId, lParam);
}

bool ResourceHandle::start(Frame* frame)
{
    ref();
    if (request().url().isLocalFile()) {
        d->m_fileLoadTimer.startOneShot(0.0);
        return true;
    } else {
        static HINTERNET internetHandle = 0;
        if (!internetHandle) {
            String userAgentStr = frame->loader()->userAgent(request().url()) + String("", 1);
            LPCWSTR userAgent = reinterpret_cast<const WCHAR*>(userAgentStr.characters());
            // leak the Internet for now
            internetHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
        }
        if (!internetHandle) {
            delete this;
            return false;
        }
        static INTERNET_STATUS_CALLBACK callbackHandle = 
            InternetSetStatusCallback(internetHandle, transferJobStatusCallback);

        initializeOffScreenResourceHandleWindow();
        d->m_jobId = addToOutstandingJobs(this);

        DWORD flags =
            INTERNET_FLAG_KEEP_CONNECTION |
            INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
            INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP;

        // For form posting, we can't use InternetOpenURL.  We have to use
        // InternetConnect followed by HttpSendRequest.
        HINTERNET urlHandle;
        String referrer = frame->loader()->referrer();
        if (request().httpMethod() == "POST") {
            d->m_postReferrer = referrer;
            String host = request().url().host();
            urlHandle = InternetConnectA(internetHandle, host.latin1().data(),
                                         request().url().port(),
                                         NULL, // no username
                                         NULL, // no password
                                         INTERNET_SERVICE_HTTP,
                                         flags, (DWORD_PTR)d->m_jobId);
        } else {
            String urlStr = request().url().string();
            int fragmentIndex = urlStr.find('#');
            if (fragmentIndex != -1)
                urlStr = urlStr.left(fragmentIndex);
            String headers;
            if (!referrer.isEmpty())
                headers += String("Referer: ") + referrer + "\r\n";

            urlHandle = InternetOpenUrlA(internetHandle, urlStr.latin1().data(),
                                         headers.latin1().data(), headers.length(),
                                         flags, (DWORD_PTR)d->m_jobId);
        }

        if (urlHandle == INVALID_HANDLE_VALUE) {
            delete this;
            return false;
        }
        d->m_threadId = GetCurrentThreadId();

        return true;
    }
}

void ResourceHandle::fileLoadTimer(Timer<ResourceHandle>*)
{
    RefPtr<ResourceHandle> protector(this);
    deref(); // balances ref in start

    String fileName = firstRequest().url().fileSystemPath();
    HANDLE fileHandle = CreateFileW(fileName.charactersWithNullTermination(), GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        client()->didFail(this, ResourceError());
        return;
    }

    ResourceResponse response;

    int dotPos = fileName.reverseFind('.');
    int slashPos = fileName.reverseFind('/');

    if (slashPos < dotPos && dotPos != -1) {
        String ext = fileName.substring(dotPos + 1);
        response.setMimeType(MIMETypeRegistry::getMIMETypeForExtension(ext));
    }

    client()->didReceiveResponse(this, response);

    bool result = false;
    DWORD bytesRead = 0;

    do {
        const int bufferSize = 8192;
        char buffer[bufferSize];
        result = ReadFile(fileHandle, &buffer, bufferSize, &bytesRead, 0);
        if (result && bytesRead)
            client()->didReceiveData(this, buffer, bytesRead, 0);
        // Check for end of file.
    } while (result && bytesRead);

    CloseHandle(fileHandle);

    client()->didFinishLoading(this);
}

void ResourceHandle::cancel()
{
    if (d->m_resourceHandle)
        InternetCloseHandle(d->m_resourceHandle);
    else
        d->m_fileLoadTimer.stop();

    client()->didFinishLoading(this); 

    if (!d->m_resourceHandle)
        // Async load canceled before we have a handle -- mark ourselves as in error, to be deleted later.
        // FIXME: need real cancel error
        client()->didFail(this, ResourceError());
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data, Frame* frame)
{
    UNUSED_PARAM(storedCredentials);

    WebCoreSynchronousLoader syncLoader(error, response, data, request.httpUserAgent());
    ResourceHandle handle(request, &syncLoader, true, false);

    handle.start(frame);
}

void ResourceHandle::setHasReceivedResponse(bool b)
{
    d->m_hasReceivedResponse = b;
}

bool ResourceHandle::hasReceivedResponse() const
{
    return d->m_hasReceivedResponse;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest&, Frame*)
{
    notImplemented();
    return false;
}

void prefetchDNS(const String&)
{
    notImplemented();
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    ASSERT_NOT_REACHED();
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
}

bool ResourceHandle::loadsBlocked()
{
    return false;
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    notImplemented();
}

} // namespace WebCore
