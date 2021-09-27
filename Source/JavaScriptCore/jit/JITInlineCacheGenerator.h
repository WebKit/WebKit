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

#include "CodeOrigin.h"
#include "JITOperationValidation.h"
#include "JITOperations.h"
#include "JSCJSValue.h"
#include "PutKind.h"
#include "RegisterSet.h"

namespace JSC {

class CacheableIdentifier;
class CallSiteIndex;
class CodeBlock;
class JIT;
class StructureStubInfo;
struct UnlinkedStructureStubInfo;

enum class AccessType : int8_t;
enum class JITType : uint8_t;

#if USE(JSVALUE64)
struct BaselineDelByValRegisters {
    static constexpr GPRReg base = GPRInfo::regT1;
    static constexpr GPRReg property = GPRInfo::regT0;
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg stubInfo = GPRInfo::regT3;
    static constexpr GPRReg scratch = GPRInfo::regT2;
};

struct BaselineDelByIdRegisters {
    static constexpr GPRReg base = GPRInfo::regT1;
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg stubInfo = GPRInfo::regT3;
    static constexpr GPRReg scratch = GPRInfo::regT2;
};

struct BaselineGetByValRegisters {
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg property = GPRInfo::regT1;
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg stubInfo = GPRInfo::regT2;
    static constexpr GPRReg scratch = GPRInfo::regT3;
};

struct BaselineEnumeratorGetByValRegisters {
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg property = GPRInfo::regT1;
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg stubInfo = GPRInfo::regT2;
    // We rely on this when linking a CodeBlock and initializing registers for a GetByVal StubInfo.
    static_assert(base == BaselineGetByValRegisters::base);
    static_assert(property == BaselineGetByValRegisters::property);
    static_assert(result == BaselineGetByValRegisters::result);
    static_assert(stubInfo == BaselineGetByValRegisters::stubInfo);

    static constexpr GPRReg scratch1 = GPRInfo::regT3;
    static constexpr GPRReg scratch2 = GPRInfo::regT4;
    static constexpr GPRReg scratch3 = GPRInfo::regT5;
};

struct BaselineInstanceofRegisters {
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg value = GPRInfo::argumentGPR2;
    static constexpr GPRReg proto = GPRInfo::argumentGPR3;
    static constexpr GPRReg stubInfo = GPRInfo::argumentGPR1;
    static constexpr GPRReg scratch1 = GPRInfo::nonArgGPR0;
    static constexpr GPRReg scratch2 = GPRInfo::nonArgGPR1;
};

struct BaselineInByValRegisters {
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg property = GPRInfo::regT1;
    static_assert(base == BaselineGetByValRegisters::base);
    static_assert(property == BaselineGetByValRegisters::property);
    static constexpr GPRReg stubInfo = GPRInfo::regT2;
    static constexpr GPRReg scratch = GPRInfo::regT3;
};

struct BaselineGetByIdRegisters {
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg stubInfo = GPRInfo::regT1;
    static constexpr GPRReg scratch = GPRInfo::regT2;
    static constexpr GPRReg dontClobberRegister = GPRInfo::regT3;
};

struct BaselineGetByIdWithThisRegisters {
    static constexpr GPRReg result = GPRInfo::regT0;
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg thisValue = GPRInfo::regT1;
    static constexpr GPRReg stubInfo = GPRInfo::regT2;
    static constexpr GPRReg scratch = GPRInfo::regT3;
};

struct BaselineInByIdRegisters {
    static constexpr GPRReg result = BaselineGetByIdRegisters::result;
    static constexpr GPRReg base = BaselineGetByIdRegisters::base;
    static constexpr GPRReg stubInfo = BaselineGetByIdRegisters::stubInfo;
    static constexpr GPRReg scratch = BaselineGetByIdRegisters::scratch;
};

struct BaselinePutByIdRegisters {
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg value = GPRInfo::regT1;
    static constexpr GPRReg stubInfo = GPRInfo::regT3;
    static constexpr GPRReg scratch = GPRInfo::regT2;
    static constexpr GPRReg scratch2 = GPRInfo::regT4;
};

struct BaselinePutByValRegisters {
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg property = GPRInfo::regT1;
    static constexpr GPRReg value = GPRInfo::regT2;
    static constexpr GPRReg profile = GPRInfo::regT3;
    static constexpr GPRReg stubInfo = GPRInfo::regT4;
};

struct BaselinePrivateBrandRegisters {
    static constexpr GPRReg base = GPRInfo::regT0;
    static constexpr GPRReg brand = GPRInfo::regT1;
    static_assert(base == BaselineGetByValRegisters::base);
    static_assert(brand == BaselineGetByValRegisters::property);
    static constexpr GPRReg stubInfo = GPRInfo::regT2;
};
#endif

class JITInlineCacheGenerator {
protected:
    JITInlineCacheGenerator() { }
    JITInlineCacheGenerator(CodeBlock*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters);
    
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
    StructureStubInfo* m_stubInfo;

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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, GPRReg stubInfoGPR);

    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);
    void generateFastPath(MacroAssembler&);
};

class JITPutByIdGenerator final : public JITByIdGenerator {
public:
    JITPutByIdGenerator() = default;

    JITPutByIdGenerator(
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, CacheableIdentifier,
        JSValueRegs base, JSValueRegs value, GPRReg stubInfoGPR);

    void generateFastPath(MacroAssembler&);
    void generateBaselineDataICFastPath(JIT&, unsigned stubInfoConstant, GPRReg stubInfoGPR);
};

class JITInstanceOfGenerator final : public JITInlineCacheGenerator {
public:
    using Base = JITInlineCacheGenerator;
    JITInstanceOfGenerator() { }
    
    JITInstanceOfGenerator(
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, GPRReg result,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
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
        CodeBlock*, JITType, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
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
