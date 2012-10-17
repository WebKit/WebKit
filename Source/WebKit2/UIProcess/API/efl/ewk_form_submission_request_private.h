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

#ifndef ewk_form_submission_request_private_h
#define ewk_form_submission_request_private_h

#include "WKDictionary.h"
#include "WKFormSubmissionListener.h"
#include "WKRetainPtr.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

typedef struct _Ewk_Form_Submission_Request Ewk_Form_Submission_Request;

class _Ewk_Form_Submission_Request : public RefCounted<_Ewk_Form_Submission_Request> {
public:
    WKRetainPtr<WKDictionaryRef> wkValues;
    WKRetainPtr<WKFormSubmissionListenerRef> wkListener;
    bool handledRequest;

    ~_Ewk_Form_Submission_Request()
    {
        // Make sure the request is always handled before destroying.
        if (!handledRequest)
            WKFormSubmissionListenerContinue(wkListener.get());
    }

    static PassRefPtr<_Ewk_Form_Submission_Request> create(WKDictionaryRef values, WKFormSubmissionListenerRef listener)
    {
        return adoptRef(new _Ewk_Form_Submission_Request(values, listener));
    }

private:
    _Ewk_Form_Submission_Request(WKDictionaryRef values, WKFormSubmissionListenerRef listener)
        : wkValues(values)
        , wkListener(listener)
        , handledRequest(false)
    { }
};

#endif // ewk_form_submission_request_private_h
