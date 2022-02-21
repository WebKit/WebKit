/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "AssemblyHelpers.h"
#include "CodeOrigin.h"
#include "JITOperationValidation.h"
#include "JITOperations.h"
#include "JSCJSValue.h"
#include "PutKind.h"
#include "RegisterSet.h"
#include <wtf/Bag.h>

namespace JSC {

class CacheableIdentifier;
class CallSiteIndex;
class CodeBlock;
class JIT;
class StructureStubInfo;
struct UnlinkedStructureStubInfo;

enum class AccessType : int8_t;
enum class JITType : uint8_t;

namespace BaselineDelByValRegisters {
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT0 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT3 };
constexpr GPRReg scratchGPR { GPRInfo::regT2 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
constexpr GPRReg scratchGPR { GPRInfo::regT6 };
#endif
}

namespace BaselineDelByIdRegisters {
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT3 };
constexpr GPRReg scratchGPR { GPRInfo::regT2 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
constexpr GPRReg scratchGPR { GPRInfo::regT6 };
#endif
}

namespace BaselineGetByValRegisters {
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT1 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT2 };
constexpr GPRReg scratchGPR { GPRInfo::regT3 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
constexpr GPRReg scratchGPR { GPRInfo::regT6 };
#endif
}

#if USE(JSVALUE64)
namespace BaselineEnumeratorGetByValRegisters {
static constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
static constexpr JSValueRegs propertyJSR { GPRInfo::regT1 };
static constexpr JSValueRegs resultJSR { GPRInfo::regT0 };
static constexpr GPRReg stubInfoGPR = GPRInfo::regT2;
// We rely on this when linking a CodeBlock and initializing registers for a GetByVal StubInfo.
static_assert(baseJSR == BaselineGetByValRegisters::baseJSR);
static_assert(propertyJSR == BaselineGetByValRegisters::propertyJSR);
static_assert(resultJSR == BaselineGetByValRegisters::resultJSR);
static_assert(stubInfoGPR == BaselineGetByValRegisters::stubInfoGPR);

static constexpr GPRReg scratch1 = GPRInfo::regT3;
static constexpr GPRReg scratch2 = GPRInfo::regT4;
static constexpr GPRReg scratch3 = GPRInfo::regT5;
}
#endif

namespace BaselineInstanceofRegisters {
#if USE(JSVALUE64)
constexpr JSValueRegs resultJSR { GPRInfo::regT0 };
constexpr JSValueRegs valueJSR { GPRInfo::argumentGPR2 };
constexpr JSValueRegs protoJSR { GPRInfo::argumentGPR3 };
constexpr GPRReg stubInfoGPR { GPRInfo::argumentGPR1 };
constexpr GPRReg scratch1GPR { GPRInfo::nonArgGPR0 };
constexpr GPRReg scratch2GPR { GPRInfo::nonArgGPR1 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs resultJSR { JSRInfo::jsRegT10 };
#if CPU(MIPS)
constexpr JSValueRegs valueJSR { GPRInfo::argumentGPR3, GPRInfo::argumentGPR2 };
#else
constexpr JSValueRegs valueJSR { JSRInfo::jsRegT32 };
#endif
constexpr JSValueRegs protoJSR { JSRInfo::jsRegT54 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT1 };
constexpr GPRReg scratch1GPR { GPRInfo::regT6 };
constexpr GPRReg scratch2GPR { GPRInfo::regT7 };
#endif
static_assert(AssemblyHelpers::noOverlap(valueJSR, protoJSR, stubInfoGPR, scratch1GPR, scratch2GPR));
}

namespace BaselineInByValRegisters {
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT1 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT2 };
constexpr GPRReg scratchGPR { GPRInfo::regT3 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr JSValueRegs propertyJSR  { GPRInfo::regT3, GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
constexpr GPRReg scratchGPR { GPRInfo::regT6 };
#endif
static_assert(baseJSR == BaselineGetByValRegisters::baseJSR);
static_assert(propertyJSR == BaselineGetByValRegisters::propertyJSR);
}

namespace BaselineGetByIdRegisters {
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
constexpr JSValueRegs baseJSR { JSRInfo::returnValueJSR };
#if USE(JSVALUE64)
constexpr GPRReg stubInfoGPR { GPRInfo::regT1 };
constexpr GPRReg scratchGPR { GPRInfo::regT2 };
constexpr JSValueRegs dontClobberJSR { GPRInfo::regT3 };
#elif USE(JSVALUE32_64)
constexpr GPRReg stubInfoGPR { GPRInfo::regT2 };
constexpr GPRReg scratchGPR { GPRInfo::regT3 };
constexpr JSValueRegs dontClobberJSR { GPRInfo::regT6, GPRInfo::regT7 };
#endif
static_assert(AssemblyHelpers::noOverlap(baseJSR, stubInfoGPR, scratchGPR, dontClobberJSR));
}

namespace BaselineGetByIdWithThisRegisters {
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
constexpr JSValueRegs thisJSR { GPRInfo::regT1 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT2 };
constexpr GPRReg scratchGPR { GPRInfo::regT3 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr JSValueRegs thisJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
constexpr GPRReg scratchGPR { GPRInfo::regT6 };
#endif
}

namespace BaselineInByIdRegisters {
constexpr JSValueRegs baseJSR { BaselineGetByIdRegisters::baseJSR };
constexpr JSValueRegs resultJSR { JSRInfo::returnValueJSR };
constexpr GPRReg stubInfoGPR { BaselineGetByIdRegisters::stubInfoGPR };
constexpr GPRReg scratchGPR { BaselineGetByIdRegisters::scratchGPR };
}

namespace BaselinePutByIdRegisters {
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
constexpr JSValueRegs valueJSR { GPRInfo::regT1 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT3 };
constexpr GPRReg scratchGPR { GPRInfo::regT2 };
constexpr GPRReg scratch2GPR { GPRInfo::regT4 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr JSValueRegs valueJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
constexpr GPRReg scratchGPR { GPRInfo::regT6 };
constexpr GPRReg scratch2GPR { baseJSR.tagGPR() }; // Reusing regT1 for better code size on ARM_THUMB2
#endif
}

namespace BaselinePutByValRegisters {
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT1 };
constexpr JSValueRegs valueJSR { GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
constexpr GPRReg profileGPR { GPRInfo::regT3 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr JSValueRegs propertyJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr JSValueRegs valueJSR { GPRInfo::regT6, GPRInfo::regT7 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT4 };
constexpr GPRReg profileGPR { GPRInfo::regT5 };
#endif
}

namespace BaselinePrivateBrandRegisters {
#if USE(JSVALUE64)
constexpr JSValueRegs baseJSR { GPRInfo::regT0 };
constexpr JSValueRegs brandJSR { GPRInfo::regT1 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT2 };
#elif USE(JSVALUE32_64)
constexpr JSValueRegs baseJSR { GPRInfo::regT1, GPRInfo::regT0 };
constexpr JSValueRegs brandJSR { GPRInfo::regT3, GPRInfo::regT2 };
constexpr GPRReg stubInfoGPR { GPRInfo::regT7 };
#endif
static_assert(baseJSR == BaselineGetByValRegisters::baseJSR);
static_assert(brandJSR == BaselineGetByValRegisters::propertyJSR);
}

class JITInlineCacheGenerator {
protected:
    JITInlineCacheGenerator() { }
    JITInlineCacheGenerator(CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters);
    
public:
    StructureStubInfo* stubInfo() const { return m_stubInfo; }

    void reportSlowPathCall(MacroAssembler::Label slowPathBegin, MacroAssembler::Call call)
    {
        m_slowPathBegin = slowPathBegin;
        m_slowPathCall = call;
    }
    
    MacroAssembler::Label slowPathBegin() const { return m_slowPathBegin; }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer,
        CodeLocationLabel<JITStubRoutinePtrTag> start);

    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);

    UnlinkedStructureStubInfo* m_unlinkedStubInfo { nullptr };
    unsigned m_unlinkedStubInfoConstantIndex { std::numeric_limits<unsigned>::max() };

protected:
    JITType m_jitType;
    StructureStubInfo* m_stubInfo { nullptr };

public:
    MacroAssembler::Label m_start;
    MacroAssembler::Label m_done;
    MacroAssembler::Label m_slowPathBegin;
    MacroAssembler::Call m_slowPathCall;
};

class JITByIdGenerator : public JITInlineCacheGenerator {
protected:
    JITByIdGenerator() { }

    JITByIdGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR);

public:
    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.isSet());
        return m_slowPathJump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);
    
protected:
    
