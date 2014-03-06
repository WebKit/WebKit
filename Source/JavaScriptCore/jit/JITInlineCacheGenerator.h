/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef JITInlineCacheGenerator_h
#define JITInlineCacheGenerator_h

#if ENABLE(JIT)

#include "CodeOrigin.h"
#include "JITOperations.h"
#include "JSCJSValue.h"
#include "PutKind.h"
#include "RegisterSet.h"

namespace JSC {

class CodeBlock;

class JITInlineCacheGenerator {
protected:
    JITInlineCacheGenerator() { }
    JITInlineCacheGenerator(CodeBlock*, CodeOrigin);
    
public:
    StructureStubInfo* stubInfo() const { return m_stubInfo; }

protected:
    CodeBlock* m_codeBlock;
    StructureStubInfo* m_stubInfo;
};

class JITByIdGenerator : public JITInlineCacheGenerator {
protected:
    JITByIdGenerator() { }

    JITByIdGenerator(
        CodeBlock*, CodeOrigin, const RegisterSet&, JSValueRegs base, JSValueRegs value,
        SpillRegistersMode spillMode);
    
public:
    void reportSlowPathCall(MacroAssembler::Label slowPathBegin, MacroAssembler::Call call)
    {
        m_slowPathBegin = slowPathBegin;
        m_call = call;
    }
    
    MacroAssembler::Label slowPathBegin() const { return m_slowPathBegin; }
    MacroAssembler::Jump slowPathJump() const { return m_structureCheck.m_jump; }

    void finalize(LinkBuffer& fastPathLinkBuffer, LinkBuffer& slowPathLinkBuffer);
    void finalize(LinkBuffer&);
    
protected:
    void generateFastPathChecks(MacroAssembler&, GPRReg butterfly);
    
    JSValueRegs m_base;
    JSValueRegs m_value;
    
    MacroAssembler::DataLabel32 m_structureImm;
    MacroAssembler::PatchableJump m_structureCheck;
    MacroAssembler::ConvertibleLoadLabel m_propertyStorageLoad;
    AssemblerLabel m_loadOrStore;
#if USE(JSVALUE32_64)
    AssemblerLabel m_tagLoadOrStore;
#endif
    MacroAssembler::Label m_done;
    MacroAssembler::Label m_slowPathBegin;
    MacroAssembler::Call m_call;
};

class JITGetByIdGenerator : public JITByIdGenerator {
public:
    JITGetByIdGenerator() { }

    JITGetByIdGenerator(
    CodeBlock* codeBlock, CodeOrigin codeOrigin, const RegisterSet& usedRegisters,
    JSValueRegs base, JSValueRegs value, SpillRegistersMode spillMode)
    : JITByIdGenerator(codeBlock, codeOrigin, usedRegisters, base, value, spillMode)
    {
    }
    
    void generateFastPath(MacroAssembler&);
};

class JITPutByIdGenerator : public JITByIdGenerator {
public:
    JITPutByIdGenerator() { }

    JITPutByIdGenerator(
        CodeBlock*, CodeOrigin, const RegisterSet& usedRegisters, JSValueRegs base,
        JSValueRegs, GPRReg scratch, SpillRegistersMode spillMode, ECMAMode, PutKind);
    
    void generateFastPath(MacroAssembler&);
    
    V_JITOperation_ESsiJJI slowPathFunction();

private:
    GPRReg m_scratch;
    ECMAMode m_ecmaMode;
    PutKind m_putKind;
};

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITInlineCacheGenerator_h

