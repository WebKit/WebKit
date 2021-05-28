/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "ArgumentCoder.h"
#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceRequest.h>

namespace WebKit {

enum class NavigatingToAppBoundDomain : bool { No, Yes };
enum class LastNavigationWasAppBound : bool { No, Yes };

using ContextDomain = WebCore::RegistrableDomain;
using RequestDomain = WebCore::RegistrableDomain;

#if PLATFORM(COCOA)
struct AppBoundNavigationTestingData {

    void setDidPerformSoftUpdate()
    {
        didPerformSoftUpdate = true;
    }
    
    void clearAppBoundNavigationDataTesting()
    {
        hasLoadedAppBoundRequestTesting = false;
        hasLoadedNonAppBoundRequestTesting = false;
        didPerformSoftUpdate = false;
        contextData.clear();
    }

    void updateAppBoundNavigationTestingData(const WebCore::ResourceRequest& request, WebCore::RegistrableDomain&& contextDomain)
    {
        request.isAppBound() ? hasLoadedAppBoundRequestTesting = true : hasLoadedNonAppBoundRequestTesting = true;
        contextData.add(WebCore::RegistrableDomain(request.url()), contextDomain);
    }

    void encode(IPC::Encoder& encoder) const
    {
        encoder << hasLoadedAppBoundRequestTesting;
        encoder << hasLoadedNonAppBoundRequestTesting;
        encoder << didPerformSoftUpdate;
        encoder << contextData;
    }

    static Optional<AppBoundNavigationTestingData> decode(IPC::Decoder& decoder)
    {
        Optional<bool> hasLoadedAppBoundRequestTesting;
        decoder >> hasLoadedAppBoundRequestTesting;
        if (!hasLoadedAppBoundRequestTesting)
            return std::nullopt;

        Optional<bool> hasLoadedNonAppBoundRequestTesting;
        decoder >> hasLoadedNonAppBoundRequestTesting;
        if (!hasLoadedNonAppBoundRequestTesting)
            return std::nullopt;
        
        Optional<bool> didPerformSoftUpdate;
        decoder >> didPerformSoftUpdate;
        if (!didPerformSoftUpdate)
            return std::nullopt;

        Optional<HashMap<RequestDomain, ContextDomain>> contextData;
        decoder >> contextData;
        if (!contextData)
            return std::nullopt;

        return {{ *hasLoadedAppBoundRequestTesting, *hasLoadedNonAppBoundRequestTesting, *didPerformSoftUpdate, WTFMove(*contextData) }};
    }

    bool hasLoadedAppBoundRequestTesting { false };
    bool hasLoadedNonAppBoundRequestTesting { false };
    bool didPerformSoftUpdate { false };
    HashMap<RequestDomain, ContextDomain> contextData;
};
#endif

}

