/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef FTLInlineCacheDescriptor_h
#define FTLInlineCacheDescriptor_h

#if ENABLE(FTL_JIT)

#include "CodeOrigin.h"
#include "FTLLazySlowPath.h"
#include "JITInlineCacheGenerator.h"
#include "MacroAssembler.h"
#include <wtf/text/UniquedStringImpl.h>

namespace JSC { namespace FTL {

class Location;

class InlineCacheDescriptor {
public:
    InlineCacheDescriptor() 
    { }
    
    InlineCacheDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, UniquedStringImpl* uid)
        : m_stackmapID(stackmapID)
        , m_codeOrigin(codeOrigin)
        , m_uid(uid)
    {
    }
    
    unsigned stackmapID() const { return m_stackmapID; }
    CodeOrigin codeOrigin() const { return m_codeOrigin; }
    UniquedStringImpl* uid() const { return m_uid; }
    
private:
    unsigned m_stackmapID;
    CodeOrigin m_codeOrigin;
    UniquedStringImpl* m_uid;
    
public:
    Vector<MacroAssembler::Jump> m_slowPathDone;
};

class GetByIdDescriptor : public InlineCacheDescriptor {
public:
    GetByIdDescriptor() { }
    
    GetByIdDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, UniquedStringImpl* uid)
        : InlineCacheDescriptor(stackmapID, codeOrigin, uid)
    {
    }
    
    Vector<JITGetByIdGenerator> m_generators;
};

class PutByIdDescriptor : public InlineCacheDescriptor {
public:
    PutByIdDescriptor() { }
    
    PutByIdDescriptor(
        unsigned stackmapID, CodeOrigin codeOrigin, UniquedStringImpl* uid,
        ECMAMode ecmaMode, PutKind putKind)
        : InlineCacheDescriptor(stackmapID, codeOrigin, uid)
        , m_ecmaMode(ecmaMode)
        , m_putKind(putKind)
    {
    }
    
    Vector<JITPutByIdGenerator> m_generators;
    
    ECMAMode ecmaMode() const { return m_ecmaMode; }
    PutKind putKind() const { return m_putKind; }

private:
    ECMAMode m_ecmaMode;
    PutKind m_putKind;
};

struct CheckInGenerator {
    StructureStubInfo* m_stub;
    MacroAssembler::Call m_slowCall;
    MacroAssembler::Label m_beginLabel;

    CheckInGenerator(StructureStubInfo* stub, MacroAssembler::Call slowCall, MacroAssembler::Label beginLabel)
        : m_stub(stub) 
        , m_slowCall(slowCall)
        , m_beginLabel(beginLabel)
    {
    }
};

class CheckInDescriptor : public InlineCacheDescriptor {
public:
    CheckInDescriptor() { }
    
    CheckInDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, UniquedStringImpl* uid)
        : InlineCacheDescriptor(stackmapID, codeOrigin, uid)
    {
    }
    
    Vector<CheckInGenerator> m_generators;
};

class ArithSubDescriptor : public InlineCacheDescriptor {
public:
    ArithSubDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, ResultType leftType, ResultType rightType)
        : InlineCacheDescriptor(stackmapID, codeOrigin, nullptr)
        , m_leftType(leftType)
        , m_rightType(rightType)
    {
    }

    ResultType leftType() const { return m_leftType; }
    ResultType rightType() const { return m_rightType; }
    
    Vector<MacroAssembler::Label> m_slowPathStarts;

private:
    ResultType m_leftType;
    ResultType m_rightType;
};

// You can create a lazy slow path call in lowerDFGToLLVM by doing:
// m_ftlState.lazySlowPaths.append(
//     LazySlowPathDescriptor(
//         stackmapID, callSiteIndex,
//         createSharedTask<RefPtr<LazySlowPath::Generator>(const Vector<Location>&)>(
//             [] (const Vector<Location>& locations) -> RefPtr<LazySlowPath::Generator> {
//                 // This lambda should just record the registers that we will be using, and return
//                 // a SharedTask that will actually generate the slow path.
//                 return createLazyCallGenerator(
//                     function, locations[0].directGPR(), locations[1].directGPR());
//             })));
//
// Usually, you can use the LowerDFGToLLVM::lazySlowPath() helper, which takes care of the descriptor
// for you and also creates the patchpoint.
typedef RefPtr<LazySlowPath::Generator> LazySlowPathLinkerFunction(const Vector<Location>&);
typedef SharedTask<LazySlowPathLinkerFunction> LazySlowPathLinkerTask;
class LazySlowPathDescriptor : public InlineCacheDescriptor {
public:
    LazySlowPathDescriptor() { }

    LazySlowPathDescriptor(
        unsigned stackmapID, CodeOrigin codeOrigin,
        RefPtr<LazySlowPathLinkerTask> linker)
        : InlineCacheDescriptor(stackmapID, codeOrigin, nullptr)
        , m_linker(linker)
    {
    }

    Vector<std::tuple<LazySlowPath*, CCallHelpers::Label>> m_generators;

    RefPtr<LazySlowPathLinkerTask> m_linker;
};

#if ENABLE(MASM_PROBE)
class ProbeDescriptor : public InlineCacheDescriptor {
public:
    ProbeDescriptor(unsigned stackmapID, std::function<void (CCallHelpers::ProbeContext*)> probeFunction)
        : InlineCacheDescriptor(stackmapID, codeOrigin(), nullptr)
        , m_probeFunction(probeFunction)
    {
    }

    std::function<void (CCallHelpers::ProbeContext*)>& probeFunction() { return m_probeFunction; }

private:
    std::function<void (CCallHelpers::ProbeContext*)> m_probeFunction;
};
#endif // ENABLE(MASM_PROBE)

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLInlineCacheDescriptor_h

