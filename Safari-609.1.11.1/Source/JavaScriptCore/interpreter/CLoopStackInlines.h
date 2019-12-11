/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#if ENABLE(C_LOOP)

#include "CLoopStack.h"
#include "CallFrame.h"
#include "CodeBlock.h"
#include "VM.h"

namespace JSC {

inline bool CLoopStack::ensureCapacityFor(Register* newTopOfStack)
{
    if (newTopOfStack >= m_end)
        return true;
    return grow(newTopOfStack);
}

inline void* CLoopStack::currentStackPointer()
{
    // One might be tempted to assert that m_currentStackPointer <= m_topCallFrame->topOfFrame()
    // here. That assertion would be incorrect because this function may be called from function
    // prologues (e.g. during a stack check) where m_currentStackPointer may be higher than
    // m_topCallFrame->topOfFrame() because the stack pointer has not been initialized to point
    // to frame top yet.
    return m_currentStackPointer;
}

inline void CLoopStack::setCLoopStackLimit(Register* newTopOfStack)
{
    m_end = newTopOfStack;
    m_vm.setCLoopStackLimit(newTopOfStack);
}

} // namespace JSC

#endif // ENABLE(C_LOOP)
