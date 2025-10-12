/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CoreIPCPKDateComponentsRange.h"

#if USE(PASSKIT) && HAVE(WK_SECURE_CODING_PKDATECOMPONENTSRANGE)

#import "ArgumentCodersCocoa.h"
#import <pal/cocoa/PassKitSoftLink.h>

namespace WebKit {

CoreIPCPKDateComponentsRange::CoreIPCPKDateComponentsRange(PKDateComponentsRange *p)
    : m_data(p ? std::optional<CoreIPCPKDateComponentsRangeData> { { p.startDateComponents, p.endDateComponents } } : std::nullopt)
{
}

CoreIPCPKDateComponentsRange::CoreIPCPKDateComponentsRange(std::optional<CoreIPCPKDateComponentsRangeData>&& data)
    : m_data { WTFMove(data) }
{
}

RetainPtr<id> CoreIPCPKDateComponentsRange::toID() const
{
    if (!m_data)
        return { };

    return adoptNS([[PAL::getPKDateComponentsRangeClassSingleton() alloc]
        initWithStartDateComponents:m_data->startDateComponents.toID().get()
        endDateComponents:m_data->endDateComponents.toID().get()]);
}

} // namespace WebKit

namespace IPC {

template<> void encodeObjectDirectly<PKDateComponentsRange>(Encoder& encoder, PKDateComponentsRange *instance)
{
    encoder << (instance ? std::optional(WebKit::CoreIPCPKDateComponentsRange(instance)) : std::nullopt);
}

template<> std::optional<RetainPtr<id>> decodeObjectDirectlyRequiringAllowedClasses<PKDateComponentsRange>(Decoder& decoder)
{
    return decoder.decode<std::optional<WebKit::CoreIPCPKDateComponentsRange>>()
        .and_then([](const std::optional<WebKit::CoreIPCPKDateComponentsRange>& inner) -> std::optional<RetainPtr<id>> {
            return inner ? inner->toID() : nullptr;
        });
}

} // namespace IPC

#endif
