/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include "WebResourceLoadStatisticsStore.h"
#include <wtf/text/WTFString.h>

namespace API {

class ResourceLoadStatisticsThirdParty final : public ObjectImpl<Object::Type::ResourceLoadStatisticsThirdParty> {
public:
    static Ref<ResourceLoadStatisticsThirdParty> create(WebKit::WebResourceLoadStatisticsStore::ThirdPartyData&& thirdPartyData)
    {
        return adoptRef(*new ResourceLoadStatisticsThirdParty(WTFMove(thirdPartyData)));
    }

    ResourceLoadStatisticsThirdParty(WebKit::WebResourceLoadStatisticsStore::ThirdPartyData&& thirdPartyData)
        : m_thirdPartyData(WTFMove(thirdPartyData))
    {
    }

    const WTF::String& thirdPartyDomain() const { return m_thirdPartyData.thirdPartyDomain.string(); }
    const Vector<WebKit::WebResourceLoadStatisticsStore::ThirdPartyDataForSpecificFirstParty>& underFirstParties() const { return m_thirdPartyData.underFirstParties; }

private:
    const WebKit::WebResourceLoadStatisticsStore::ThirdPartyData m_thirdPartyData;
};

} // namespace API
