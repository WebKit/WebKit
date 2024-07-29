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

#if PLATFORM(COCOA)

#include "ArgumentCodersCocoa.h"
#include <wtf/RetainPtr.h>
#include <wtf/UniqueRef.h>

namespace WebKit {

class CoreIPCPlistArray;
class CoreIPCPlistDictionary;
class CoreIPCString;
class CoreIPCNumber;
class CoreIPCDate;
class CoreIPCData;

using PlistValue = std::variant<
    CoreIPCPlistArray,
    CoreIPCPlistDictionary,
    CoreIPCString,
    CoreIPCNumber,
    CoreIPCDate,
    CoreIPCData
>;

class CoreIPCPlistObject {
public:
    CoreIPCPlistObject(id);
    CoreIPCPlistObject(UniqueRef<PlistValue>&&);

    RetainPtr<id> toID() const;
    static bool isPlistType(id);

    const UniqueRef<PlistValue>& value() const { return m_value; }
private:
    UniqueRef<PlistValue> m_value;
};

} // namespace WebKit

namespace IPC {

// This ArgumentCoders specialization for UniqueRef<PlistValue> is to allow us to use
// makeUniqueRefWithoutFastMallocCheck<>, since we can't make the variant fast malloc'ed
template<> struct ArgumentCoder<UniqueRef<WebKit::PlistValue>> {
    static void encode(Encoder&, const UniqueRef<WebKit::PlistValue>&);
    static std::optional<UniqueRef<WebKit::PlistValue>> decode(Decoder&);
};

} // namespace IPC

#endif // PLATFORM(COCOA)
