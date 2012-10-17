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

#ifndef ewk_navigation_data_private_h
#define ewk_navigation_data_private_h

#include "WKAPICast.h"
#include "WKEinaSharedString.h"
#include "WKNavigationData.h"
#include "WKRetainPtr.h"
#include "ewk_private.h"
#include "ewk_url_request_private.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

/**
 * \struct  _Ewk_Navigation_Data
 * @brief   Contains the navigation data details.
 */
class _Ewk_Navigation_Data : public RefCounted<_Ewk_Navigation_Data> {
public:
    RefPtr<Ewk_Url_Request> request;
    WKEinaSharedString title;
    WKEinaSharedString url;

    static PassRefPtr<_Ewk_Navigation_Data> create(WKNavigationDataRef dataRef)
    {
        return adoptRef(new _Ewk_Navigation_Data(dataRef));
    }

private:
    explicit _Ewk_Navigation_Data(WKNavigationDataRef dataRef)
        : request(Ewk_Url_Request::create(adoptWK(WKNavigationDataCopyOriginalRequest(dataRef)).get()))
        , title(AdoptWK, WKNavigationDataCopyTitle(dataRef))
        , url(AdoptWK, WKNavigationDataCopyURL(dataRef))
    { }
};

typedef struct _Ewk_Navigation_Data Ewk_Navigation_Data;

#endif // ewk_navigation_data_private_h
