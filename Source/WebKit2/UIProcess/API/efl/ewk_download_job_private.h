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
#include <Evas.h>

typedef struct _Ewk_Download_Job Ewk_Download_Job;
typedef struct _Ewk_Url_Response Ewk_Url_Response;
typedef struct _Ewk_Web_Error Ewk_Web_Error;

namespace WebKit {
class DownloadProxy;
}

Ewk_Download_Job* ewk_download_job_new(WebKit::DownloadProxy*, Evas_Object* ewkView);
uint64_t ewk_download_job_id_get(const Ewk_Download_Job*);
Evas_Object* ewk_download_job_view_get(const Ewk_Download_Job*);

void ewk_download_job_state_set(Ewk_Download_Job*, Ewk_Download_Job_State);
void ewk_download_job_cancelled(Ewk_Download_Job*);
void ewk_download_job_failed(Ewk_Download_Job*);
void ewk_download_job_finished(Ewk_Download_Job*);
void ewk_download_job_started(Ewk_Download_Job*);

void ewk_download_job_received_data(Ewk_Download_Job*, uint64_t length);
void ewk_download_job_response_set(Ewk_Download_Job*, Ewk_Url_Response*);
void ewk_download_job_suggested_filename_set(Ewk_Download_Job*, const char* suggestedFilename);

#endif // ewk_download_job_private_h
