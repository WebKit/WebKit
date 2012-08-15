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
#include "ewk_download_job.h"

#include "DownloadProxy.h"
#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKRetainPtr.h"
#include "WebURLRequest.h"
#include "ewk_download_job_private.h"
#include "ewk_url_request_private.h"
#include <Ecore.h>

using namespace WebKit;

/**
 * \struct  _Ewk_Download_Job
 * @brief   Contains the download data.
 */
struct _Ewk_Download_Job {
    unsigned int __ref; /**< the reference count of the object */
    DownloadProxy* downloadProxy;
    Evas_Object* view;
    Ewk_Download_Job_State state;
    Ewk_Url_Request* request;
    Ewk_Url_Response* response;
    double startTime;
    double endTime;
    uint64_t downloaded; /**< length already downloaded */
    WKEinaSharedString destination;
    WKEinaSharedString suggestedFilename;

    _Ewk_Download_Job(DownloadProxy* download, Evas_Object* ewkView)
        : __ref(1)
        , downloadProxy(download)
        , view(ewkView)
        , state(EWK_DOWNLOAD_JOB_STATE_NOT_STARTED)
        , request(0)
        , response(0)
        , startTime(-1)
        , endTime(-1)
        , downloaded(0)
    { }

    ~_Ewk_Download_Job()
    {
        ASSERT(!__ref);
        if (request)
            ewk_url_request_unref(request);
        if (response)
            ewk_url_response_unref(response);
    }
};

void ewk_download_job_ref(Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN(download);

    ++download->__ref;
}

void ewk_download_job_unref(Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN(download);

    if (--download->__ref)
        return;

    delete download;
}

/**
 * @internal
 * Queries the identifier for this download
 */
uint64_t ewk_download_job_id_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(download->downloadProxy, 0);

    return download->downloadProxy->downloadID();
}

/**
 * @internal
 * Returns the view this download is attached to.
 * The view is needed to send notification signals.
 */
Evas_Object* ewk_download_job_view_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->view;
}

Ewk_Download_Job_State ewk_download_job_state_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, EWK_DOWNLOAD_JOB_STATE_UNKNOWN);

    return download->state;
}

Ewk_Url_Request* ewk_download_job_request_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    if (!download->request) {
        EINA_SAFETY_ON_NULL_RETURN_VAL(download->downloadProxy, 0);
        WKRetainPtr<WKURLRequestRef> wkURLRequest(AdoptWK, toAPI(WebURLRequest::create(download->downloadProxy->request()).leakRef()));
        const_cast<Ewk_Download_Job*>(download)->request = ewk_url_request_new(wkURLRequest.get());
    }

    return download->request;
}

Ewk_Url_Response* ewk_download_job_response_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->response;
}

const char* ewk_download_job_destination_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->destination;
}

Eina_Bool ewk_download_job_destination_set(Ewk_Download_Job* download, const char* destination)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(destination, false);

    download->destination = destination;

    return true;
}

const char *ewk_download_job_suggested_filename_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->suggestedFilename;
}

Eina_Bool ewk_download_job_cancel(Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(download->downloadProxy, false);

    if (download->state != EWK_DOWNLOAD_JOB_STATE_DOWNLOADING)
        return false;

    download->state = EWK_DOWNLOAD_JOB_STATE_CANCELLING;
    download->downloadProxy->cancel();
    return true;
}

double ewk_download_job_estimated_progress_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    if (!download->response)
        return 0;

    const unsigned long contentLength = ewk_url_response_content_length_get(download->response);
    if (!contentLength)
        return 0;

    return static_cast<double>(download->downloaded) / contentLength;
}

double ewk_download_job_elapsed_time_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    // Download has not started yet.
    if (download->startTime < 0)
        return 0;

    // Download had ended, return the time elapsed between the
    // download start and the end event.
    if (download->endTime >= 0)
        return download->endTime - download->startTime;

    // Download is still going.
    return ecore_time_get() - download->startTime;
}

/**
 * @internal
 * Sets the URL @a response for this @a download.
 */
void ewk_download_job_response_set(Ewk_Download_Job* download, Ewk_Url_Response* response)
{
    EINA_SAFETY_ON_NULL_RETURN(download);
    EINA_SAFETY_ON_NULL_RETURN(response);

    ewk_url_response_ref(response);
    download->response = response;
}

/**
 * @internal
 * Sets the suggested file name for this @a download.
 */
void ewk_download_job_suggested_filename_set(Ewk_Download_Job* download, const char* suggestedFilename)
{
    EINA_SAFETY_ON_NULL_RETURN(download);

    download->suggestedFilename = suggestedFilename;
}

/**
 * @internal
 * Report a given amount of data was received.
 */
void ewk_download_job_received_data(Ewk_Download_Job* download, uint64_t length)
{
    EINA_SAFETY_ON_NULL_RETURN(download);

    download->downloaded += length;
}

/**
 * @internal
 * Sets the state of the download.
 */
void ewk_download_job_state_set(Ewk_Download_Job* download, Ewk_Download_Job_State state)
{
    download->state = state;

    // Update start time if the download has started
    if (state == EWK_DOWNLOAD_JOB_STATE_DOWNLOADING)
        download->startTime = ecore_time_get();

    // Update end time if the download has finished (successfully or not)
    if (state == EWK_DOWNLOAD_JOB_STATE_FAILED
        || state == EWK_DOWNLOAD_JOB_STATE_CANCELLED
        || state == EWK_DOWNLOAD_JOB_STATE_FINISHED)
        download->endTime = ecore_time_get();
}

/**
 * @internal
 * Constructs a Ewk_Download_Job from a DownloadProxy.
 */
Ewk_Download_Job* ewk_download_job_new(DownloadProxy* download, Evas_Object* ewkView)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkView, 0);

    return new _Ewk_Download_Job(download, ewkView);
}
