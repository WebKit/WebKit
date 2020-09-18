/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(WHLSL_COMPILER)

#include "WHLSLUnnamedType.h"
#include <wtf/HashTraits.h>

namespace WebCore {

namespace WHLSL {

class UnnamedTypeKey {
public:
    UnnamedTypeKey() = default;
    UnnamedTypeKey(WTF::HashTableDeletedValueType)
        : m_type(bitwise_cast<AST::UnnamedType*>(static_cast<uintptr_t>(1)))
    {
    }

    UnnamedTypeKey(AST::UnnamedType& type)
        : m_type(&type)
    {
    }

    bool isEmptyValue() const { return !m_type; }
    bool isHashTableDeletedValue() const { return m_type == bitwise_cast<AST::UnnamedType*>(static_cast<uintptr_t>(1)); }

    unsigned hash() const { return m_type->hash(); }
    bool operator==(const UnnamedTypeKey& other) const { return *m_type == *other.m_type; }
    AST::UnnamedType& unnamedType() const { return *m_type; }

    struct Hash {
        static unsigned hash(const UnnamedTypeKey& key) { return key.hash(); }
        static bool equal(const UnnamedTypeKey& a, const UnnamedTypeKey& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = false;
        static const bool emptyValueIsZero = true;
    };

    struct Traits : public WTF::SimpleClassHashTraits<UnnamedTypeKey> {
        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const UnnamedTypeKey& key) { return key.isEmptyValue(); }
    };

private:
    AST::UnnamedType* m_type { nullptr };
};

}

}

namespace WTF {

template<> struct HashTraits<WebCore::WHLSL::UnnamedTypeKey> : WebCore::WHLSL::UnnamedTypeKey::Traits { };
template<> struct DefaultHash<WebCore::WHLSL::UnnamedTypeKey> : WebCore::WHLSL::UnnamedTypeKey::Hash { };

} // namespace WTF

#endif // ENABLE(WHLSL_COMPILER)
