/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(PHONE_NUMBER_POSSESSION)

#include <WebCore/AggregatorResponseDataFields.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class AggregatorResponseData final : public RefCounted<AggregatorResponseData> {
    WTF_MAKE_TZONE_ALLOCATED(AggregatorResponseData);
public:
    static Ref<AggregatorResponseData> create(AggregatorResponseDataFields&& data) { return adoptRef(*new AggregatorResponseData(WTF::move(data))); }

    RefPtr<ArrayBuffer> encryptedProviderErrorInfo() const { return m_data.encryptedProviderErrorInfo; }
    RefPtr<ArrayBuffer> encryptedZKPEncryptionKey() const { return m_data.encryptedZKPEncryptionKey; }

private:
    explicit AggregatorResponseData(AggregatorResponseDataFields&& data)
        : m_data(WTF::move(data))
    {
    }

    const AggregatorResponseDataFields m_data;
};

} // namespace WebCore

#endif // ENABLE(PHONE_NUMBER_POSSESSION)