    void generateFastCommon(MacroAssembler&, size_t size);
    
    JSValueRegs m_base;
    JSValueRegs m_value;

public:
    MacroAssembler::Jump m_slowPathJump;
};

class JITGetByIdGenerator final : public JITByIdGenerator {
public:
    JITGetByIdGenerator() { }

    JITGetByIdGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR, AccessType);
    
    void generateFastPath(MacroAssembler&);
    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);

private:
    bool m_isLengthAccess;
};

class JITGetByIdWithThisGenerator final : public JITByIdGenerator {
public:
    JITGetByIdWithThisGenerator() { }

    JITGetByIdWithThisGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, GPRReg stubInfoGPR);

    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);
    void generateFastPath(MacroAssembler&);
};

class JITPutByIdGenerator final : public JITByIdGenerator {
public:
    JITPutByIdGenerator() = default;

    JITPutByIdGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR, GPRReg scratch, ECMAMode, PutKind);
    
    void generateFastPath(MacroAssembler&);
    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);
    
    V_JITOperation_GSsiJJC slowPathFunction();

private:
    ECMAMode m_ecmaMode { ECMAMode::strict() };
    PutKind m_putKind;
};

class JITPutByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITPutByValGenerator() = default;

    JITPutByValGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg arrayProfileGPR, GPRReg stubInfoGPR);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(MacroAssembler&);

    JSValueRegs m_base;
    JSValueRegs m_value;

    MacroAssembler::PatchableJump m_slowPathJump;
};

class JITDelByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITDelByValGenerator() { }

    JITDelByValGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratch);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(MacroAssembler&);

    MacroAssembler::PatchableJump m_slowPathJump;
};

class JITDelByIdGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITDelByIdGenerator() { }

    JITDelByIdGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs result, GPRReg stubInfoGPR, GPRReg scratch);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(MacroAssembler&);

    MacroAssembler::PatchableJump m_slowPathJump;
};

class JITInByValGenerator : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITInByValGenerator() { }

    JITInByValGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    void generateFastPath(MacroAssembler&);

    MacroAssembler::PatchableJump m_slowPathJump;
};

class JITInByIdGenerator final : public JITByIdGenerator {
public:
    JITInByIdGenerator() { }

    JITInByIdGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR);

    void generateFastPath(MacroAssembler&);
    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);
};

class JITInstanceOfGenerator final : public JITInlineCacheGenerator {
public:
    using Base = JITInlineCacheGenerator;
    JITInstanceOfGenerator() { }
    
    JITInstanceOfGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, GPRReg result,
        GPRReg value, GPRReg prototype, GPRReg stubInfoGPR, GPRReg scratch1, GPRReg scratch2,
        bool prototypeIsKnownObject = false);
    
    void generateFastPath(MacroAssembler&);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

    MacroAssembler::PatchableJump m_slowPathJump;
};

class JITGetByValGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITGetByValGenerator() { }

    JITGetByValGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs property, JSValueRegs result, GPRReg stubInfoGPR);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);
    
    void generateFastPath(MacroAssembler&);

    JSValueRegs m_base;
    JSValueRegs m_result;

    MacroAssembler::PatchableJump m_slowPathJump;
};

class JITPrivateBrandAccessGenerator final : public JITInlineCacheGenerator {
    using Base = JITInlineCacheGenerator;
public:
    JITPrivateBrandAccessGenerator() { }

    JITPrivateBrandAccessGenerator(
        CodeBlock*, Bag<StructureStubInfo>*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs brand, GPRReg stubInfoGPR);

    MacroAssembler::Jump slowPathJump() const
    {
        ASSERT(m_slowPathJump.m_jump.isSet());
        return m_slowPathJump.m_jump;
    }

    void finalize(
        LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);
    
    void generateFastPath(MacroAssembler&);

    MacroAssembler::PatchableJump m_slowPathJump;
};

template<typename VectorType>
void finalizeInlineCaches(VectorType& vector, LinkBuffer& fastPath, LinkBuffer& slowPath)
{
    for (auto& entry : vector)
        entry.finalize(fastPath, slowPath);
}

template<typename VectorType>
void finalizeInlineCaches(VectorType& vector, LinkBuffer& linkBuffer)
{
    finalizeInlineCaches(vector, linkBuffer, linkBuffer);
}

} // namespace JSC

#endif // ENABLE(JIT)
