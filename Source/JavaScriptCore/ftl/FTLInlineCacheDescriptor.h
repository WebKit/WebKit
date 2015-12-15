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
#include "DFGAbstractValue.h"
#include "DFGOperations.h"
#include "FTLInlineCacheSize.h"
#include "FTLLazySlowPath.h"
#include "JITInlineCacheGenerator.h"
#include "MacroAssembler.h"
#include "SnippetOperand.h"
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

class BinaryOpDescriptor : public InlineCacheDescriptor {
public:
    typedef EncodedJSValue (*SlowPathFunction)(ExecState*, EncodedJSValue encodedOp1, EncodedJSValue encodedOp2);

    unsigned nodeType() const { return m_nodeType; }
    size_t size() const { return m_size; }
    const char* name() const { return m_name; }
    SlowPathFunction slowPathFunction() const { return m_slowPathFunction; }

    SnippetOperand leftOperand() { return m_leftOperand; }
    SnippetOperand rightOperand() { return m_rightOperand; }

    Vector<MacroAssembler::Label> m_slowPathStarts;

protected:
    BinaryOpDescriptor(unsigned nodeType, unsigned stackmapID, CodeOrigin codeOrigin,
        size_t size, const char* name, SlowPathFunction slowPathFunction,
        const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : InlineCacheDescriptor(stackmapID, codeOrigin, nullptr)
        , m_nodeType(nodeType)
        , m_size(size)
        , m_name(name)
        , m_slowPathFunction(slowPathFunction)
        , m_leftOperand(leftOperand)
        , m_rightOperand(rightOperand)
    {
    }

    unsigned m_nodeType;
    size_t m_size;
    const char* m_name;
    SlowPathFunction m_slowPathFunction;

    SnippetOperand m_leftOperand;
    SnippetOperand m_rightOperand;
};

class BitAndDescriptor : public BinaryOpDescriptor {
public:
    BitAndDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfBitAnd(); }
    static unsigned nodeType() { return DFG::BitAnd; }
    static const char* opName() { return "BitAnd"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueBitAnd; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class BitOrDescriptor : public BinaryOpDescriptor {
public:
    BitOrDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfBitOr(); }
    static unsigned nodeType() { return DFG::BitOr; }
    static const char* opName() { return "BitOr"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueBitOr; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class BitXorDescriptor : public BinaryOpDescriptor {
public:
    BitXorDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfBitXor(); }
    static unsigned nodeType() { return DFG::BitXor; }
    static const char* opName() { return "BitXor"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueBitXor; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class BitLShiftDescriptor : public BinaryOpDescriptor {
public:
    BitLShiftDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfBitLShift(); }
    static unsigned nodeType() { return DFG::BitLShift; }
    static const char* opName() { return "BitLShift"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueBitLShift; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class BitRShiftDescriptor : public BinaryOpDescriptor {
public:
    BitRShiftDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfBitRShift(); }
    static unsigned nodeType() { return DFG::BitRShift; }
    static const char* opName() { return "BitRShift"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueBitRShift; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class BitURShiftDescriptor : public BinaryOpDescriptor {
public:
    BitURShiftDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfBitURShift(); }
    static unsigned nodeType() { return DFG::BitURShift; }
    static const char* opName() { return "BitURShift"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueBitURShift; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class ArithDivDescriptor : public BinaryOpDescriptor {
public:
    ArithDivDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfArithDiv(); }
    static unsigned nodeType() { return DFG::ArithDiv; }
    static const char* opName() { return "ArithDiv"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueDiv; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class ArithMulDescriptor : public BinaryOpDescriptor {
public:
    ArithMulDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfArithMul(); }
    static unsigned nodeType() { return DFG::ArithMul; }
    static const char* opName() { return "ArithMul"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueMul; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class ArithSubDescriptor : public BinaryOpDescriptor {
public:
    ArithSubDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfArithSub(); }
    static unsigned nodeType() { return DFG::ArithSub; }
    static const char* opName() { return "ArithSub"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueSub; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return slowPathFunction(); }
};

class ValueAddDescriptor : public BinaryOpDescriptor {
public:
    ValueAddDescriptor(unsigned stackmapID, CodeOrigin codeOrigin, const SnippetOperand& leftOperand, const SnippetOperand& rightOperand)
        : BinaryOpDescriptor(nodeType(), stackmapID, codeOrigin, icSize(), opName(), slowPathFunction(), leftOperand, rightOperand)
    { }

    static size_t icSize() { return sizeOfValueAdd(); }
    static unsigned nodeType() { return DFG::ValueAdd; }
    static const char* opName() { return "ValueAdd"; }
    static J_JITOperation_EJJ slowPathFunction() { return DFG::operationValueAdd; }
    static J_JITOperation_EJJ nonNumberSlowPathFunction() { return DFG::operationValueAddNotNumber; }
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

