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

static inline HINTERNET createInternetHandle(const String& userAgent, bool asynchronous)
{
    String userAgentString = userAgent;
    HINTERNET internetHandle = InternetOpenW(userAgentString.charactersWithNullTermination(), INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, asynchronous ? INTERNET_FLAG_ASYNC : 0);

    if (asynchronous)
        InternetSetStatusCallback(internetHandle, &ResourceHandle::internetStatusCallback);

    return internetHandle;
}

static HINTERNET asynchronousInternetHandle(const String& userAgent)
{
    static HINTERNET internetHandle = createInternetHandle(userAgent, true);
    return internetHandle;
}

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
    ~WebCoreSynchronousLoader();

    HINTERNET internetHandle() const { return m_internetHandle; }

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse&);
    virtual void didReceiveData(ResourceHandle*, const char*, int, int lengthReceived);
    virtual void didFinishLoading(ResourceHandle*, double /*finishTime*/);
    virtual void didFail(ResourceHandle*, const ResourceError&);

private:
    ResourceError& m_error;
    ResourceResponse& m_response;
    Vector<char>& m_data;
    HINTERNET m_internetHandle;
};

WebCoreSynchronousLoader::WebCoreSynchronousLoader(ResourceError& error, ResourceResponse& response, Vector<char>& data, const String& userAgent)
    : m_error(error)
    , m_response(response)
    , m_data(data)
    , m_internetHandle(createInternetHandle(userAgent, false))
{
}

WebCoreSynchronousLoader::~WebCoreSynchronousLoader()
{
    InternetCloseHandle(m_internetHandle);
}

void WebCoreSynchronousLoader::didReceiveResponse(ResourceHandle*, const ResourceResponse& response)
{
    m_response = response;
}

void WebCoreSynchronousLoader::didReceiveData(ResourceHandle*, const char* data, int length, int)
{
    m_data.append(data, length);
}

void WebCoreSynchronousLoader::didFinishLoading(ResourceHandle*, double)
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
        if (firstRequest().httpBody()) {
            firstRequest().httpBody()->flatten(d->m_formData);
            d->m_bytesRemainingToWrite = d->m_formData.size();
        }
        INTERNET_BUFFERSA buffers;
        memset(&buffers, 0, sizeof(buffers));
        buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
        buffers.lpcszHeader = headersLatin1.data();
        buffers.dwHeadersLength = headers.length();
        buffers.dwBufferTotal = d->m_bytesRemainingToWrite;
        
        HttpSendRequestExA(d->m_secondaryHandle, &buffers, 0, 0, (DWORD_PTR)d->m_jobId);
        // FIXME: add proper error handling
    }
}

static void callOnRedirect(void* context)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(context);
    handle->onRedirect();
}

static void callOnRequestComplete(void* context)
{
    ResourceHandle* handle = static_cast<ResourceHandle*>(context);
    handle->onRequestComplete();
}

void ResourceHandle::internetStatusCallback(HINTERNET internetHandle, DWORD_PTR context, DWORD internetStatus,
                                                     LPVOID statusInformation, DWORD statusInformationLength)
{
    ResourceHandle* handle = reinterpret_cast<ResourceHandle*>(context);

    switch (internetStatus) {
    case INTERNET_STATUS_REDIRECT:
        handle->d->m_redirectUrl = String(static_cast<UChar*>(statusInformation), statusInformationLength);
        callOnMainThread(callOnRedirect, handle);
        break;

    case INTERNET_STATUS_REQUEST_COMPLETE:
        callOnMainThread(callOnRequestComplete, handle);
        break;

    default:
        break;
    }
}

void ResourceHandle::onRedirect()
{
    ResourceRequest newRequest = firstRequest();
    newRequest.setURL(KURL(ParsedURLString, d->m_redirectUrl));

    ResourceResponse response(firstRequest().url(), String(), 0, String(), String());

    if (ResourceHandleClient* resourceHandleClient = client())
        resourceHandleClient->willSendRequest(this, newRequest, response);
}

