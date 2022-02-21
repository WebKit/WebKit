/*
 * Copyright (C) 2021, Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebLockMode.h"
#include <wtf/CrossThreadCopier.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct WebLockManagerSnapshot {
    struct Info {
        String name;
        WebLockMode mode { WebLockMode::Exclusive };
        String clientId;

        Info isolatedCopy() const { return { name.isolatedCopy(), mode, clientId.isolatedCopy() }; }

        template<class Encoder> void encode(Encoder& encoder) const { encoder << name << mode << clientId; }
        template<class Decoder> static std::optional<Info> decode(Decoder&);
    };

    Vector<Info> held;
    Vector<Info> pending;

    WebLockManagerSnapshot isolatedCopy() const { return { crossThreadCopy(held), crossThreadCopy(pending) }; }

    template<class Encoder> void encode(Encoder& encoder) const { encoder << held << pending; }
    template<class Decoder> static std::optional<WebLockManagerSnapshot> decode(Decoder&);
};

template<class Decoder>
std::optional<WebLockManagerSnapshot::Info> WebLockManagerSnapshot::Info::decode(Decoder& decoder)
{
    std::optional<String> name;
    decoder >> name;
    if (!name)
        return std::nullopt;

    std::optional<WebLockMode> mode;
    decoder >> mode;
    if (!mode)
        return std::nullopt;

    std::optional<String> clientId;
    decoder >> clientId;
    if (!clientId)
        return std::nullopt;

    return { { WTFMove(*name), *mode, WTFMove(*clientId) } };
}

template<class Decoder>
std::optional<WebLockManagerSnapshot> WebLockManagerSnapshot::decode(Decoder& decoder)
{
    std::optional<Vector<Info>> held;
    decoder >> held;
    if (!held)
        return std::nullopt;

    std::optional<Vector<Info>> pending;
    decoder >> pending;
    if (!pending)
        return std::nullopt;

    return { { WTFMove(*held), WTFMove(*pending) } };
}

} // namespace WebCore
