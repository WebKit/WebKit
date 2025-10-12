/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCCFURL.h"

#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

std::optional<CoreIPCCFURL> CoreIPCCFURL::createWithBaseURLAndBytes(std::optional<CoreIPCCFURL>&& baseURL, Vector<uint8_t>&& bytes)
{
    if (bytes.isEmpty()) {
        // CFURL can't hold an empty URL, unlike NSURL.
        return CoreIPCCFURL { bridge_cast([NSURL URLWithString:@""]) };
    }

    RetainPtr baseCFURL = baseURL ? baseURL->m_cfURL.get() : nullptr;
    if (RetainPtr newCFURL = adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, bytes.span().data(), bytes.size(), kCFStringEncodingUTF8, baseCFURL.get(), true)))
        return CoreIPCCFURL { WTFMove(newCFURL) };

    return std::nullopt;
}

std::optional<CoreIPCCFURL> CoreIPCCFURL::baseURL() const
{
    if (RetainPtr baseURL = CFURLGetBaseURL(m_cfURL.get()))
        return CoreIPCCFURL { WTFMove(baseURL) };
    return std::nullopt;
}

Vector<uint8_t> CoreIPCCFURL::toVector() const
{
    auto bytesLength = CFURLGetBytes(m_cfURL.get(), nullptr, 0);
    RELEASE_ASSERT(bytesLength != -1);
    Vector<uint8_t> result(bytesLength);
    CFURLGetBytes(m_cfURL.get(), result.mutableSpan().data(), bytesLength);

    return result;
}

} // namespace WebKit

