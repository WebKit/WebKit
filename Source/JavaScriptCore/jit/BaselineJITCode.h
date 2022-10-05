/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "CallLinkInfo.h"
#include "JITCode.h"
#include "JITCodeMap.h"
#include "StructureStubInfo.h"
#include <wtf/CompactPointerTuple.h>

#if ENABLE(JIT)

namespace JSC {

class BinaryArithProfile;
class UnaryArithProfile;
struct BaselineUnlinkedStructureStubInfo;
struct SimpleJumpTable;
struct StringJumpTable;

class MathICHolder {
public:
    void adoptMathICs(MathICHolder& other);
    JITAddIC* addJITAddIC(BinaryArithProfile*);
    JITMulIC* addJITMulIC(BinaryArithProfile*);
    JITSubIC* addJITSubIC(BinaryArithProfile*);
    JITNegIC* addJITNegIC(UnaryArithProfile*);

private:
    Bag<JITAddIC> m_addICs;
    Bag<JITMulIC> m_mulICs;
    Bag<JITNegIC> m_negICs;
    Bag<JITSubIC> m_subICs;
};

class JITConstantPool {
    WTF_MAKE_NONCOPYABLE(JITConstantPool);
public:
    using Constant = unsigned;

    enum class Type : uint8_t {
        GlobalObject,
        StructureStubInfo,
        FunctionDecl,
        FunctionExpr,
    };

    using Value = CompactPointerTuple<void*, Type>;

    JITConstantPool() = default;
    JITConstantPool(JITConstantPool&&) = default;
    JITConstantPool& operator=(JITConstantPool&&) = default;

    JITConstantPool(Vector<Value>&& constants)
        : m_constants(WTFMove(constants))
    {
    }

    size_t size() const { return m_constants.size(); }
    Value at(size_t i) const { return m_constants[i]; }

private:
    FixedVector<Value> m_constants;
};


class BaselineJITCode : public DirectJITCode, public MathICHolder {
public:
    BaselineJITCode(CodeRef<JSEntryPtrTag>, CodePtr<JSEntryPtrTag> withArityCheck);
    ~BaselineJITCode() override;
    PCToCodeOriginMap* pcToCodeOriginMap() override { return m_pcToCodeOriginMap.get(); }

    FixedVector<BaselineUnlinkedCallLinkInfo> m_unlinkedCalls;
    FixedVector<BaselineUnlinkedStructureStubInfo> m_unlinkedStubInfos;
    FixedVector<SimpleJumpTable> m_switchJumpTables;
    FixedVector<StringJumpTable> m_stringSwitchJumpTables;
    JITCodeMap m_jitCodeMap;
    JITConstantPool m_constantPool;
    std::unique_ptr<PCToCodeOriginMap> m_pcToCodeOriginMap;
    bool m_isShareable { true };
};

class BaselineJITData final : public TrailingArray<BaselineJITData, void*> {
    WTF_MAKE_FAST_ALLOCATED;
    friend class LLIntOffsetsExtractor;
public:
    using Base = TrailingArray<BaselineJITData, void*>;

    static std::unique_ptr<BaselineJITData> create(unsigned poolSize)
    {
        return std::unique_ptr<BaselineJITData> { new (NotNull, fastMalloc(Base::allocationSize(poolSize))) BaselineJITData(poolSize) };
    }

    explicit BaselineJITData(unsigned size)
        : Base(size)
    {
    }

    FixedVector<StructureStubInfo> m_stubInfos;
};

} // namespace JSC

#endif // ENABLE(JIT)
