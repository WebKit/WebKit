/*
 * Copyright (C) 2023-2025 Apple Inc. All rights reserved.
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

#include "CoreIPCRetainPtr.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/UniqueRef.h>

namespace WebCore {
class Color;
}

namespace WebKit {

class CoreIPCCFArray;
class CoreIPCBoolean;
class CoreIPCCFCharacterSet;
class CoreIPCColor;
class CoreIPCData;
class CoreIPCDate;
class CoreIPCCFDictionary;
class CoreIPCNull;
class CoreIPCNumber;
class CoreIPCString;
class CoreIPCCFURL;
class CoreIPCCGColorSpace;
class CoreIPCSecCertificate;
class CoreIPCSecTrust;
class CoreIPCSecAccessControl;

using CFObjectValue = Variant<
    std::nullptr_t,
    CoreIPCCFArray,
    CoreIPCBoolean,
    CoreIPCCFCharacterSet,
    CoreIPCData,
    CoreIPCDate,
    CoreIPCCFDictionary,
    CoreIPCNull,
    CoreIPCNumber,
    CoreIPCString,
    CoreIPCCFURL,
    CoreIPCSecCertificate,
    CoreIPCSecTrust,
    CoreIPCCGColorSpace,
    WebCore::Color,
    CoreIPCSecAccessControl
>;

class CoreIPCCFType {
public:
    CoreIPCCFType(CFTypeRef);
    CoreIPCCFType(CoreIPCCFType&&);
    CoreIPCCFType(UniqueRef<CFObjectValue>&&);
    ~CoreIPCCFType();

    const UniqueRef<CFObjectValue>& object() const { return m_object; }
    RetainPtr<id> toID() const;
    RetainPtr<CFTypeRef> toCFType() const;

private:
    UniqueRef<CFObjectValue> m_object;
};

} // namespace WebKit

namespace IPC {

// This ArgumentCoders specialization for UniqueRef<CFObjectValue> is to allow us to use
// makeUniqueRefWithoutFastMallocCheck<>, since we can't make the variant fast malloc'ed
template<> struct ArgumentCoder<UniqueRef<WebKit::CFObjectValue>> {
    template<typename Encoder>
    static void encode(Encoder&, const UniqueRef<WebKit::CFObjectValue>&);
    static std::optional<UniqueRef<WebKit::CFObjectValue>> decode(Decoder&);
};

} // namespace IPC
