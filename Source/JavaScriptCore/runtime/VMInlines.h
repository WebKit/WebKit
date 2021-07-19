/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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

#include "EntryFrame.h"
#include "ProfilerDatabase.h"
#include "VM.h"
#include "Watchdog.h"

namespace JSC {

inline ActiveScratchBufferScope::ActiveScratchBufferScope(ScratchBuffer* buffer, size_t activeScratchBufferSizeInJSValues)
    : m_scratchBuffer(buffer)
{
    // Tell GC mark phase how much of the scratch buffer is active during the call operation this scope is used in.
    if (m_scratchBuffer)
        m_scratchBuffer->setActiveLength(activeScratchBufferSizeInJSValues * sizeof(EncodedJSValue));
}

inline ActiveScratchBufferScope::~ActiveScratchBufferScope()
{
    // Tell the GC that we're not using the scratch buffer anymore.
    if (m_scratchBuffer)
        m_scratchBuffer->setActiveLength(0);
}

bool VM::ensureStackCapacityFor(Register* newTopOfStack)
{
#if !ENABLE(C_LOOP)
    return newTopOfStack >= m_softStackLimit;
#else
    return ensureStackCapacityForCLoop(newTopOfStack);
#endif
    
}

bool VM::isSafeToRecurseSoft() const
{
    bool safe = isSafeToRecurse(m_softStackLimit);
#if ENABLE(C_LOOP)
    safe = safe && isSafeToRecurseSoftCLoop();
#endif
    return safe;
}

template<typename Func>
void VM::logEvent(CodeBlock* codeBlock, const char* summary, const Func& func)
{
    if (LIKELY(!m_perBytecodeProfiler))
        return;
    
    m_perBytecodeProfiler->logEvent(codeBlock, summary, func());
}

inline CallFrame* VM::topJSCallFrame() const
{
    CallFrame* frame = topCallFrame;
    if (UNLIKELY(!frame))
        return frame;
    if (LIKELY(!frame->isWasmFrame() && !frame->isStackOverflowFrame()))
        return frame;
    EntryFrame* entryFrame = topEntryFrame;
    do {
        frame = frame->callerFrame(entryFrame);
        ASSERT(!frame || !frame->isStackOverflowFrame());
    } while (frame && frame->isWasmFrame());
    return frame;
}

} // namespace JSC
