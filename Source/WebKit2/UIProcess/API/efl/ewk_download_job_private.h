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

#ifndef ewk_download_job_private_h
#define ewk_download_job_private_h

#include "WKBase.h"
#include "WKEinaSharedString.h"
#include "ewk_url_request_private.h"
#include "ewk_url_response_private.h"
#include <Evas.h>
#include <wtf/PassRefPtr.h>

namespace WebKit {
class DownloadProxy;
}

/**
 * \struct  Ewk_Download_Job
 * @brief   Contains the download data.
 */
class Ewk_Download_Job : public RefCounted<Ewk_Download_Job> {
public:
    WebKit::DownloadProxy* downloadProxy;
    Evas_Object* view;
    Ewk_Download_Job_State state;
    RefPtr<Ewk_Url_Request> request;
    RefPtr<Ewk_Url_Response> response;
    double startTime;
    double endTime;
    uint64_t downloaded; /**< length already downloaded */
    WKEinaSharedString destination;
    WKEinaSharedString suggestedFilename;

    static PassRefPtr<Ewk_Download_Job> create(WebKit::DownloadProxy* download, Evas_Object* ewkView)
    {
        return adoptRef(new Ewk_Download_Job(download, ewkView));
    }

private:
    Ewk_Download_Job(WebKit::DownloadProxy* download, Evas_Object* ewkView)
        : downloadProxy(download)
        , view(ewkView)
        , state(EWK_DOWNLOAD_JOB_STATE_NOT_STARTED)
        , startTime(-1)
        , endTime(-1)
        , downloaded(0)
    { }
};

typedef struct Ewk_Error Ewk_Error;

uint64_t ewk_download_job_id_get(const Ewk_Download_Job*);
Evas_Object* ewk_download_job_view_get(const Ewk_Download_Job*);
void ewk_download_job_state_set(Ewk_Download_Job*, Ewk_Download_Job_State);
void ewk_download_job_received_data(Ewk_Download_Job*, uint64_t length);
void ewk_download_job_response_set(Ewk_Download_Job*, PassRefPtr<Ewk_Url_Response>);
void ewk_download_job_suggested_filename_set(Ewk_Download_Job*, const char* suggestedFilename);

#endif // ewk_download_job_private_h
