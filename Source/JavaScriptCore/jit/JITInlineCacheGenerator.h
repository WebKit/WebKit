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
#include "JITOperations.h"
#include "JSCJSValue.h"
#include "PutKind.h"
#include "RegisterSet.h"

namespace JSC {

class CallSiteIndex;
class CodeBlock;
class StructureStubInfo;

enum class AccessType : int8_t;

class JITInlineCacheGenerator {
protected:
    JITInlineCacheGenerator() { }
    JITInlineCacheGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters);
    
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
    
protected:
    CodeBlock* m_codeBlock;
    StructureStubInfo* m_stubInfo;

    MacroAssembler::Label m_done;
    MacroAssembler::Label m_slowPathBegin;
    MacroAssembler::Call m_slowPathCall;
};

class JITByIdGenerator : public JITInlineCacheGenerator {
protected:
    JITByIdGenerator() { }

    JITByIdGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, AccessType, const RegisterSet& usedRegisters,
        JSValueRegs base, JSValueRegs value);

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

    MacroAssembler::Label m_start;
    MacroAssembler::Jump m_slowPathJump;
};

class JITGetByIdGenerator : public JITByIdGenerator {
public:
    JITGetByIdGenerator() { }

    JITGetByIdGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, UniquedStringImpl* propertyName,
        JSValueRegs base, JSValueRegs value, AccessType);
    
    void generateFastPath(MacroAssembler&);

private:
    bool m_isLengthAccess;
};

class JITGetByIdWithThisGenerator : public JITByIdGenerator {
public:
    JITGetByIdWithThisGenerator() { }

    JITGetByIdWithThisGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, UniquedStringImpl* propertyName,
        JSValueRegs value, JSValueRegs base, JSValueRegs thisRegs, AccessType);

    void generateFastPath(MacroAssembler&);
};

class JITPutByIdGenerator : public JITByIdGenerator {
public:
    JITPutByIdGenerator() { }

    JITPutByIdGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, JSValueRegs base,
        JSValueRegs value, GPRReg scratch, ECMAMode, PutKind);
    
    void generateFastPath(MacroAssembler&);
    
    V_JITOperation_ESsiJJI slowPathFunction();

private:
    ECMAMode m_ecmaMode;
    PutKind m_putKind;
};

class JITInByIdGenerator : public JITByIdGenerator {
public:
    JITInByIdGenerator() { }

    JITInByIdGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, UniquedStringImpl* propertyName,
        JSValueRegs base, JSValueRegs value);

    void generateFastPath(MacroAssembler&);
};

class JITInstanceOfGenerator : public JITInlineCacheGenerator {
public:
    JITInstanceOfGenerator() { }
    
    JITInstanceOfGenerator(
        CodeBlock*, CodeOrigin, CallSiteIndex, const RegisterSet& usedRegisters, GPRReg result,
        GPRReg value, GPRReg prototype, GPRReg scratch1, GPRReg scratch2,
        bool prototypeIsKnownObject = false);
    
    void generateFastPath(MacroAssembler&);

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);

private:
    MacroAssembler::PatchableJump m_jump;
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
