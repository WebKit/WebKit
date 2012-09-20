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

#include "WKPage.h"
#include "ewk_form_submission_request.h"
#include "ewk_form_submission_request_private.h"
#include "ewk_view_form_client_private.h"
#include "ewk_view_private.h"

static void willSubmitForm(WKPageRef, WKFrameRef /*frame*/, WKFrameRef /*sourceFrame*/, WKDictionaryRef values, WKTypeRef /*userData*/, WKFormSubmissionListenerRef listener, const void* clientInfo)
{
    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));

    Ewk_Form_Submission_Request* request = ewk_form_submission_request_new(values, listener);
    ewk_view_form_submission_request_new(ewkView, request);
    ewk_form_submission_request_unref(request);
}

void ewk_view_form_client_attach(WKPageRef pageRef, Evas_Object* ewkView)
{
    WKPageFormClient formClient;
    memset(&formClient, 0, sizeof(WKPageFormClient));
    formClient.version = kWKPageFormClientCurrentVersion;
    formClient.clientInfo = ewkView;
    formClient.willSubmitForm = willSubmitForm;
    WKPageSetPageFormClient(pageRef, &formClient);
}