bool ResourceHandle::onRequestComplete()
{
    if (d->m_bytesRemainingToWrite) {
        DWORD bytesWritten;
        InternetWriteFile(d->m_requestHandle,
                          d->m_formData.data() + (d->m_formData.size() - d->m_bytesRemainingToWrite),
                          d->m_bytesRemainingToWrite,
                          &bytesWritten);
        d->m_bytesRemainingToWrite -= bytesWritten;
        if (d->m_bytesRemainingToWrite)
            return true;
        d->m_formData.clear();
    }

    if (!d->m_sentEndRequest) {
        HttpEndRequestW(d->m_requestHandle, 0, 0, reinterpret_cast<DWORD_PTR>(this));
        d->m_sentEndRequest = true;
        return true;
    }

    HINTERNET handle = (request().httpMethod() == "POST") ? d->m_secondaryHandle : d->m_resourceHandle;

    static const int bufferSize = 32768;
    char buffer[bufferSize];
    INTERNET_BUFFERSA buffers;
    buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
    buffers.lpvBuffer = buffer;
    buffers.dwBufferLength = bufferSize;

    BOOL ok = FALSE;
    while ((ok = InternetReadFileExA(d->m_requestHandle, &buffers, d->m_loadSynchronously ? 0 : IRF_NO_WAIT, reinterpret_cast<DWORD_PTR>(this))) && buffers.dwBufferLength) {
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

    if (!ok && GetLastError() == ERROR_IO_PENDING)
        return true;

    client()->didFinishLoading(this, 0);
    InternetCloseHandle(d->m_requestHandle);
    InternetCloseHandle(d->m_connectHandle);
    deref(); // balances ref in start
    return false;
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

bool ResourceHandle::start(NetworkingContext* context)
{
    if (request().url().isLocalFile()) {
        ref(); // balanced by deref in fileLoadTimer
        d->m_fileLoadTimer.startOneShot(0.0);
        return true;
    }

    if (!d->m_internetHandle)
        d->m_internetHandle = asynchronousInternetHandle(context->userAgent());

    if (!d->m_internetHandle)
        return false;

    DWORD flags = INTERNET_FLAG_KEEP_CONNECTION
        | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS
        | INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP;

    d->m_connectHandle = InternetConnectW(d->m_internetHandle, firstRequest().url().host().charactersWithNullTermination(), firstRequest().url().port(),
                                          0, 0, INTERNET_SERVICE_HTTP, flags, reinterpret_cast<DWORD_PTR>(this));

    if (!d->m_connectHandle)
        return false;

    String urlStr = firstRequest().url().path();
    String urlQuery = firstRequest().url().query();

    if (!urlQuery.isEmpty()) {
        urlStr.append('?');
        urlStr.append(urlQuery);
    }

    String httpMethod = firstRequest().httpMethod();
    String httpReferrer = firstRequest().httpReferrer();

    LPCWSTR httpAccept[] = { L"*/*", 0 };

    d->m_requestHandle = HttpOpenRequestW(d->m_connectHandle, httpMethod.charactersWithNullTermination(), urlStr.charactersWithNullTermination(),
                                          0, httpReferrer.charactersWithNullTermination(), httpAccept, flags, reinterpret_cast<DWORD_PTR>(this));

    if (!d->m_requestHandle) {
        InternetCloseHandle(d->m_connectHandle);
        return false;
    }

    INTERNET_BUFFERSW internetBuffers;
    ZeroMemory(&internetBuffers, sizeof(internetBuffers));
    internetBuffers.dwStructSize = sizeof(internetBuffers);

    HttpSendRequestExW(d->m_requestHandle, &internetBuffers, 0, 0, reinterpret_cast<DWORD_PTR>(this));

    ref(); // balanced by deref in onRequestComplete

    if (d->m_loadSynchronously)
        while (onRequestComplete()) {
            // Loop until finished.
        }

    return true;
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

    client()->didFinishLoading(this, 0);
}

void ResourceHandle::cancel()
{
    if (d->m_resourceHandle)
        InternetCloseHandle(d->m_resourceHandle);
    else
        d->m_fileLoadTimer.stop();

    client()->didFinishLoading(this, 0); 

    if (!d->m_resourceHandle)
        // Async load canceled before we have a handle -- mark ourselves as in error, to be deleted later.
        // FIXME: need real cancel error
        client()->didFail(this, ResourceError());
}

void ResourceHandle::loadResourceSynchronously(NetworkingContext* context, const ResourceRequest& request, StoredCredentials storedCredentials, ResourceError& error, ResourceResponse& response, Vector<char>& data)
{
    UNUSED_PARAM(storedCredentials);

    WebCoreSynchronousLoader syncLoader(error, response, data, request.httpUserAgent());
    ResourceHandle handle(request, &syncLoader, true, false);

    handle.start(context);
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
