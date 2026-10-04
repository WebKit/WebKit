/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#include <wtf/Platform.h>

#if ENABLE(WEBASSEMBLY)

#include <JavaScriptCore/BytecodeConventions.h>
#include <JavaScriptCore/HandlerInfo.h>
#include <JavaScriptCore/InstructionStream.h>
#include <JavaScriptCore/MacroAssemblerCodeRef.h>
#include <JavaScriptCore/SIMDInfo.h>
#include <JavaScriptCore/WasmHandlerInfo.h>
#include <JavaScriptCore/WasmIPIntGenerator.h>
#include <JavaScriptCore/WasmIPIntTierUpCounter.h>
#include <array>
#include <limits>
#include <wtf/HashMap.h>
#include <wtf/RefCountedFixedVector.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class JITCode;

template <typename Traits>
class BytecodeGeneratorBase;

namespace Wasm {

class IPIntCallee;
struct IPIntGeneratorTraits;
struct JumpTableEntry;

#define WRITE_TO_METADATA(dst, src, type) \
    do { \
        type tmp = (src); \
        memcpy((dst), &tmp, sizeof(type)); \
    } while (false)

class FunctionIPIntMetadataGenerator {
    WTF_MAKE_TZONE_ALLOCATED(FunctionIPIntMetadataGenerator);
    WTF_MAKE_NONCOPYABLE(FunctionIPIntMetadataGenerator);

    friend class IPIntGenerator;
    friend class IPIntCallee;

public:
    FunctionIPIntMetadataGenerator(FunctionCodeIndex functionIndex, std::span<const uint8_t> bytecode)
        : m_functionIndex(functionIndex)
        , m_bytecode(bytecode)
    {
    }

    FunctionCodeIndex functionIndex() const { return m_functionIndex; }

    const uint8_t* getBytecode() const LIFETIME_BOUND { return m_bytecode.data(); }
    const uint8_t* getMetadata() const LIFETIME_BOUND { return m_metadata.span().data(); }

    UncheckedKeyHashMap<IPIntPC, IPIntTierUpCounter::OSREntryData>& tierUpCounter() LIFETIME_BOUND { return m_tierUpCounter; }

    void addCallTarget(unsigned callProfileIndex, FunctionSpaceIndex target)
    {
        if (callProfileIndex >= m_callTargets.size())
            m_callTargets.insertFill(m_callTargets.size(), FunctionSpaceIndex { }, callProfileIndex - m_callTargets.size() + 1);
        m_callTargets[callProfileIndex] = target;
    }

    static constexpr unsigned numTrackedHotLocals = 6;
    using HotLocals = std::array<uint32_t, numTrackedHotLocals>;
    static constexpr uint32_t noHotLocal = std::numeric_limits<uint32_t>::max();

    static constexpr uint32_t minimumHotLocalUses = 4;

    // A read of a local that has a register is free, but an assignment to one still stores, since
    // everything that unwinds or reconstructs a frame reads locals from their slots. So a register is
    // worth the reads it saves less the stores it costs.
    void recordLocalRead(uint32_t index) { adjustLocalScore(index, 1); }
    void recordLocalWrite(uint32_t index) { adjustLocalScore(index, -1); }

    void enterLoop() { ++m_loopDepth; }
    void exitLoop()
    {
        ASSERT(m_loopDepth);
        --m_loopDepth;
    }
    unsigned loopDepth() const { return m_loopDepth; }

    void adjustLocalScore(uint32_t index, int32_t delta)
    {
        if (!m_loopDepth)
            return;
        if (index >= m_localUseCounts.size())
            m_localUseCounts.insertFill(m_localUseCounts.size(), 0, index + 1 - m_localUseCounts.size());
        m_localUseCounts[index] += delta;
    }

    HotLocals hotLocals() const;

private:
    struct MetadataBufferMalloc final : public FastMalloc {
        static constexpr ALWAYS_INLINE size_t nextCapacity(size_t capacity) { return capacity + capacity; }
    };
    using MetadataBuffer = Vector<uint8_t, 0, UnsafeVectorOverflow, 16, MetadataBufferMalloc>;

    inline void addBlankSpace(size_t);
    template <typename T> inline void addBlankSpace() { addBlankSpace(sizeof(T)); };

    template <typename T> inline void appendMetadata(T t)
    {
        auto size = m_metadata.size();
        addBlankSpace<T>();
        WRITE_TO_METADATA(m_metadata.mutableSpan().data() + size, t, T);
    };

    void addLength(size_t length);
    void addMemorySize(uint8_t memoryIndex, size_t length);
    void addMemoryGrow(uint8_t memoryIndex, size_t length);
    void addTableAccess(uint32_t index, size_t length);
    void addRefFunc(uint32_t index, size_t length);
    void addElemDrop(uint32_t index, size_t length);
    void addDataAccess(uint32_t index, size_t length);
    void addMemoryInit(uint8_t memoryIndex, uint32_t dataIndex, size_t length);
    void addMemoryFill(uint8_t memoryIndex, size_t length);
    void addMemoryCopy(uint8_t dstMemoryIndex, uint8_t srcMemoryIndex, size_t length);
    void addAtomicMemoryAccess(uint8_t memoryIndex, uint64_t offset, size_t length);

    FunctionCodeIndex m_functionIndex;

    std::span<const uint8_t> m_bytecode;
    MetadataBuffer m_metadata { };

    uint32_t m_bytecodeOffset { 0 };
    unsigned m_maxFrameSizeInV128 { 0 };
    unsigned m_maxCalleeStackSize { 0 };
    unsigned m_numLocals { 0 };
    unsigned m_numAlignedRethrowSlots { 0 };
    unsigned m_numArguments { 0 };
    unsigned m_numArgumentsOnStack { 0 };
    unsigned m_nonArgLocalOffset { 0 };
    Vector<FunctionSpaceIndex> m_callTargets { };
    Vector<uint8_t, 8> m_localInitBytecode { };
    Vector<int32_t, 8> m_localUseCounts { };
    unsigned m_loopDepth { 0 };

    UncheckedKeyHashMap<IPIntPC, IPIntTierUpCounter::OSREntryData> m_tierUpCounter;
    Vector<UnlinkedHandlerInfo> m_exceptionHandlers;
};

void FunctionIPIntMetadataGenerator::addBlankSpace(size_t size)
{
    m_metadata.grow(m_metadata.size() + size);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
