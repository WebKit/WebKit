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
#include "KURL.h"
#include "formdata.h"
#include <windows.h>
#include <wininet.h>

namespace WebCore {
    
TransferJobInternal::~TransferJobInternal()
{
    if (m_fileHandle)
        CloseHandle(m_fileHandle);
}

TransferJob::~TransferJob()
{
}

static void __stdcall transferJobStatusCallback(HINTERNET internetHandle, DWORD_PTR context, DWORD internetStatus, LPVOID statusInformation, DWORD statusInformationLength)
{
    TransferJob* job = (TransferJob*)context;

    if (internetStatus == INTERNET_STATUS_HANDLE_CREATED)
        job->d->m_resourceHandle = HINTERNET(LPINTERNET_ASYNC_RESULT(statusInformation)->dwResult);
    else if (internetStatus == INTERNET_STATUS_REQUEST_COMPLETE) {
        HINTERNET resourceHandle = job->d->m_resourceHandle;

        char buffer[32728];
        DWORD bytesRead = 0;
        DWORD totalBytes = 0;
        bool ok = false;
        while ((ok = InternetReadFile(resourceHandle, buffer, 32728, &bytesRead)) && bytesRead) {
            job->client()->receivedData(job, buffer, bytesRead);
            totalBytes += bytesRead;
            bytesRead = 0;
        }
         
        if (ok) {
            InternetCloseHandle(resourceHandle);
            job->client()->receivedAllData(job, 0);
            job->client()->receivedAllData(job);
        }
    }
}

bool TransferJob::start(DocLoader* docLoader)
{
    if (d->URL.isLocalFile()) {
        QString path = d->URL.path();
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
        static HINTERNET internetHandle = InternetOpen(L"Spinneret", INTERNET_OPEN_TYPE_PRECONFIG, 0, 0, INTERNET_FLAG_ASYNC);
        if (!internetHandle) {
            delete this;
            return false;
        }
        static INTERNET_STATUS_CALLBACK callbackHandle = InternetSetStatusCallback(internetHandle, transferJobStatusCallback);

        HINTERNET urlHandle =
            InternetOpenUrlA(internetHandle, d->URL.url().ascii(), NULL, -1, 0, (DWORD_PTR)this);

        if (urlHandle == INVALID_HANDLE_VALUE) {
            delete this;
            return false;
        }

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
    d->m_fileLoadTimer.stop();
    d->client->receivedAllData(this, 0);
    d->client->receivedAllData(this);
}

} // namespace WebCore
