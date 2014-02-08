/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DFGDesiredWriteBarriers_h
#define DFGDesiredWriteBarriers_h

#include "WriteBarrier.h"
#include <wtf/Vector.h>

#if ENABLE(DFG_JIT)

namespace JSC {

class JSFunction;
class ScriptExecutable;
class SlotVisitor;
class VM;
struct InlineCallFrame;

namespace DFG {

class DesiredWriteBarrier {
public:
    enum Type {
        ConstantType,
        InlineCallFrameExecutableType,
    };
    DesiredWriteBarrier(Type, CodeBlock*, unsigned index, JSCell* owner);
    DesiredWriteBarrier(Type, CodeBlock*, InlineCallFrame*, JSCell* owner);

    void trigger(VM&);
    
    void visitChildren(SlotVisitor&);

private:
    JSCell* m_owner;
    Type m_type;
    CodeBlock* m_codeBlock;
    union {
        unsigned index;
        InlineCallFrame* inlineCallFrame;
    } m_which;
};

class DesiredWriteBarriers {
public:
    DesiredWriteBarriers();
    ~DesiredWriteBarriers();

    DesiredWriteBarrier& add(DesiredWriteBarrier::Type type, CodeBlock* codeBlock, unsigned index, JSCell* owner)
    {
        m_barriers.append(DesiredWriteBarrier(type, codeBlock, index, owner));
        return m_barriers.last();
    }
    DesiredWriteBarrier& add(DesiredWriteBarrier::Type type, CodeBlock* codeBlock, InlineCallFrame* inlineCallFrame, JSCell* owner)
    {
        m_barriers.append(DesiredWriteBarrier(type, codeBlock, inlineCallFrame, owner));
        return m_barriers.last();
    }

    void trigger(VM&);
    
    void visitChildren(SlotVisitor&);

private:
    Vector<DesiredWriteBarrier> m_barriers;
};

inline void initializeLazyWriteBarrierForInlineCallFrameExecutable(DesiredWriteBarriers& barriers, WriteBarrier<ScriptExecutable>& barrier, CodeBlock* codeBlock, InlineCallFrame* inlineCallFrame, JSCell* owner, ScriptExecutable* value)
{
    DesiredWriteBarrier& desiredBarrier = barriers.add(DesiredWriteBarrier::InlineCallFrameExecutableType, codeBlock, inlineCallFrame, owner);
    barrier = WriteBarrier<ScriptExecutable>(desiredBarrier, value);
}

inline void initializeLazyWriteBarrierForConstant(DesiredWriteBarriers& barriers, WriteBarrier<Unknown>& barrier, CodeBlock* codeBlock, unsigned index, JSCell* owner, JSValue value)
{
    DesiredWriteBarrier& desiredBarrier = barriers.add(DesiredWriteBarrier::ConstantType, codeBlock, index, owner);
    barrier = WriteBarrier<Unknown>(desiredBarrier, value);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDesiredWriteBarriers_h
