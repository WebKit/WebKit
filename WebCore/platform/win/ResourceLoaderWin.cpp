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
#include "ResourceLoader.h"
#include "ResourceLoaderInternal.h"
#include "ResourceLoaderWin.h"

#include "CString.h"
#include "DocLoader.h"
#include "Frame.h"
#include "Document.h"
#include "Page.h"
#include <windows.h>
#include <wininet.h>

namespace WebCore {

static unsigned transferJobId = 0;
static HashMap<int, ResourceLoader*>* jobIdMap = 0;

static HWND transferJobWindowHandle = 0;
const LPCWSTR kResourceLoaderWindowClassName = L"ResourceLoaderWindowClass";

// Message types for internal use (keep in sync with kMessageHandlers)
enum {
  handleCreatedMessage = WM_USER,
  requestRedirectedMessage,
  requestCompleteMessage
};

typedef void (ResourceLoader:: *ResourceLoaderEventHandler)(LPARAM);
static const ResourceLoaderEventHandler messageHandlers[] = {
    &ResourceLoader::onHandleCreated,
    &ResourceLoader::onRequestRedirected,
    &ResourceLoader::onRequestComplete
};

static int addToOutstandingJobs(ResourceLoader* job)
{
    if (!jobIdMap)
        jobIdMap = new HashMap<int, ResourceLoader*>;
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

static ResourceLoader* lookupResourceLoader(int jobId)
{
    if (!jobIdMap)
        return 0;
    return jobIdMap->get(jobId);
}

static LRESULT CALLBACK ResourceLoaderWndProc(HWND hWnd, UINT message,
                                              WPARAM wParam, LPARAM lParam)
{
    if (message >= handleCreatedMessage) {
        UINT index = message - handleCreatedMessage;
        if (index < _countof(messageHandlers)) {
            unsigned jobId = (unsigned) wParam;
            ResourceLoader* job = lookupResourceLoader(jobId);
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

static void initializeOffScreenResourceLoaderWindow()
{
    if (transferJobWindowHandle)
        return;

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = ResourceLoaderWndProc;
    wcex.hInstance      = Page::instanceHandle();
    wcex.lpszClassName  = kResourceLoaderWindowClassName;
    RegisterClassEx(&wcex);

    transferJobWindowHandle = CreateWindow(kResourceLoaderWindowClassName, 0, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        HWND_MESSAGE, 0, Page::instanceHandle(), 0);
}

ResourceLoaderInternal::~ResourceLoaderInternal()
{
    if (m_fileHandle != INVALID_HANDLE_VALUE)
        CloseHandle(m_fileHandle);
}

ResourceLoader::~ResourceLoader()
{
    if (d->m_jobId)
        removeFromOutstandingJobs(d->m_jobId);
}

void ResourceLoader::onHandleCreated(LPARAM lParam)
{
    if (!d->m_resourceHandle) {
        d->m_resourceHandle = HINTERNET(lParam);
        if (d->status != 0) {
            // We were canceled before Windows actually created a handle for us, close and delete now.
            InternetCloseHandle(d->m_resourceHandle);
            delete this;
            return;
        }

        if (method() == "POST") {
            // FIXME: Too late to set referrer properly.
            DeprecatedString urlStr = d->URL.path();
            int fragmentIndex = urlStr.find('#');
            if (fragmentIndex != -1)
                urlStr = urlStr.left(fragmentIndex);
            static LPCSTR accept[2]={"*/*", NULL};
            HINTERNET urlHandle = HttpOpenRequestA(d->m_resourceHandle, 
                                                   "POST", urlStr.latin1(), 0, 0, accept,
                                                   INTERNET_FLAG_KEEP_CONNECTION | 
                                                   INTERNET_FLAG_FORMS_SUBMIT |
                                                   INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE,
                                                   (DWORD_PTR)d->m_jobId);
            if (urlHandle == INVALID_HANDLE_VALUE) {
                InternetCloseHandle(d->m_resourceHandle);
                delete this;
            }
        }
    } else if (!d->m_secondaryHandle) {
        assert(method() == "POST");
        d->m_secondaryHandle = HINTERNET(lParam);
        
        // Need to actually send the request now.
        String headers = "Content-Type: application/x-www-form-urlencoded\n";
        headers += "Referer: ";
        headers += d->m_postReferrer;
        headers += "\n";
        String formData = postData().flattenToString();
        INTERNET_BUFFERSA buffers;
        memset(&buffers, 0, sizeof(buffers));
        buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
        buffers.lpcszHeader = headers.latin1();
        buffers.dwHeadersLength = headers.length();
        buffers.dwBufferTotal = formData.length();
        
        d->m_bytesRemainingToWrite = formData.length();
        d->m_formDataString = (char*)malloc(formData.length());
        d->m_formDataLength = formData.length();
        strncpy(d->m_formDataString, formData.latin1(), formData.length());
        d->m_writing = true;
        HttpSendRequestExA(d->m_secondaryHandle, &buffers, 0, 0, (DWORD_PTR)d->m_jobId);
    }
}

void ResourceLoader::onRequestRedirected(LPARAM lParam)
{
    // If already canceled, then ignore this event.
    if (d->status != 0)
        return;

    KURL url(String((StringImpl*) lParam).deprecatedString());
    client()->receivedRedirect(this, url);
}

void ResourceLoader::onRequestComplete(LPARAM lParam)
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

    HINTERNET handle = (method() == "POST") ? d->m_secondaryHandle : d->m_resourceHandle;
    BOOL ok = FALSE;

    static const int bufferSize = 32768;
    char buffer[bufferSize];
    INTERNET_BUFFERSA buffers;
    buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
    buffers.lpvBuffer = buffer;
    buffers.dwBufferLength = bufferSize;

    bool receivedAnyData = false;
    while ((ok = InternetReadFileExA(handle, &buffers, IRF_NO_WAIT, (DWORD_PTR)this)) && buffers.dwBufferLength) {
        if (!hasReceivedResponse()) {
            setHasReceivedResponse();
            client()->receivedResponse(this, 0);
        }
        client()->receivedData(this, buffer, buffers.dwBufferLength);
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
        _RPTF1(_CRT_WARN, "Load error: %i\n", error);
    }
    
    if (d->m_secondaryHandle)
        InternetCloseHandle(d->m_secondaryHandle);
    InternetCloseHandle(d->m_resourceHandle);
    
    client()->receivedAllData(this, &platformData);
    client()->receivedAllData(this);
    delete this;
}

static void __stdcall transferJobStatusCallback(HINTERNET internetHandle,
                                                DWORD_PTR jobId,
                                                DWORD internetStatus,
                                                LPVOID statusInformation,
                                                DWORD statusInformationLength)
{
    UINT msg;
    LPARAM lParam;

    switch (internetStatus) {
    case INTERNET_STATUS_HANDLE_CREATED:
        // tell the main thread about the newly created handle
        msg = handleCreatedMessage;
        lParam = (LPARAM) LPINTERNET_ASYNC_RESULT(statusInformation)->dwResult;
        break;
    case INTERNET_STATUS_REQUEST_COMPLETE:
        // tell the main thread that the request is done
        msg = requestCompleteMessage;
        lParam = 0;
        break;
    case INTERNET_STATUS_REDIRECT:
        // tell the main thread to observe this redirect (FIXME: we probably
        // need to block the redirect at this point so the application can
        // decide whether or not to follow the redirect)
        msg = requestRedirectedMessage;
        lParam = (LPARAM) new StringImpl((const UChar*) statusInformation,
                                         statusInformationLength);
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

bool ResourceLoader::start(DocLoader* docLoader)
{
    if (d->URL.isLocalFile()) {
        DeprecatedString path = d->URL.path();
        // windows does not enjoy a leading slash on paths
        if (path[0] == '/')
            path = path.mid(1);
        d->m_fileHandle = CreateFileA(path.ascii(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        // FIXME: perhaps this error should be reported asynchronously for
        // consistency.
        if (d->m_fileHandle == INVALID_HANDLE_VALUE) {
            delete this;
            return false;
        }

        d->m_fileLoadTimer.startOneShot(0.0);
        return true;
    } else {
        static HINTERNET internetHandle = 0;
        if (!internetHandle) {
            String userAgentStr = docLoader->frame()->userAgent() + String("", 1);
            LPCWSTR userAgent = reinterpret_cast<const WCHAR*>(userAgentStr.characters());
            // leak the Internet for now
            internetHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
        }
        if (!internetHandle) {
            delete this;
            return false;
        }
        static INTERNET_STATUS_CALLBACK callbackHandle = InternetSetStatusCallback(internetHandle, transferJobStatusCallback);

        initializeOffScreenResourceLoaderWindow();
        d->m_jobId = addToOutstandingJobs(this);

        // For form posting, we can't use InternetOpenURL.  We have to use InternetConnect followed by
        // HttpSendRequest.
        HINTERNET urlHandle;
        String referrer = docLoader->frame()->referrer();
        if (method() == "POST") {
            d->m_postReferrer = referrer;
            DeprecatedString host = d->URL.host();
            host += "\0";
            urlHandle = InternetConnectA(internetHandle, host.ascii(), d->URL.port(), 0, 0, 
                                         INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)d->m_jobId);
        } else {
            DeprecatedString urlStr = d->URL.url();
            int fragmentIndex = urlStr.find('#');
            if (fragmentIndex != -1)
                urlStr = urlStr.left(fragmentIndex);
            String headers;
            if (!referrer.isEmpty())
                headers += String("Referer: ") + referrer + "\r\n";

            urlHandle = InternetOpenUrlA(internetHandle, urlStr.ascii(), headers.latin1(), headers.length(),
                                         INTERNET_FLAG_KEEP_CONNECTION, (DWORD_PTR)d->m_jobId);
        }

        if (urlHandle == INVALID_HANDLE_VALUE) {
            delete this;
            return false;
        }
        d->m_threadId = GetCurrentThreadId();

        return true;
    }
}

void ResourceLoader::fileLoadTimer(Timer<ResourceLoader>* timer)
{
    d->client->receivedResponse(this, 0);

    bool result = false;
    DWORD bytesRead = 0;

    do {
        const int bufferSize = 8192;
        char buffer[bufferSize];
        result = ReadFile(d->m_fileHandle, &buffer, bufferSize, &bytesRead, NULL); 
        if (result && bytesRead)
            d->client->receivedData(this, buffer, bytesRead);
        // Check for end of file. 
    } while (result && bytesRead);

    // FIXME: handle errors better

    CloseHandle(d->m_fileHandle);
    d->m_fileHandle = INVALID_HANDLE_VALUE;

    PlatformDataStruct platformData;
    platformData.errorString = 0;
    platformData.error = 0;
    platformData.loaded = TRUE;

    d->client->receivedAllData(this, &platformData);
    d->client->receivedAllData(this);
}

void ResourceLoader::cancel()
{
    if (d->m_resourceHandle)
        InternetCloseHandle(d->m_resourceHandle);
    else
        d->m_fileLoadTimer.stop();

    PlatformDataStruct platformData;
    platformData.errorString = 0;
    platformData.error = 0;
    platformData.loaded = FALSE;

    d->client->receivedAllData(this, &platformData);
    d->client->receivedAllData(this);

    if (!d->m_resourceHandle)
        // Async load canceled before we have a handle -- mark ourselves as in error, to be deleted later.
        setError(1);
}

void ResourceLoader::setHasReceivedResponse(bool b)
{
    d->m_hasReceivedResponse = b;
}

bool ResourceLoader::hasReceivedResponse() const
{
    return d->m_hasReceivedResponse;
}

} // namespace WebCore
