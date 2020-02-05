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

#include "StructureIDTable.h"
#include "VM.h"
#include <wtf/Vector.h>

namespace JSC {

class JSCell;
class JSGlobalObject;
class JSObject;
class PropertySlot;
class Structure;

class PolyProtoAccessChain {
    WTF_MAKE_FAST_ALLOCATED;

public:
    PolyProtoAccessChain(PolyProtoAccessChain&) = default;

    // Returns nullptr when invalid.
    static std::unique_ptr<PolyProtoAccessChain> create(JSGlobalObject*, JSCell* base, const PropertySlot&);
    static std::unique_ptr<PolyProtoAccessChain> create(JSGlobalObject*, JSCell* base, JSObject* target);

    std::unique_ptr<PolyProtoAccessChain> clone()
    {
        return makeUnique<PolyProtoAccessChain>(*this);
    }

    const Vector<StructureID>& chain() const { return m_chain; }

    void dump(Structure* baseStructure, PrintStream& out) const;

    bool operator==(const PolyProtoAccessChain& other) const;
    bool operator!=(const PolyProtoAccessChain& other) const
    {
        return !(*this == other);
    }

    bool needImpurePropertyWatchpoint(VM&) const;

    template <typename Func>
    void forEach(VM& vm, Structure* baseStructure, const Func& func) const
    {
        bool atEnd = !m_chain.size();
        func(baseStructure, atEnd);
        for (unsigned i = 0; i < m_chain.size(); ++i) {
            atEnd = i + 1 == m_chain.size();
            func(vm.getStructure(m_chain[i]), atEnd);
        }
    }

    Structure* slotBaseStructure(VM& vm, Structure* baseStructure) const
    {
        if (m_chain.size())
            return vm.getStructure(m_chain.last());
        return baseStructure;
    }

private:
    PolyProtoAccessChain() = default;

    // This does not include the base. We rely on AccessCase providing it for us. That said, this data
    // structure is tied to the base that it was created with.
    Vector<StructureID> m_chain; 
};

}
