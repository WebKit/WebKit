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

#include "GlobalFrameIdentifier.h"
#include "PolicyContainer.h"
#include "SecurityOrigin.h"

namespace WebCore {

class Document;

struct NavigationRequester {
    static NavigationRequester from(Document&);

    URL url;
    Ref<SecurityOrigin> securityOrigin;
    Ref<SecurityOrigin> topOrigin;
    PolicyContainer policyContainer;
    std::optional<GlobalFrameIdentifier> globalFrameIdentifier;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<NavigationRequester> decode(Decoder&);
};

template<class Encoder>
void NavigationRequester::encode(Encoder& encoder) const
{
    encoder << url << securityOrigin.get() << topOrigin.get() << policyContainer << globalFrameIdentifier;
}

template<class Decoder>
std::optional<NavigationRequester> NavigationRequester::decode(Decoder& decoder)
{
    std::optional<URL> url;
    decoder >> url;
    if (!url)
        return std::nullopt;

    auto securityOrigin = SecurityOrigin::decode(decoder);
    if (!securityOrigin)
        return std::nullopt;

    auto topOrigin = SecurityOrigin::decode(decoder);
    if (!topOrigin)
        return std::nullopt;

    std::optional<PolicyContainer> policyContainer;
    decoder >> policyContainer;
    if (!policyContainer)
        return std::nullopt;

    std::optional<std::optional<GlobalFrameIdentifier>> globalFrameIdentifier;
    decoder >> globalFrameIdentifier;
    if (!globalFrameIdentifier)
        return std::nullopt;

    return NavigationRequester { WTFMove(*url), securityOrigin.releaseNonNull(), topOrigin.releaseNonNull(), WTFMove(*policyContainer), *globalFrameIdentifier };
}

} // namespace WebCore
