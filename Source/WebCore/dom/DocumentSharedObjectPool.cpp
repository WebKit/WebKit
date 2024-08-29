/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "config.h"
#include "DocumentSharedObjectPool.h"

#include "Element.h"
#include "ElementData.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UnalignedAccess.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DocumentSharedObjectPool);

// Do comparisons 8 bytes-at-a-time on architectures where it's safe.
#if (CPU(X86_64) || CPU(ARM64)) && !ASAN_ENABLED
ALWAYS_INLINE bool equalAttributes(const uint8_t* a, const uint8_t* b, unsigned bytes)
{
    unsigned length = bytes >> 3;
    for (unsigned i = 0; i != length; ++i) {
        if (WTF::unalignedLoad<uint64_t>(a) != WTF::unalignedLoad<uint64_t>(b))
            return false;

        a += sizeof(uint64_t);
        b += sizeof(uint64_t);
    }

    ASSERT(!(bytes & 4));
    ASSERT(!(bytes & 2));
    ASSERT(!(bytes & 1));

    return true;
}
#else
ALWAYS_INLINE bool equalAttributes(const uint8_t* a, const uint8_t* b, unsigned bytes)
{
    return !memcmp(a, b, bytes);
}
#endif

struct DocumentSharedObjectPool::ShareableElementDataHash {
    static unsigned hash(const Ref<ShareableElementData>& data)
    {
        return computeHash(std::span<const Attribute> { data->m_attributeArray, data->length() });
    }
    static bool equal(const Ref<ShareableElementData>& a, const Ref<ShareableElementData>& b)
    {
        if (a->length() != b->length())
            return false;
        return equalAttributes(reinterpret_cast<const uint8_t*>(a->m_attributeArray), reinterpret_cast<const uint8_t*>(b->m_attributeArray), a->length() * sizeof(Attribute));
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

struct AttributeSpanTranslator {
    static unsigned hash(std::span<const Attribute> attributes)
    {
        return computeHash(attributes);
    }

    static bool equal(const Ref<ShareableElementData>& a, std::span<const Attribute> b)
    {
        if (a->length() != b.size())
            return false;
        return equalAttributes(reinterpret_cast<const uint8_t*>(a->m_attributeArray), reinterpret_cast<const uint8_t*>(b.data()), b.size() * sizeof(Attribute));
    }

    static void translate(Ref<ShareableElementData>& location, std::span<const Attribute> attributes, unsigned /*hash*/)
    {
        location = ShareableElementData::createWithAttributes(attributes);
    }
};

Ref<ShareableElementData> DocumentSharedObjectPool::cachedShareableElementDataWithAttributes(std::span<const Attribute> attributes)
{
    ASSERT(!attributes.empty());

    return m_shareableElementDataCache.add<AttributeSpanTranslator>(attributes).iterator->get();
}

} // namespace WebCore
