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
#include "WKRetainPtr.h"
#include "WebURLRequest.h"
#include "ewk_download_job_private.h"
#include "ewk_url_response_private.h"
#include <Ecore.h>

using namespace WebKit;

Ewk_Download_Job::Ewk_Download_Job(WebKit::DownloadProxy* download, Evas_Object* ewkView)
    : m_downloadProxy(download)
    , m_view(ewkView)
    , m_state(EWK_DOWNLOAD_JOB_STATE_NOT_STARTED)
    , m_startTime(-1)
    , m_endTime(-1)
    , m_downloaded(0)
{ }

Ewk_Download_Job* ewk_download_job_ref(Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    download->ref();

    return download;
}

void ewk_download_job_unref(Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN(download);

    download->deref();
}

/**
 * @internal
 * Queries the identifier for this download
 */
uint64_t Ewk_Download_Job::id() const
{
    return m_downloadProxy->downloadID();
}

/**
 * @internal
 * Returns the view this download is attached to.
 * The view is needed to send notification signals.
 */
Evas_Object* Ewk_Download_Job::view() const
{
    return m_view;
}

Ewk_Download_Job_State ewk_download_job_state_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, EWK_DOWNLOAD_JOB_STATE_UNKNOWN);

    return download->state();
}

Ewk_Download_Job_State Ewk_Download_Job::state() const
{
    return m_state;
}

Ewk_Url_Request* ewk_download_job_request_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->request();
}

Ewk_Url_Request* Ewk_Download_Job::request() const
{
    if (!m_request) {
        WKRetainPtr<WKURLRequestRef> wkURLRequest(AdoptWK, toAPI(WebURLRequest::create(m_downloadProxy->request()).leakRef()));
        m_request = Ewk_Url_Request::create(wkURLRequest.get());
    }

    return m_request.get();
}

Ewk_Url_Response* ewk_download_job_response_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->response();
}

Ewk_Url_Response* Ewk_Download_Job::response() const
{
    return m_response.get();
}

const char* ewk_download_job_destination_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->destination();
}

const char* Ewk_Download_Job::destination() const
{
    return m_destination;
}

Eina_Bool ewk_download_job_destination_set(Ewk_Download_Job* download, const char* destination)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(destination, false);

    download->setDestination(destination);

    return true;
}

void Ewk_Download_Job::setDestination(const char* destination)
{
    m_destination = destination;
}

const char* ewk_download_job_suggested_filename_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->suggestedFileName();
}

const char* Ewk_Download_Job::suggestedFileName() const
{
    return m_suggestedFilename;
}

Eina_Bool ewk_download_job_cancel(Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, false);

    return download->cancel();
}

bool Ewk_Download_Job::cancel()
{
    if (m_state != EWK_DOWNLOAD_JOB_STATE_DOWNLOADING)
        return false;

    m_state = EWK_DOWNLOAD_JOB_STATE_CANCELLING;
    m_downloadProxy->cancel();
    return true;
}

double ewk_download_job_estimated_progress_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->estimatedProgress();
}

double Ewk_Download_Job::estimatedProgress() const
{
    if (!m_response)
        return 0;

    const unsigned long contentLength = m_response->contentLength();
    if (!contentLength)
        return 0;

    return static_cast<double>(m_downloaded) / contentLength;
}

double ewk_download_job_elapsed_time_get(const Ewk_Download_Job* download)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(download, 0);

    return download->elapsedTime();
}

double Ewk_Download_Job::elapsedTime() const
{
    // Download has not started yet.
    if (m_startTime < 0)
        return 0;

    // Download had ended, return the time elapsed between the
    // download start and the end event.
    if (m_endTime >= 0)
        return m_endTime - m_startTime;

    // Download is still going.
    return ecore_time_get() - m_startTime;
}

/**
 * @internal
 * Sets the URL @a response for this @a download.
 */
void Ewk_Download_Job::setResponse(PassRefPtr<Ewk_Url_Response> response)
{
    ASSERT(response);

    m_response = response;
}

/**
 * @internal
 * Sets the suggested file name for this @a download.
 */
void Ewk_Download_Job::setSuggestedFileName(const char* suggestedFilename)
{
    m_suggestedFilename = suggestedFilename;
}

/**
 * @internal
 * Report a given amount of data was received.
 */
void Ewk_Download_Job::incrementReceivedData(uint64_t length)
{
    m_downloaded += length;
}

/**
 * @internal
 * Sets the state of the download.
 */
void Ewk_Download_Job::setState(Ewk_Download_Job_State state)
{
    m_state = state;

    switch (state) {
    case EWK_DOWNLOAD_JOB_STATE_DOWNLOADING:
        m_startTime = ecore_time_get();
        break;
    case EWK_DOWNLOAD_JOB_STATE_FAILED:
    case EWK_DOWNLOAD_JOB_STATE_CANCELLED:
    case EWK_DOWNLOAD_JOB_STATE_FINISHED:
        m_endTime = ecore_time_get();
        break;
    default:
        break;
    }
}
