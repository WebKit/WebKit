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
#include "ewk_form_submission_request.h"

#include "WKAPICast.h"
#include "WKArray.h"
#include "WKBase.h"
#include "WKDictionary.h"
#include "WKFormSubmissionListener.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "ewk_form_submission_request_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * \struct _Ewk_Form_Submission_Request
 * @brief Contains the form submission request data.
 */
struct _Ewk_Form_Submission_Request {
    unsigned int __ref; /**< the reference count of the object */
    WKRetainPtr<WKDictionaryRef> wkValues;
    WKRetainPtr<WKFormSubmissionListenerRef> wkListener;
    bool handledRequest;

    _Ewk_Form_Submission_Request(WKDictionaryRef values, WKFormSubmissionListenerRef listener)
        : __ref(1)
        , wkValues(values)
        , wkListener(listener)
        , handledRequest(false)
    { }

    ~_Ewk_Form_Submission_Request()
    {
        ASSERT(!__ref);

        // Make sure the request is always handled before destroying.
        if (!handledRequest)
            WKFormSubmissionListenerContinue(wkListener.get());
    }
};

Ewk_Form_Submission_Request* ewk_form_submission_request_ref(Ewk_Form_Submission_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);
    ++request->__ref;

    return request;
}

void ewk_form_submission_request_unref(Ewk_Form_Submission_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN(request);

    if (--request->__ref)
        return;

    delete request;
}

Eina_List* ewk_form_submission_request_field_names_get(Ewk_Form_Submission_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);

    Eina_List* names = 0;

    WKRetainPtr<WKArrayRef> wkKeys(AdoptWK, WKDictionaryCopyKeys(request->wkValues.get()));
    const size_t numKeys = WKArrayGetSize(wkKeys.get());
    for (size_t i = 0; i < numKeys; ++i) {
        WKStringRef wkKey = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkKeys.get(), i));
        names = eina_list_append(names, eina_stringshare_add(toImpl(wkKey)->string().utf8().data()));
    }

    return names;
}

const char* ewk_form_submission_request_field_value_get(Ewk_Form_Submission_Request* request, const char* name)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(name, 0);

    WKRetainPtr<WKStringRef> wkKey(AdoptWK, WKStringCreateWithUTF8CString(name));
    WKStringRef wkValue = static_cast<WKStringRef>(WKDictionaryGetItemForKey(request->wkValues.get(), wkKey.get()));

    return wkValue ? eina_stringshare_add(toImpl(wkValue)->string().utf8().data()) : 0;
}

Eina_Bool ewk_form_submission_request_submit(Ewk_Form_Submission_Request* request)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(request, false);

    WKFormSubmissionListenerContinue(request->wkListener.get());
    request->handledRequest = true;

    return true;
}

/**
 * @internal ewk_form_submission_request_new
 * Creates a Ewk_Form_Submission_Request from a dictionary and a WKFormSubmissionListenerRef.
 */
Ewk_Form_Submission_Request* ewk_form_submission_request_new(WKDictionaryRef values, WKFormSubmissionListenerRef listener)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(values, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(listener, 0);

    return new Ewk_Form_Submission_Request(values, listener);
}
