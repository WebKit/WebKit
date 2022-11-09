/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "CacheStorageConnection.h"

#include "FetchResponse.h"
#include <wtf/CryptographicallyRandomNumber.h>

namespace WebCore {

using namespace WebCore::DOMCacheEngine;

static inline uint64_t formDataSize(const FormData& formData)
{
    if (isMainThread())
        return formData.lengthInBytes();

    uint64_t resultSize;
    callOnMainThreadAndWait([formData = formData.isolatedCopy(), &resultSize] {
        resultSize = formData->lengthInBytes();
    });
    return resultSize;
}

uint64_t CacheStorageConnection::computeRealBodySize(const DOMCacheEngine::ResponseBody& body)
{
    uint64_t result = 0;
    WTF::switchOn(body, [&] (const Ref<FormData>& formData) {
        result = formDataSize(formData);
    }, [&] (const Ref<SharedBuffer>& buffer) {
        result = buffer->size();
    }, [] (const std::nullptr_t&) {
    });
    return result;
}

uint64_t CacheStorageConnection::computeRecordBodySize(const FetchResponse& response, const DOMCacheEngine::ResponseBody& body)
{
    if (!response.opaqueLoadIdentifier()) {
        ASSERT(response.tainting() != ResourceResponse::Tainting::Opaque);
        return computeRealBodySize(body);
    }

    return m_opaqueResponseToSizeWithPaddingMap.ensure(response.opaqueLoadIdentifier(), [&] () {
        uint64_t realSize = computeRealBodySize(body);

        // Padding the size as per https://github.com/whatwg/storage/issues/31.
        uint64_t sizeWithPadding = realSize + static_cast<uint64_t>(cryptographicallyRandomUnitInterval() * 128000);
        sizeWithPadding = ((sizeWithPadding / 32000) + 1) * 32000;

        m_opaqueResponseToSizeWithPaddingMap.set(response.opaqueLoadIdentifier(), sizeWithPadding);
        return sizeWithPadding;
    }).iterator->value;
}

} // namespace WebCore
