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

#include "config.h"

#if ENABLE(DFG_JIT)

#include "DFGDesiredWriteBarriers.h"

#include "CodeBlock.h"
#include "JSCJSValueInlines.h"

namespace JSC { namespace DFG {

DesiredWriteBarrier::DesiredWriteBarrier(Type type, CodeBlock* codeBlock, unsigned index, JSCell* owner)
    : m_owner(owner)
    , m_type(type)
    , m_codeBlock(codeBlock)
    , m_index(index)
{
}

void DesiredWriteBarrier::trigger(VM& vm)
{
    switch (m_type) {
    case ConstantType: {
        WriteBarrier<Unknown>& barrier = m_codeBlock->constants()[m_index];
        barrier.set(vm, m_owner, barrier.get());
        break;
    }

    case InlineCallFrameExecutableType: {
        InlineCallFrame& inlineCallFrame = m_codeBlock->inlineCallFrames()[m_index];
        WriteBarrier<ScriptExecutable>& executable = inlineCallFrame.executable;
        executable.set(vm, m_owner, executable.get());
        break;
    }

    case InlineCallFrameCalleeType: {
        InlineCallFrame& inlineCallFrame = m_codeBlock->inlineCallFrames()[m_index];
        ASSERT(!!inlineCallFrame.callee);
        WriteBarrier<JSFunction>& callee = inlineCallFrame.callee;
        callee.set(vm, m_owner, callee.get());
        break;
    }

    }
}

DesiredWriteBarriers::DesiredWriteBarriers()
{
}

DesiredWriteBarriers::~DesiredWriteBarriers()
{
}

void DesiredWriteBarriers::trigger(VM& vm)
{
    for (unsigned i = 0; i < m_barriers.size(); i++)
        m_barriers[i].trigger(vm);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
