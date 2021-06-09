/*
 * Copyright (C) 2008-2018 Apple Inc. All rights reserved.
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

#include "ArityCheckMode.h"
#include "CallFrame.h"
#include "CodeOrigin.h"
#include "JSCJSValue.h"
#include "MacroAssemblerCodeRef.h"
#include "RegisterSet.h"

namespace JSC {

namespace DFG {
class CommonData;
class JITCode;
}
namespace FTL {
class ForOSREntryJITCode;
class JITCode;
}
namespace DOMJIT {
class Signature;
}

struct ProtoCallFrame;
class TrackedReferences;
class VM;

enum class JITType : uint8_t {
    None = 0b000,
    HostCallThunk = 0b001,
    InterpreterThunk = 0b010,
    BaselineJIT = 0b011,
    DFGJIT = 0b100,
    FTLJIT = 0b101,
};
static constexpr unsigned widthOfJITType = 3;
static_assert(WTF::getMSBSetConstexpr(static_cast<std::underlying_type_t<JITType>>(JITType::FTLJIT)) + 1 == widthOfJITType);

class JITCode : public ThreadSafeRefCounted<JITCode> {
public:
    template<PtrTag tag> using CodePtr = MacroAssemblerCodePtr<tag>;
    template<PtrTag tag> using CodeRef = MacroAssemblerCodeRef<tag>;

    static const char* typeName(JITType);

    static JITType bottomTierJIT()
    {
        return JITType::BaselineJIT;
    }
    
    static JITType topTierJIT()
    {
        return JITType::FTLJIT;
    }
    
    static JITType nextTierJIT(JITType jitType)
    {
        switch (jitType) {
        case JITType::BaselineJIT:
            return JITType::DFGJIT;
        case JITType::DFGJIT:
            return JITType::FTLJIT;
        default:
            RELEASE_ASSERT_NOT_REACHED();
            return JITType::None;
        }
    }
    
    static bool isExecutableScript(JITType jitType)
    {
        switch (jitType) {
        case JITType::None:
        case JITType::HostCallThunk:
            return false;
        default:
            return true;
        }
    }
    
    static bool couldBeInterpreted(JITType jitType)
    {
        switch (jitType) {
        case JITType::InterpreterThunk:
        case JITType::BaselineJIT:
            return true;
        default:
            return false;
        }
    }
    
    static bool isJIT(JITType jitType)
    {
        switch (jitType) {
        case JITType::BaselineJIT:
        case JITType::DFGJIT:
        case JITType::FTLJIT:
            return true;
        default:
            return false;
        }
    }

    static bool isLowerTier(JITType expectedLower, JITType expectedHigher)
    {
        RELEASE_ASSERT(isExecutableScript(expectedLower));
        RELEASE_ASSERT(isExecutableScript(expectedHigher));
        return expectedLower < expectedHigher;
    }
    
    static bool isHigherTier(JITType expectedHigher, JITType expectedLower)
    {
        return isLowerTier(expectedLower, expectedHigher);
    }
    
    static bool isLowerOrSameTier(JITType expectedLower, JITType expectedHigher)
    {
        return !isHigherTier(expectedLower, expectedHigher);
    }
    
    static bool isHigherOrSameTier(JITType expectedHigher, JITType expectedLower)
    {
        return isLowerOrSameTier(expectedLower, expectedHigher);
    }
    
    static bool isOptimizingJIT(JITType jitType)
    {
        return jitType == JITType::DFGJIT || jitType == JITType::FTLJIT;
    }
    
    static bool isBaselineCode(JITType jitType)
    {
        return jitType == JITType::InterpreterThunk || jitType == JITType::BaselineJIT;
    }

    static bool useDataIC(JITType jitType)
    {
        if (!Options::useDataIC())
            return false;
        if (JITCode::isBaselineCode(jitType))
            return true;
        return Options::useDataICInOptimizingJIT();
    }

    virtual const DOMJIT::Signature* signature() const { return nullptr; }
    
    enum class ShareAttribute : uint8_t {
        NotShared,
        Shared
    };

protected:
    JITCode(JITType, JITCode::ShareAttribute = JITCode::ShareAttribute::NotShared);
    
public:
    virtual ~JITCode();
    
    JITType jitType() const
    {
        return m_jitType;
    }
    
    template<typename PointerType>
    static JITType jitTypeFor(PointerType jitCode)
    {
        if (!jitCode)
            return JITType::None;
        return jitCode->jitType();
    }
    
    virtual CodePtr<JSEntryPtrTag> addressForCall(ArityCheckMode) = 0;
    virtual void* executableAddressAtOffset(size_t offset) = 0;
    void* executableAddress() { return executableAddressAtOffset(0); }
    virtual void* dataAddressAtOffset(size_t offset) = 0;
    virtual unsigned offsetOf(void* pointerIntoCode) = 0;
    
    virtual DFG::CommonData* dfgCommon();
    virtual DFG::JITCode* dfg();
    virtual FTL::JITCode* ftl();
    virtual FTL::ForOSREntryJITCode* ftlForOSREntry();
    virtual void shrinkToFit(const ConcurrentJSLocker&);
    
    virtual void validateReferences(const TrackedReferences&);
    
    JSValue execute(VM*, ProtoCallFrame*);
    
    void* start() { return dataAddressAtOffset(0); }
    virtual size_t size() = 0;
    void* end() { return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(start()) + size()); }
    
    virtual bool contains(void*) = 0;

#if ENABLE(JIT)
    virtual RegisterSet liveRegistersToPreserveAtExceptionHandlingCallSite(CodeBlock*, CallSiteIndex);
    virtual std::optional<CodeOrigin> findPC(CodeBlock*, void* pc) { UNUSED_PARAM(pc); return std::nullopt; }
#endif

    Intrinsic intrinsic() { return m_intrinsic; }

    bool isShared() const { return m_shareAttribute == ShareAttribute::Shared; }

private:
    JITType m_jitType;
    ShareAttribute m_shareAttribute;
protected:
    Intrinsic m_intrinsic { NoIntrinsic }; // Effective only in NativeExecutable.
};

class JITCodeWithCodeRef : public JITCode {
protected:
    JITCodeWithCodeRef(JITType);
    JITCodeWithCodeRef(CodeRef<JSEntryPtrTag>, JITType, JITCode::ShareAttribute);

public:
    ~JITCodeWithCodeRef() override;

    void* executableAddressAtOffset(size_t offset) override;
    void* dataAddressAtOffset(size_t offset) override;
    unsigned offsetOf(void* pointerIntoCode) override;
    size_t size() override;
    bool contains(void*) override;

protected:
    CodeRef<JSEntryPtrTag> m_ref;
};

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(DirectJITCode);
class DirectJITCode : public JITCodeWithCodeRef {
    WTF_MAKE_STRUCT_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(DirectJITCode);
public:
    DirectJITCode(JITType);
    DirectJITCode(CodeRef<JSEntryPtrTag>, CodePtr<JSEntryPtrTag> withArityCheck, JITType, JITCode::ShareAttribute = JITCode::ShareAttribute::NotShared);
    DirectJITCode(CodeRef<JSEntryPtrTag>, CodePtr<JSEntryPtrTag> withArityCheck, JITType, Intrinsic, JITCode::ShareAttribute = JITCode::ShareAttribute::NotShared); // For generated thunk.
    ~DirectJITCode() override;
    
    CodePtr<JSEntryPtrTag> addressForCall(ArityCheckMode) override;

protected:
    void initializeCodeRefForDFG(CodeRef<JSEntryPtrTag>, CodePtr<JSEntryPtrTag> withArityCheck);

private:
    CodePtr<JSEntryPtrTag> m_withArityCheck;
};

class NativeJITCode : public JITCodeWithCodeRef {
public:
    NativeJITCode(JITType);
    NativeJITCode(CodeRef<JSEntryPtrTag>, JITType, Intrinsic, JITCode::ShareAttribute = JITCode::ShareAttribute::NotShared);
    ~NativeJITCode() override;

    CodePtr<JSEntryPtrTag> addressForCall(ArityCheckMode) override;
};

class NativeDOMJITCode final : public NativeJITCode {
public:
    NativeDOMJITCode(CodeRef<JSEntryPtrTag>, JITType, Intrinsic, const DOMJIT::Signature*);
    ~NativeDOMJITCode() final = default;

    const DOMJIT::Signature* signature() const final { return m_signature; }

private:
    const DOMJIT::Signature* m_signature;
};

} // namespace JSC

namespace WTF {

class PrintStream;
void printInternal(PrintStream&, JSC::JITType);

} // namespace WTF
