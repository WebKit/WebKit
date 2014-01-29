/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef CodeOrigin_h
#define CodeOrigin_h

#include "CodeBlockHash.h"
#include "CodeSpecializationKind.h"
#include "JSFunction.h"
#include "ValueRecovery.h"
#include "WriteBarrier.h"
#include <wtf/BitVector.h>
#include <wtf/HashMap.h>
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace JSC {

struct InlineCallFrame;
class ExecState;
class ScriptExecutable;
class JSFunction;

struct CodeOrigin {
    static const unsigned invalidBytecodeIndex = UINT_MAX;
    
    // Bytecode offset that you'd use to re-execute this instruction, and the
    // bytecode index of the bytecode instruction that produces some result that
    // you're interested in (used for mapping Nodes whose values you're using
    // to bytecode instructions that have the appropriate value profile).
    unsigned bytecodeIndex;
    
    InlineCallFrame* inlineCallFrame;
    
    CodeOrigin()
        : bytecodeIndex(invalidBytecodeIndex)
        , inlineCallFrame(0)
    {
    }
    
    CodeOrigin(WTF::HashTableDeletedValueType)
        : bytecodeIndex(invalidBytecodeIndex)
        , inlineCallFrame(deletedMarker())
    {
    }
    
    explicit CodeOrigin(unsigned bytecodeIndex, InlineCallFrame* inlineCallFrame = 0)
        : bytecodeIndex(bytecodeIndex)
        , inlineCallFrame(inlineCallFrame)
    {
        ASSERT(bytecodeIndex < invalidBytecodeIndex);
    }
    
    bool isSet() const { return bytecodeIndex != invalidBytecodeIndex; }
    
    bool isHashTableDeletedValue() const
    {
        return bytecodeIndex == invalidBytecodeIndex && !!inlineCallFrame;
    }
    
    // The inline depth is the depth of the inline stack, so 1 = not inlined,
    // 2 = inlined one deep, etc.
    unsigned inlineDepth() const;
    
    // If the code origin corresponds to inlined code, gives you the heap object that
    // would have owned the code if it had not been inlined. Otherwise returns 0.
    ScriptExecutable* codeOriginOwner() const;
    
    int stackOffset() const;
    
    static unsigned inlineDepthForCallFrame(InlineCallFrame*);
    
    unsigned hash() const;
    bool operator==(const CodeOrigin& other) const;
    bool operator!=(const CodeOrigin& other) const { return !(*this == other); }
    
    // This checks if the two code origins correspond to the same stack trace snippets,
    // but ignore whether the InlineCallFrame's are identical.
    bool isApproximatelyEqualTo(const CodeOrigin& other) const;
    
    unsigned approximateHash() const;
    
    // Get the inline stack. This is slow, and is intended for debugging only.
    Vector<CodeOrigin> inlineStack() const;
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

private:
    static InlineCallFrame* deletedMarker()
    {
        return bitwise_cast<InlineCallFrame*>(static_cast<uintptr_t>(1));
    }
};

struct InlineCallFrame {
    Vector<ValueRecovery> arguments; // Includes 'this'.
    WriteBarrier<ScriptExecutable> executable;
    ValueRecovery calleeRecovery;
    CodeOrigin caller;
    BitVector capturedVars; // Indexed by the machine call frame's variable numbering.
    signed stackOffset : 30;
    bool isCall : 1;
    bool isClosureCall : 1; // If false then we know that callee/scope are constants and the DFG won't treat them as variables, i.e. they have to be recovered manually.
    VirtualRegister argumentsRegister; // This is only set if the code uses arguments. The unmodified arguments register follows the unmodifiedArgumentsRegister() convention (see CodeBlock.h).
    
    // There is really no good notion of a "default" set of values for
    // InlineCallFrame's fields. This constructor is here just to reduce confusion if
    // we forgot to initialize explicitly.
    InlineCallFrame()
        : stackOffset(0)
        , isCall(false)
        , isClosureCall(false)
    {
    }
    
    CodeSpecializationKind specializationKind() const { return specializationFromIsCall(isCall); }

    JSFunction* calleeConstant() const
    {
        if (calleeRecovery.isConstant())
            return jsCast<JSFunction*>(calleeRecovery.constant());
        return 0;
    }
    
    // Get the callee given a machine call frame to which this InlineCallFrame belongs.
    JSFunction* calleeForCallFrame(ExecState*) const;
    
    CString inferredName() const;
    CodeBlockHash hash() const;
    
    CodeBlock* baselineCodeBlock() const;
    
    ptrdiff_t callerFrameOffset() const { return stackOffset * sizeof(Register) + CallFrame::callerFrameOffset(); }
    ptrdiff_t returnPCOffset() const { return stackOffset * sizeof(Register) + CallFrame::returnPCOffset(); }

    void dumpBriefFunctionInformation(PrintStream&) const;
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

    MAKE_PRINT_METHOD(InlineCallFrame, dumpBriefFunctionInformation, briefFunctionInformation);
};

inline int CodeOrigin::stackOffset() const
{
    if (!inlineCallFrame)
        return 0;
    
    return inlineCallFrame->stackOffset;
}

inline unsigned CodeOrigin::hash() const
{
    return WTF::IntHash<unsigned>::hash(bytecodeIndex) +
        WTF::PtrHash<InlineCallFrame*>::hash(inlineCallFrame);
}

inline bool CodeOrigin::operator==(const CodeOrigin& other) const
{
    return bytecodeIndex == other.bytecodeIndex
        && inlineCallFrame == other.inlineCallFrame;
}
    
inline ScriptExecutable* CodeOrigin::codeOriginOwner() const
{
    if (!inlineCallFrame)
        return 0;
    return inlineCallFrame->executable.get();
}

struct CodeOriginHash {
    static unsigned hash(const CodeOrigin& key) { return key.hash(); }
    static bool equal(const CodeOrigin& a, const CodeOrigin& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct CodeOriginApproximateHash {
    static unsigned hash(const CodeOrigin& key) { return key.approximateHash(); }
    static bool equal(const CodeOrigin& a, const CodeOrigin& b) { return a.isApproximatelyEqualTo(b); }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::CodeOrigin> {
    typedef JSC::CodeOriginHash Hash;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::CodeOrigin> : SimpleClassHashTraits<JSC::CodeOrigin> {
    static const bool emptyValueIsZero = false;
};

} // namespace WTF

#endif // CodeOrigin_h

