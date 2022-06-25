/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>
#include <wtf/RetainPtr.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {
    
class SecItemRequestData {
public:
    enum Type {
        Invalid,
        CopyMatching,
        Add,
        Update,
        Delete,
    };

    SecItemRequestData() = default;
    SecItemRequestData(Type, CFDictionaryRef query);
    SecItemRequestData(Type, CFDictionaryRef query, CFDictionaryRef attributesToMatch);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, SecItemRequestData&);

    Type type() const { return m_type; }

    CFDictionaryRef query() const { return m_queryDictionary.get(); }
    CFDictionaryRef attributesToMatch() const { return m_attributesToMatch.get(); }

private:
    Type m_type { Invalid };
    RetainPtr<CFDictionaryRef> m_queryDictionary;
    RetainPtr<CFDictionaryRef> m_attributesToMatch;
};
    
} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::SecItemRequestData::Type> {
    using values = EnumValues<
        WebKit::SecItemRequestData::Type,
        WebKit::SecItemRequestData::Type::Invalid,
        WebKit::SecItemRequestData::Type::CopyMatching,
        WebKit::SecItemRequestData::Type::Add,
        WebKit::SecItemRequestData::Type::Update,
        WebKit::SecItemRequestData::Type::Delete
    >;
};

} // namespace WTF
