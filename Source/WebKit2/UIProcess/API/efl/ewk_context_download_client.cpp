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

#include "DownloadProxy.h"
#include "WKAPICast.h"
#include "WKContext.h"
#include "WKString.h"
#include "ewk_context_download_client_private.h"
#include "ewk_context_private.h"
#include "ewk_download_job.h"
#include "ewk_download_job_private.h"
#include "ewk_error_private.h"
#include "ewk_url_response.h"
#include "ewk_url_response_private.h"
#include "ewk_view_private.h"
#include <string.h>
#include <wtf/text/CString.h>

using namespace WebKit;

static inline Ewk_Context* toEwkContext(const void* clientInfo)
{
    return static_cast<Ewk_Context*>(const_cast<void*>(clientInfo));
}

static WKStringRef decideDestinationWithSuggestedFilename(WKContextRef, WKDownloadRef wkDownload, WKStringRef filename, bool* /*allowOverwrite*/, const void* clientInfo)
{
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), toImpl(wkDownload)->downloadID());
    ASSERT(download);

    ewk_download_job_suggested_filename_set(download, toImpl(filename)->string().utf8().data());

    // We send the new download signal on the Ewk_View only once we have received the response
    // and the suggested file name.
    ewk_view_download_job_requested(ewk_download_job_view_get(download), download);

    // DownloadSoup expects the destination to be a URL.
    String destination = String("file://") + String::fromUTF8(ewk_download_job_destination_get(download));

    return WKStringCreateWithUTF8CString(destination.utf8().data());
}

static void didReceiveResponse(WKContextRef, WKDownloadRef wkDownload, WKURLResponseRef wkResponse, const void* clientInfo)
{
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), toImpl(wkDownload)->downloadID());
    ASSERT(download);
    RefPtr<Ewk_Url_Response> response = Ewk_Url_Response::create(wkResponse);
    ewk_download_job_response_set(download, response.get());
}

static void didCreateDestination(WKContextRef, WKDownloadRef wkDownload, WKStringRef /*path*/, const void* clientInfo)
{
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), toImpl(wkDownload)->downloadID());
    ASSERT(download);

    ewk_download_job_state_set(download, EWK_DOWNLOAD_JOB_STATE_DOWNLOADING);
}

static void didReceiveData(WKContextRef, WKDownloadRef wkDownload, uint64_t length, const void* clientInfo)
{
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), toImpl(wkDownload)->downloadID());
    ASSERT(download);
    ewk_download_job_received_data(download, length);
}

static void didFail(WKContextRef, WKDownloadRef wkDownload, WKErrorRef error, const void *clientInfo)
{
    uint64_t downloadId = toImpl(wkDownload)->downloadID();
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), downloadId);
    ASSERT(download);

    Ewk_Error* ewkError = ewk_error_new(error);
    ewk_download_job_state_set(download, EWK_DOWNLOAD_JOB_STATE_FAILED);
    ewk_view_download_job_failed(ewk_download_job_view_get(download), download, ewkError);
    ewk_error_free(ewkError);
    ewk_context_download_job_remove(toEwkContext(clientInfo), downloadId);
}

static void didCancel(WKContextRef, WKDownloadRef wkDownload, const void *clientInfo)
{
    uint64_t downloadId = toImpl(wkDownload)->downloadID();
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), downloadId);
    ASSERT(download);

    ewk_download_job_state_set(download, EWK_DOWNLOAD_JOB_STATE_CANCELLED);
    ewk_view_download_job_cancelled(ewk_download_job_view_get(download), download);
    ewk_context_download_job_remove(toEwkContext(clientInfo), downloadId);
}

static void didFinish(WKContextRef, WKDownloadRef wkDownload, const void *clientInfo)
{
    uint64_t downloadId = toImpl(wkDownload)->downloadID();
    Ewk_Download_Job* download = ewk_context_download_job_get(toEwkContext(clientInfo), downloadId);
    ASSERT(download);

    ewk_download_job_state_set(download, EWK_DOWNLOAD_JOB_STATE_FINISHED);
    ewk_view_download_job_finished(ewk_download_job_view_get(download), download);
    ewk_context_download_job_remove(toEwkContext(clientInfo), downloadId);
}

void ewk_context_download_client_attach(Ewk_Context* ewkContext)
{
    WKContextDownloadClient wkDownloadClient;
    memset(&wkDownloadClient, 0, sizeof(WKContextDownloadClient));

    wkDownloadClient.version = kWKContextDownloadClientCurrentVersion;
    wkDownloadClient.clientInfo = ewkContext;
    wkDownloadClient.didCancel = didCancel;
    wkDownloadClient.decideDestinationWithSuggestedFilename = decideDestinationWithSuggestedFilename;
    wkDownloadClient.didCreateDestination = didCreateDestination;
    wkDownloadClient.didReceiveResponse = didReceiveResponse;
    wkDownloadClient.didReceiveData = didReceiveData;
    wkDownloadClient.didFail = didFail;
    wkDownloadClient.didFinish = didFinish;

    WKContextSetDownloadClient(ewk_context_WKContext_get(ewkContext), &wkDownloadClient);
}


