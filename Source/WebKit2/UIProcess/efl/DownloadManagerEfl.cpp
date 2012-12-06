/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DownloadManagerEfl.h"

#include "DownloadProxy.h"
#include "EwkViewImpl.h"
#include "WKContext.h"
#include "WKString.h"
#include "ewk_context_private.h"
#include "ewk_error_private.h"
#include "ewk_view.h"

using namespace EwkViewCallbacks;

namespace WebKit {

static inline DownloadManagerEfl* toDownloadManagerEfl(const void* clientInfo)
{
    return static_cast<DownloadManagerEfl*>(const_cast<void*>(clientInfo));
}

WKStringRef DownloadManagerEfl::decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef wkDownload, WKStringRef filename, bool* /*allowOverwrite*/, const void* clientInfo)
{
    EwkDownloadJob* download = toDownloadManagerEfl(clientInfo)->downloadJob(toImpl(wkDownload)->downloadID());
    ASSERT(download);

    download->setSuggestedFileName(toImpl(filename)->string().utf8().data());

    // We send the new download signal on the Ewk_View only once we have received the response
    // and the suggested file name.
    download->viewImpl()->smartCallback<DownloadJobRequested>().call(download);

    // DownloadSoup expects the destination to be a URL.
    String destination = ASCIILiteral("file://") + String::fromUTF8(download->destination());

    return WKStringCreateWithUTF8CString(destination.utf8().data());
}

void DownloadManagerEfl::didReceiveResponse(WKContextRef, WKDownloadRef wkDownload, WKURLResponseRef wkResponse, const void* clientInfo)
{
    EwkDownloadJob* download = toDownloadManagerEfl(clientInfo)->downloadJob(toImpl(wkDownload)->downloadID());
    ASSERT(download);
    download->setResponse(EwkUrlResponse::create(wkResponse));
}

void DownloadManagerEfl::didCreateDestination(WKContextRef, WKDownloadRef wkDownload, WKStringRef /*path*/, const void* clientInfo)
{
    EwkDownloadJob* download = toDownloadManagerEfl(clientInfo)->downloadJob(toImpl(wkDownload)->downloadID());
    ASSERT(download);

    download->setState(EWK_DOWNLOAD_JOB_STATE_DOWNLOADING);
}

void DownloadManagerEfl::didReceiveData(WKContextRef, WKDownloadRef wkDownload, uint64_t length, const void* clientInfo)
{
    EwkDownloadJob* download = toDownloadManagerEfl(clientInfo)->downloadJob(toImpl(wkDownload)->downloadID());
    ASSERT(download);
    download->incrementReceivedData(length);
}

void DownloadManagerEfl::didFail(WKContextRef, WKDownloadRef wkDownload, WKErrorRef error, const void* clientInfo)
{
    DownloadManagerEfl* downloadManager = toDownloadManagerEfl(clientInfo);
    uint64_t downloadId = toImpl(wkDownload)->downloadID();
    EwkDownloadJob* download = downloadManager->downloadJob(downloadId);
    ASSERT(download);

    OwnPtr<EwkError> ewkError = EwkError::create(error);
    download->setState(EWK_DOWNLOAD_JOB_STATE_FAILED);
    Ewk_Download_Job_Error downloadError = { download, ewkError.get() };
    download->viewImpl()->smartCallback<DownloadJobFailed>().call(&downloadError);
    downloadManager->unregisterDownloadJob(downloadId);
}

void DownloadManagerEfl::didCancel(WKContextRef, WKDownloadRef wkDownload, const void* clientInfo)
{
    DownloadManagerEfl* downloadManager = toDownloadManagerEfl(clientInfo);
    uint64_t downloadId = toImpl(wkDownload)->downloadID();
    EwkDownloadJob* download = downloadManager->downloadJob(downloadId);
    ASSERT(download);

    download->setState(EWK_DOWNLOAD_JOB_STATE_CANCELLED);
    download->viewImpl()->smartCallback<DownloadJobCancelled>().call(download);
    downloadManager->unregisterDownloadJob(downloadId);
}

void DownloadManagerEfl::didFinish(WKContextRef, WKDownloadRef wkDownload, const void* clientInfo)
{
    DownloadManagerEfl* downloadManager = toDownloadManagerEfl(clientInfo);
    uint64_t downloadId = toImpl(wkDownload)->downloadID();
    EwkDownloadJob* download = downloadManager->downloadJob(downloadId);
    ASSERT(download);

    download->setState(EWK_DOWNLOAD_JOB_STATE_FINISHED);
    download->viewImpl()->smartCallback<DownloadJobFinished>().call(download);
    downloadManager->unregisterDownloadJob(downloadId);
}

DownloadManagerEfl::DownloadManagerEfl(EwkContext* context)
    : m_context(context)
{
    WKContextDownloadClient wkDownloadClient;
    memset(&wkDownloadClient, 0, sizeof(WKContextDownloadClient));

    wkDownloadClient.version = kWKContextDownloadClientCurrentVersion;
    wkDownloadClient.clientInfo = this;
    wkDownloadClient.didCancel = didCancel;
    wkDownloadClient.decideDestinationWithSuggestedFilename = decideDestinationWithSuggestedFilename;
    wkDownloadClient.didCreateDestination = didCreateDestination;
    wkDownloadClient.didReceiveResponse = didReceiveResponse;
    wkDownloadClient.didReceiveData = didReceiveData;
    wkDownloadClient.didFail = didFail;
    wkDownloadClient.didFinish = didFinish;

    WKContextSetDownloadClient(toAPI(context->webContext().get()), &wkDownloadClient);
}

DownloadManagerEfl::~DownloadManagerEfl()
{
    WKContextSetDownloadClient(toAPI(m_context->webContext().get()), 0);
}

void DownloadManagerEfl::registerDownload(DownloadProxy* download, EwkViewImpl* viewImpl)
{
    uint64_t downloadId = download->downloadID();
    if (m_downloadJobs.contains(downloadId))
        return;

    RefPtr<EwkDownloadJob> ewkDownload = EwkDownloadJob::create(download, viewImpl);
    m_downloadJobs.add(downloadId, ewkDownload);
}

EwkDownloadJob* DownloadManagerEfl::downloadJob(uint64_t id) const
{
    return m_downloadJobs.get(id).get();
}

void DownloadManagerEfl::unregisterDownloadJob(uint64_t id)
{
    m_downloadJobs.remove(id);
}

} // namespace WebKit
