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
#include "WKString.h"
#include "ewk_form_submission_request_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

EwkFormSubmissionRequest::EwkFormSubmissionRequest(WKDictionaryRef values, WKFormSubmissionListenerRef listener)
    : m_wkValues(values)
    , m_wkListener(listener)
    , m_handledRequest(false)
{ }

EwkFormSubmissionRequest::~EwkFormSubmissionRequest()
{
    // Make sure the request is always handled before destroying.
    if (!m_handledRequest)
        WKFormSubmissionListenerContinue(m_wkListener.get());
}

String EwkFormSubmissionRequest::fieldValue(const String& fieldName) const
{
    ASSERT(fieldName);
    WKRetainPtr<WKStringRef> wkFieldName = adoptWK(toCopiedAPI(fieldName));
    WKStringRef wkValue = static_cast<WKStringRef>(WKDictionaryGetItemForKey(m_wkValues.get(), wkFieldName.get()));

    return wkValue ? toImpl(wkValue)->string() : String();
}

WKRetainPtr<WKArrayRef> EwkFormSubmissionRequest::fieldNames() const
{
    return adoptWK(WKDictionaryCopyKeys(m_wkValues.get()));
}

void EwkFormSubmissionRequest::submit()
{
    WKFormSubmissionListenerContinue(m_wkListener.get());
    m_handledRequest = true;
}

Eina_List* ewk_form_submission_request_field_names_get(Ewk_Form_Submission_Request* request)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkFormSubmissionRequest, request, impl, 0);

    Eina_List* names = 0;

    WKRetainPtr<WKArrayRef> wkKeys = impl->fieldNames();
    const size_t numKeys = WKArrayGetSize(wkKeys.get());
    for (size_t i = 0; i < numKeys; ++i) {
        WKStringRef wkKey = static_cast<WKStringRef>(WKArrayGetItemAtIndex(wkKeys.get(), i));
        names = eina_list_append(names, eina_stringshare_add(toImpl(wkKey)->string().utf8().data()));
    }

    return names;
}

const char* ewk_form_submission_request_field_value_get(Ewk_Form_Submission_Request* request, const char* name)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkFormSubmissionRequest, request, impl, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(name, 0);

    String value = impl->fieldValue(String::fromUTF8(name));

    return value.isNull() ?  0 : eina_stringshare_add(value.utf8().data());
}

Eina_Bool ewk_form_submission_request_submit(Ewk_Form_Submission_Request* request)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkFormSubmissionRequest, request, impl, false);

    impl->submit();

    return true;
}
