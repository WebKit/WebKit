/*
 * Copyright (C) 2017-2024 Apple Inc. All rights reserved.
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
 */

#pragma once

#if ENABLE(WEBASSEMBLY)

#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/Vector.h>

namespace JSC {
namespace Wasm {

struct ModuleDebugInfo {
    WTF_MAKE_TZONE_ALLOCATED(ModuleDebugInfo);
public:
    // FIXME: Here only functionIndex is used in practice. Let's leave metadataOffset and metadataSize as they are for validation purposes.
    struct Data {
        size_t functionIndex;
        size_t metadataOffset;
        size_t metadataSize;
    
        Data() = default;
        Data(size_t functionIndex, size_t metadataOffset, size_t metadataSize)
            : functionIndex(functionIndex)
            , metadataOffset(metadataOffset)
            , metadataSize(metadataSize)
        {
        }
    };
    
    const Data* data(uint32_t offset) const
    {
        auto it = m_offsetToData.find(offset);
        if (it == m_offsetToData.end())
            return nullptr;
        return &it->value;
    }
    
    void addData(uint32_t offset, Data&& info)
    {
        Locker locker { m_lock };
        m_offsetToData.add(offset, WTFMove(info));
    }

    void takeSource(Vector<uint8_t>&& source) { m_source = WTFMove(source); }
    Vector<uint8_t>& source() { return m_source; }

    void setId(uint32_t id) { m_id = id; }
    uint32_t id() { return m_id; }

private:
    uint32_t m_id { 0 };
    Lock m_lock;
    UncheckedKeyHashMap<uint32_t, Data, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_offsetToData;
    Vector<uint8_t> m_source;
};

} // namespace Wasm
} // namespace JSC

#endif // ENABLE(WEBASSEMBLY)
