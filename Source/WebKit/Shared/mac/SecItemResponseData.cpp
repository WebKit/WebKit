/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "SecItemResponseData.h"

#include "ArgumentCoders.h"
#include "ArgumentCodersCF.h"

namespace WebKit {

SecItemResponseData::SecItemResponseData(OSStatus resultCode, RetainPtr<CFTypeRef>&& resultObject)
    : m_resultObject(WTFMove(resultObject))
    , m_resultCode(resultCode)
{
}

void SecItemResponseData::encode(IPC::Encoder& encoder) const
{
    encoder << static_cast<int64_t>(m_resultCode);
    encoder << static_cast<bool>(m_resultObject);
    if (m_resultObject)
        encoder << m_resultObject;
}

std::optional<SecItemResponseData> SecItemResponseData::decode(IPC::Decoder& decoder)
{
    int64_t resultCode;
    if (!decoder.decode(resultCode))
        return std::nullopt;

    bool expectResultObject;
    if (!decoder.decode(expectResultObject))
        return std::nullopt;

    RetainPtr<CFTypeRef> result;
    if (expectResultObject && !decoder.decode(result))
        return std::nullopt;

    return {{ static_cast<OSStatus>(resultCode), WTFMove(result) }};
}

} // namespace WebKit
