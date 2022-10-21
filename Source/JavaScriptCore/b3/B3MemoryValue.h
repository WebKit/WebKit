/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if ENABLE(B3_JIT)

#include "B3Bank.h"
#include "B3HeapRange.h"
#include "B3Value.h"
#include <type_traits>

namespace JSC { namespace B3 {

class JS_EXPORT_PRIVATE MemoryValue : public Value {
public:
    static bool accepts(Kind kind)
    {
        return isMemoryAccess(kind.opcode());
    }

    ~MemoryValue() override;

    OffsetType offset() const { return m_offset; }
    template<typename Int, typename = IsLegalOffset<Int>>
    void setOffset(Int offset) { m_offset = offset; }

    // You don't have to worry about using legal offsets unless you've entered quirks mode.
    template<typename Int,
        typename = typename std::enable_if<std::is_integral<Int>::value>::type,
        typename = typename std::enable_if<std::is_signed<Int>::value>::type
    >
    bool isLegalOffset(Int offset) const { return isLegalOffsetImpl(offset); }

    // A necessary consequence of MemoryValue having an offset is that it participates in instruction
    // selection. This tells you if this will get lowered to something that requires an offsetless
    // address.
    bool requiresSimpleAddr() const;

    const HeapRange& range() const { return m_range; }
    void setRange(const HeapRange& range) { m_range = range; }
    
    // This is an alias for range.
    const HeapRange& accessRange() const { return range(); }
    void setAccessRange(const HeapRange& range) { setRange(range); }
    
    const HeapRange& fenceRange() const { return m_fenceRange; }
    void setFenceRange(const HeapRange& range) { m_fenceRange = range; }

    bool isStore() const { return B3::isStore(opcode()); }
    bool isLoad() const { return B3::isLoad(opcode()); }

    bool hasFence() const { return !!fenceRange(); }
    bool isExotic() const { return hasFence() || isAtom(opcode()); }

    Type accessType() const;
    Bank accessBank() const;
    size_t accessByteSize() const;
    
    Width accessWidth() const;

    bool isCanonicalWidth() const { return JSC::isCanonicalWidth(accessWidth()); }

    B3_SPECIALIZE_VALUE_FOR_NON_VARARGS_CHILDREN

protected:
    void dumpMeta(CommaPrinter&, PrintStream&) const override;
    
    template<typename Int, typename = IsLegalOffset<Int>, typename... Arguments>
    MemoryValue(CheckedOpcodeTag, Kind kind, Type type, NumChildren numChildren, Origin origin, Int offset, HeapRange range, HeapRange fenceRange, Arguments... arguments)
        : Value(CheckedOpcode, kind, type, numChildren, origin, static_cast<Value*>(arguments)...)
        , m_offset(offset)
        , m_range(range)
        , m_fenceRange(fenceRange)
    {
    }
    
private:
    friend class Procedure;
    friend class Value;

    bool isLegalOffsetImpl(int32_t offset) const;
    bool isLegalOffsetImpl(int64_t offset) const;

    enum MemoryValueLoad { MemoryValueLoadTag };
    enum MemoryValueLoadImplied { MemoryValueLoadImpliedTag };
    enum MemoryValueStore { MemoryValueStoreTag };

    // Use this form for Load (but not Load8Z, Load8S, or any of the Loads that have a suffix that
    // describes the returned type).
    MemoryValue(Kind kind, Type type, Origin origin, Value* pointer)
        : MemoryValue(MemoryValueLoadTag, kind, type, origin, pointer)
    {
    }
    template<typename Int, typename = IsLegalOffset<Int>>
    MemoryValue(Kind kind, Type type, Origin origin, Value* pointer, Int offset, HeapRange range = HeapRange::top(), HeapRange fenceRange = HeapRange())
        : MemoryValue(MemoryValueLoadTag, kind, type, origin, pointer, offset, range, fenceRange)
    {
    }

    // Use this form for loads where the return type is implied.
    MemoryValue(Kind kind, Origin origin, Value* pointer)
        : MemoryValue(MemoryValueLoadImpliedTag, kind, origin, pointer)
    {
    }
    template<typename Int, typename = IsLegalOffset<Int>>
    MemoryValue(Kind kind, Origin origin, Value* pointer, Int offset, HeapRange range = HeapRange::top(), HeapRange fenceRange = HeapRange())
        : MemoryValue(MemoryValueLoadImpliedTag, kind, origin, pointer, offset, range, fenceRange)
    {
    }

    // Use this form for stores.
    MemoryValue(Kind kind, Origin origin, Value* value, Value* pointer)
        : MemoryValue(MemoryValueStoreTag, kind, origin, value, pointer)
    {
    }
    template<typename Int, typename = IsLegalOffset<Int>>
    MemoryValue(Kind kind, Origin origin, Value* value, Value* pointer, Int offset, HeapRange range = HeapRange::top(), HeapRange fenceRange = HeapRange())
        : MemoryValue(MemoryValueStoreTag, kind, origin, value, pointer, offset, range, fenceRange)
    {
    }

    // The above templates forward to these implementations.
    MemoryValue(MemoryValueLoad, Kind, Type, Origin, Value* pointer, OffsetType = 0, HeapRange = HeapRange::top(), HeapRange fenceRange = HeapRange());
    MemoryValue(MemoryValueLoadImplied, Kind, Origin, Value* pointer, OffsetType = 0, HeapRange = HeapRange::top(), HeapRange fenceRange = HeapRange());
    MemoryValue(MemoryValueStore, Kind, Origin, Value*, Value* pointer, OffsetType = 0, HeapRange = HeapRange::top(), HeapRange fenceRange = HeapRange());

    OffsetType m_offset { 0 };
    HeapRange m_range { HeapRange::top() };
    HeapRange m_fenceRange { HeapRange() };
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
