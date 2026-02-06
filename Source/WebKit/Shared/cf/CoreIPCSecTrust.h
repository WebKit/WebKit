/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if USE(CF)

#import "CoreIPCData.h"
#import "CoreIPCDate.h"
#import "CoreIPCNumber.h"
#import "CoreIPCString.h"

#import <wtf/RetainPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/spi/cocoa/SecuritySPI.h>

namespace WebKit {

#if HAVE(WK_SECURE_CODING_SECTRUST)

enum class CoreIPCSecTrustResult : uint8_t {
    Invalid = 0,
    Proceed,
    Confirm,
    Deny,
    Unspecified,
    RecoverableTrustFailure,
    FatalTrustFailure,
    OtherError
};

struct CoreIPCSecTrustData {
    using Detail = Vector<std::pair<CoreIPCString, bool>>;
    using RevocationInfoSubDictValue = Variant<CoreIPCNumber, CoreIPCData, CoreIPCDate, bool>;
    using RevocationInfoSubDict = Vector<std::pair<CoreIPCString, RevocationInfoSubDictValue>>;
    using RevocationInfoEntry = Vector<std::pair<CoreIPCString, RevocationInfoSubDict>>;
    using RevocationInfoArray = Vector<RevocationInfoEntry>;
    using InfoOption = Variant<CoreIPCDate, CoreIPCString, bool, RevocationInfoArray>;
    using InfoType = Vector<std::pair<CoreIPCString, InfoOption>>;
    using PolicyDictionaryValueIsNumber = Vector<std::pair<CoreIPCString, CoreIPCNumber>>;
    using PolicyArrayOfArrayContainingDateOrNumbers = Vector<Vector<Variant<CoreIPCNumber, CoreIPCDate>>>;
    using PolicyArrayOfNumbers = Vector<CoreIPCNumber>;
    using PolicyArrayOfStrings = Vector<CoreIPCString>;
    using PolicyArrayOfData = Vector<CoreIPCData>;
    using PolicyVariant = Variant<bool, CoreIPCString, PolicyArrayOfNumbers, PolicyArrayOfStrings, PolicyArrayOfData, PolicyArrayOfArrayContainingDateOrNumbers, PolicyDictionaryValueIsNumber>;
    using PolicyOption = Vector<std::pair<CoreIPCString, PolicyVariant>>;
    using PolicyValue = Variant<CoreIPCString, PolicyOption>;
    using PolicyType = Vector<std::pair<CoreIPCString, PolicyValue>>;
    using ExceptionType = Vector<std::pair<CoreIPCString, Variant<CoreIPCNumber, CoreIPCData, bool>>>;

    CoreIPCSecTrustResult result { CoreIPCSecTrustResult::Invalid };
    bool anchorsOnly { false };
    bool keychainsAllowed { false };
    Vector<CoreIPCData> certificates;
    Vector<CoreIPCData> chain;
    Vector<Detail> details;
    Vector<PolicyType> policies;
    std::optional<InfoType> info;
    std::optional<CoreIPCDate> verifyDate;
    std::optional<Vector<CoreIPCData>> responses;
    std::optional<Vector<CoreIPCData>> scts;
    std::optional<Vector<CoreIPCData>> anchors;
    std::optional<Vector<CoreIPCData>> trustedLogs;
    std::optional<Vector<ExceptionType>> exceptions;
};

class CoreIPCSecTrust {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCSecTrust);
public:
    CoreIPCSecTrust() { }

    CoreIPCSecTrust(SecTrustRef);

    CoreIPCSecTrust(std::optional<WebKit::CoreIPCSecTrustData>&& data)
        : m_data(WTF::move(data)) { }

    RetainPtr<SecTrustRef> createSecTrust() const;

    std::optional<CoreIPCSecTrustData> m_data;

    enum class PolicyOptionValueShape {
        Invalid,
        Bool,
        String,
        ArrayOfNumbers,
        ArrayOfStrings,
        ArrayOfData,
        ArrayOfArrayContainingDateOrNumber,
        DictionaryValueIsNumber,
    };
    static PolicyOptionValueShape detectPolicyOptionShape(id);
};
#else // !HAVE(WK_SECURE_CODING_SECTRUST)
class CoreIPCSecTrust {
public:
    CoreIPCSecTrust()
        : m_trustData() { };

    CoreIPCSecTrust(SecTrustRef trust)
        : m_trustData(adoptCF(SecTrustSerialize(trust, NULL)))
    {
    }

    CoreIPCSecTrust(RetainPtr<CFDataRef> data)
        : m_trustData(data)
    {
    }

    CoreIPCSecTrust(std::span<const uint8_t> data)
        : m_trustData(data.empty() ? nullptr : adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size())))
    {
    }

    RetainPtr<SecTrustRef> createSecTrust() const
    {
        if (!m_trustData)
            return nullptr;

        return adoptCF(SecTrustDeserialize(m_trustData.get(), NULL));
    }

    std::span<const uint8_t> dataReference() const
    {
        if (!m_trustData)
            return { };

        return span(m_trustData.get());
    }

private:
    RetainPtr<CFDataRef> m_trustData;
};
#endif

} // namespace WebKit

#endif // USE(CF)
