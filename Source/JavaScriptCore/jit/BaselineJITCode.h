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
#include "PropertyInlineCache.h"
#include <wtf/ButterflyArray.h>
#include <wtf/CompactPointerTuple.h>

#if ENABLE(JIT)

namespace JSC {

class BinaryArithProfile;
class UnaryArithProfile;
struct BaselineUnlinkedPropertyInlineCache;
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
        FunctionDecl,
        FunctionExpr,
    };

    using Value = JITConstant<Type>;

    JITConstantPool() = default;
    JITConstantPool(JITConstantPool&&) = default;
    JITConstantPool& operator=(JITConstantPool&&) = default;

    JITConstantPool(Vector<Value>&& constants)
        : m_constants(WTF::move(constants))
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
    PCToCodeOriginMap* pcToCodeOriginMap() LIFETIME_BOUND override { return m_pcToCodeOriginMap.get(); }

    CodeLocationLabel<JSInternalPtrTag> getCallLinkDoneLocationForBytecodeIndex(BytecodeIndex) const;

    double livenessRate() const { return m_livenessRate; }
    void setLivenessRate(double rate) { m_livenessRate = rate; }
    double fullnessRate() const { return m_fullnessRate; }
    void setFullnessRate(double rate) { m_fullnessRate = rate; }

    FixedVector<BaselineUnlinkedCallLinkInfo> m_unlinkedCalls;
    FixedVector<BaselineUnlinkedPropertyInlineCache> m_unlinkedPropertyInlineCaches;
    FixedVector<SimpleJumpTable> m_switchJumpTables;
    FixedVector<StringJumpTable> m_stringSwitchJumpTables;
    JITCodeMap m_jitCodeMap;
    JITConstantPool m_constantPool;
    std::unique_ptr<PCToCodeOriginMap> m_pcToCodeOriginMap;
private:
    // The percentage of ValueProfiles that had some profiling data in them.
    double m_livenessRate { 0 };
    // The percentage of ValueProfile buckets that had a value in them.
    double m_fullnessRate { 0 };
public:
    bool m_isShareable { true };
};

class BaselineJITData final : public ButterflyArray<BaselineJITData, PropertyInlineCache, void*> {
    friend class LLIntOffsetsExtractor;
public:
    using Base = ButterflyArray<BaselineJITData, PropertyInlineCache, void*>;

    static std::unique_ptr<BaselineJITData> create(unsigned propertyCacheSize, unsigned poolSize, CodeBlock* codeBlock)
    {
        return std::unique_ptr<BaselineJITData> { createImpl(propertyCacheSize, poolSize, codeBlock) };
    }

    explicit BaselineJITData(unsigned poolSize, unsigned propertyCacheSize, CodeBlock*);

    static constexpr ptrdiff_t offsetOfGlobalObject() { return OBJECT_OFFSETOF(BaselineJITData, m_globalObject); }
    static constexpr ptrdiff_t offsetOfStackOffset() { return OBJECT_OFFSETOF(BaselineJITData, m_stackOffset); }
    static constexpr ptrdiff_t offsetOfJITExecuteCounter() { return OBJECT_OFFSETOF(BaselineJITData, m_executeCounter) + OBJECT_OFFSETOF(BaselineExecutionCounter, m_counter); }
    static constexpr ptrdiff_t offsetOfJITExecutionActiveThreshold() { return OBJECT_OFFSETOF(BaselineJITData, m_executeCounter) + OBJECT_OFFSETOF(BaselineExecutionCounter, m_activeThreshold); }
    static constexpr ptrdiff_t offsetOfJITExecutionTotalCount() { return OBJECT_OFFSETOF(BaselineJITData, m_executeCounter) + OBJECT_OFFSETOF(BaselineExecutionCounter, m_totalCount); }

    PropertyInlineCache& propertyCache(unsigned index)
    {
        auto span = propertyInlineCaches();
        return span[span.size() - index - 1];
    }

    auto propertyInlineCaches() -> decltype(leadingSpan())
    {
        return leadingSpan();
    }

    BaselineExecutionCounter& executeCounter() LIFETIME_BOUND { return m_executeCounter; }
    const BaselineExecutionCounter& executeCounter() const LIFETIME_BOUND { return m_executeCounter; }

    JSGlobalObject* m_globalObject { nullptr }; // This is not marked since owner CodeBlock will mark JSGlobalObject.
    intptr_t m_stackOffset { 0 };
    BaselineExecutionCounter m_executeCounter;
};

} // namespace JSC

#endif // ENABLE(JIT)
