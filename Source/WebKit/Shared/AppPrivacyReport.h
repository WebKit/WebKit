/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include <WebCore/ResourceRequest.h>

namespace WebKit {

enum class LastNavigationWasAppInitiated : bool { No, Yes };

#if PLATFORM(COCOA)
struct AppPrivacyReportTestingData {

    void setDidPerformSoftUpdate()
    {
        didPerformSoftUpdate = true;
    }

    void clearAppPrivacyReportTestingData()
    {
        hasLoadedAppInitiatedRequestTesting = false;
        hasLoadedNonAppInitiatedRequestTesting = false;
        didPerformSoftUpdate = false;
    }

    void didLoadAppInitiatedRequest(bool isAppInitiated)
    {
        isAppInitiated ? hasLoadedAppInitiatedRequestTesting = true : hasLoadedNonAppInitiatedRequestTesting = true;
    }

    void encode(IPC::Encoder& encoder) const
    {
        encoder << hasLoadedAppInitiatedRequestTesting;
        encoder << hasLoadedNonAppInitiatedRequestTesting;
        encoder << didPerformSoftUpdate;
    }

    static std::optional<AppPrivacyReportTestingData> decode(IPC::Decoder& decoder)
    {
        std::optional<bool> hasLoadedAppInitiatedRequestTesting;
        decoder >> hasLoadedAppInitiatedRequestTesting;
        if (!hasLoadedAppInitiatedRequestTesting)
            return std::nullopt;

        std::optional<bool> hasLoadedNonAppInitiatedRequestTesting;
        decoder >> hasLoadedNonAppInitiatedRequestTesting;
        if (!hasLoadedNonAppInitiatedRequestTesting)
            return std::nullopt;

        std::optional<bool> didPerformSoftUpdate;
        decoder >> didPerformSoftUpdate;
        if (!didPerformSoftUpdate)
            return std::nullopt;

        return {{ *hasLoadedAppInitiatedRequestTesting, *hasLoadedNonAppInitiatedRequestTesting, *didPerformSoftUpdate }};
    }

    bool hasLoadedAppInitiatedRequestTesting { false };
    bool hasLoadedNonAppInitiatedRequestTesting { false };
    bool didPerformSoftUpdate { false };
};
#endif

}

