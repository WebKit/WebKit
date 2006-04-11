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
#include "TransferJob.h"
#include "TransferJobInternal.h"

#include "DocLoader.h"
#include "Frame.h"
#include <windows.h>
#include <wininet.h>

namespace WebCore {

static unsigned transferJobId = 0;
static HashMap<int, TransferJob*>* jobIdMap = 0;

static HWND transferJobWindowHandle = 0;
static UINT loadStatusMessage = 0;
const LPCWSTR kTransferJobWindowClassName = L"TransferJobWindowClass";

static int addToOutstandingJobs(TransferJob* job)
{
    if (!jobIdMap)
        jobIdMap = new HashMap<int, TransferJob*>;
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

static TransferJob* lookupTransferJob(int jobId)
{
    if (!jobIdMap)
        return 0;
    return jobIdMap->get(jobId);
}

struct JobLoadStatus {
    DWORD internetStatus;
    DWORD_PTR dwResult;
};

LRESULT CALLBACK TransferJobWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == loadStatusMessage) {
        JobLoadStatus* jobLoadStatus = (JobLoadStatus*)lParam;
        DWORD internetStatus = jobLoadStatus->internetStatus;
        DWORD_PTR dwResult = jobLoadStatus->dwResult;
        delete jobLoadStatus;
        jobLoadStatus = 0;

        // If we get a message about a job we no longer know about (already deleted), ignore it.
        unsigned jobId = (unsigned)wParam;
        TransferJob* job = lookupTransferJob(jobId);
        if (!job)
            return 0;

        ASSERT(job->d->m_jobId == jobId);
        ASSERT(job->d->m_threadId == GetCurrentThreadId());

        if (internetStatus == INTERNET_STATUS_HANDLE_CREATED) {
            job->d->m_resourceHandle = HINTERNET(dwResult);
            if (job->d->status != 0) {
                // We were canceled before Windows actually created a handle for us, close and delete now.
                InternetCloseHandle(job->d->m_resourceHandle);
                delete job;
            }
        } else if (internetStatus == INTERNET_STATUS_REQUEST_COMPLETE) {
            bool ok = false;

            static const int bufferSize = 32768;
            char buffer[bufferSize];
            INTERNET_BUFFERSA buffers;
            buffers.dwStructSize = sizeof(INTERNET_BUFFERSA);
            buffers.lpvBuffer = buffer;
            buffers.dwBufferLength = bufferSize;

            while ((ok = InternetReadFileExA(job->d->m_resourceHandle, &buffers, IRF_NO_WAIT, (DWORD_PTR)job)) && buffers.dwBufferLength) {
                job->client()->receivedData(job, buffer, buffers.dwBufferLength);
                buffers.dwBufferLength = bufferSize;
            }

            if (!ok) {
                int error = GetLastError();
                if (error == ERROR_IO_PENDING)
                    return 0;
                else
                    _RPTF1(_CRT_WARN, "Load error: %i\n", error);
            }
            
            InternetCloseHandle(job->d->m_resourceHandle);
            job->client()->receivedAllData(job, 0);
            job->client()->receivedAllData(job);
            delete job;
        }
    } else
        return DefWindowProc(hWnd, message, wParam, lParam);
    return 0;
}

static void initializeOffScreenTransferJobWindow()
{
    if (transferJobWindowHandle)
        return;

    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = TransferJobWndProc;
    wcex.hInstance      = Widget::instanceHandle;
    wcex.lpszClassName  = kTransferJobWindowClassName;
    RegisterClassEx(&wcex);

    transferJobWindowHandle = CreateWindow(kTransferJobWindowClassName, 0, 0, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
        HWND_MESSAGE, 0, Widget::instanceHandle, 0);
    loadStatusMessage = RegisterWindowMessage(L"com.apple.WebKit.TransferJobLoadStatus");
}

TransferJobInternal::~TransferJobInternal()
{
    if (m_fileHandle)
        CloseHandle(m_fileHandle);
}

TransferJob::~TransferJob()
{
    if (d->m_jobId)
        removeFromOutstandingJobs(d->m_jobId);
}

static void __stdcall transferJobStatusCallback(HINTERNET internetHandle, DWORD_PTR timerId, DWORD internetStatus, LPVOID statusInformation, DWORD statusInformationLength)
{
    switch (internetStatus) {
    case INTERNET_STATUS_HANDLE_CREATED:
    case INTERNET_STATUS_REQUEST_COMPLETE:
        JobLoadStatus* jobLoadStatus = new JobLoadStatus;
        jobLoadStatus->internetStatus = internetStatus;
        jobLoadStatus->dwResult = LPINTERNET_ASYNC_RESULT(statusInformation)->dwResult;
        PostMessage(transferJobWindowHandle, loadStatusMessage, (WPARAM)timerId, (LPARAM)jobLoadStatus);
    }
}

bool TransferJob::start(DocLoader* docLoader)
{
    if (d->URL.isLocalFile()) {
        DeprecatedString path = d->URL.path();
        // windows does not enjoy a leading slash on paths
        if (path[0] == '/')
            path = path.mid(1);
        d->m_fileHandle = CreateFileA(path.ascii(), GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

        if (d->m_fileHandle == INVALID_HANDLE_VALUE) {
            delete this;
            return false;
        }

        d->m_fileLoadTimer.startOneShot(0.0);
        return true;
    } else {
         // leak the Internet for now
        LPCWSTR userAgent = reinterpret_cast<const WCHAR*>(docLoader->frame()->userAgent().unicode());
        static HINTERNET internetHandle = InternetOpen(userAgent, INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
        if (!internetHandle) {
            delete this;
            return false;
        }
        static INTERNET_STATUS_CALLBACK callbackHandle = InternetSetStatusCallback(internetHandle, transferJobStatusCallback);

        initializeOffScreenTransferJobWindow();
        d->m_jobId = addToOutstandingJobs(this);

        HINTERNET urlHandle = InternetOpenUrlA(internetHandle, d->URL.url().ascii(), NULL, -1, 0, (DWORD_PTR)d->m_jobId);

        if (urlHandle == INVALID_HANDLE_VALUE) {
            delete this;
            return false;
        }
        d->m_threadId = GetCurrentThreadId();

        return true;
    }
}

void TransferJob::fileLoadTimer(Timer<TransferJob>* timer)
{
    bool result = false;
    DWORD bytesRead = 0;

    do {
        const int bufferSize = 8192;
        char buffer[bufferSize];
        result = ReadFile(d->m_fileHandle, &buffer, bufferSize, &bytesRead, NULL); 
        d->client->receivedData(this, buffer, bytesRead);
        // Check for end of file. 
    } while (result && bytesRead);

    CloseHandle(d->m_fileHandle);
    d->m_fileHandle = 0;

    d->client->receivedAllData(this, 0);
    d->client->receivedAllData(this);
}

void TransferJob::cancel()
{
    if (d->m_resourceHandle)
        InternetCloseHandle(d->m_resourceHandle);
    else
        d->m_fileLoadTimer.stop();

    d->client->receivedAllData(this, 0);
    d->client->receivedAllData(this);

    if (!d->m_resourceHandle)
        // Async load canceled before we have a handle -- mark ourselves as in error, to be deleted later.
        setError(1);
}

} // namespace WebCore
