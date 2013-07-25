/*
 * Copyright (C) 2008, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef JITCode_h
#define JITCode_h

#if ENABLE(JIT) || ENABLE(LLINT)
#include "CallFrame.h"
#include "Disassembler.h"
#include "JITStubs.h"
#include "JSCJSValue.h"
#include "LegacyProfiler.h"
#include "MacroAssemblerCodeRef.h"
#endif

namespace JSC {

#if ENABLE(JIT)
    class VM;
    class JSStack;
#endif

class JITCode : public RefCounted<JITCode> {
public:
    typedef MacroAssemblerCodeRef CodeRef;
    typedef MacroAssemblerCodePtr CodePtr;

    enum JITType { None, HostCallThunk, InterpreterThunk, BaselineJIT, DFGJIT };
    
    static JITType bottomTierJIT()
    {
        return BaselineJIT;
    }
    
    static JITType topTierJIT()
    {
        return DFGJIT;
    }
    
    static JITType nextTierJIT(JITType jitType)
    {
        ASSERT_UNUSED(jitType, jitType == BaselineJIT || jitType == DFGJIT);
        return DFGJIT;
    }
    
    static bool isOptimizingJIT(JITType jitType)
    {
        return jitType == DFGJIT;
    }
    
    static bool isBaselineCode(JITType jitType)
    {
        return jitType == InterpreterThunk || jitType == BaselineJIT;
    }
    
protected:
    JITCode(JITType);
    
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
            return None;
        return jitCode->jitType();
    }
    
    virtual CodePtr addressForCall() = 0;
    virtual void* executableAddressAtOffset(size_t offset) = 0;
    void* executableAddress() { return executableAddressAtOffset(0); }
    virtual void* dataAddressAtOffset(size_t offset) = 0;
    virtual unsigned offsetOf(void* pointerIntoCode) = 0;
    
    JSValue execute(JSStack*, CallFrame*, VM*);
    
    void* start() { return dataAddressAtOffset(0); }
    virtual size_t size() = 0;
    void* end() { return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(start()) + size()); }
    
    virtual bool contains(void*) = 0;

    static PassRefPtr<JITCode> hostFunction(CodeRef);

private:
    JITType m_jitType;
};

class DirectJITCode : public JITCode {
public:
    DirectJITCode(const CodeRef, JITType);
    ~DirectJITCode();
    
    CodePtr addressForCall();
    void* executableAddressAtOffset(size_t offset);
    void* dataAddressAtOffset(size_t offset);
    unsigned offsetOf(void* pointerIntoCode);
    size_t size();
    bool contains(void*);

private:
    CodeRef m_ref;
};

} // namespace JSC

namespace WTF {

class PrintStream;
void printInternal(PrintStream&, JSC::JITCode::JITType);

} // namespace WTF

#endif
