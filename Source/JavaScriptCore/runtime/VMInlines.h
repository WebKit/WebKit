/*
 * Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/EntryFrame.h>
#include <JavaScriptCore/FuzzerAgent.h>
#include <JavaScriptCore/ProfilerDatabase.h>
#include <JavaScriptCore/SideDataRepository.h>
#include <JavaScriptCore/VM.h>
#include <JavaScriptCore/Watchdog.h>

#if ENABLE(C_LOOP)
#include "CLoopStackInlines.h"
#endif

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

bool VM::ensureJSStackCapacityFor(Register* newTopOfStack)
{
#if !ENABLE(C_LOOP)
    return newTopOfStack >= softStackLimit();
#else
    return cloopStack().ensureCapacityFor(newTopOfStack);
#endif
    
}

bool VM::isSafeToRecurseSoft() const
{
    bool safe = isSafeToRecurse(softStackLimit());
#if ENABLE(C_LOOP)
    safe = safe && cloopStack().isSafeToRecurse();
#endif
    return safe;
}

template<typename Func>
void VM::logEvent(CodeBlock* codeBlock, const char* summary, const Func& func)
{
    if (!m_perBytecodeProfiler) [[likely]]
        return;
    
    m_perBytecodeProfiler->logEvent(codeBlock, summary, func());
}

inline CallFrame* VM::topJSCallFrame() const
{
    CallFrame* frame = topCallFrame;
    if (!frame) [[unlikely]]
        return frame;
    if (!frame->isNativeCalleeFrame() && !frame->isZombieFrame()) [[likely]]
        return frame;
    EntryFrame* entryFrame = topEntryFrame;
    do {
        frame = frame->callerFrame(entryFrame);
        ASSERT(!frame || !frame->isZombieFrame());
    } while (frame && frame->isNativeCalleeFrame());
    return frame;
}

inline void VM::setFuzzerAgent(std::unique_ptr<FuzzerAgent>&& fuzzerAgent)
{
    RELEASE_ASSERT_WITH_MESSAGE(!m_fuzzerAgent, "Only one FuzzerAgent can be specified at a time.");
    m_fuzzerAgent = WTFMove(fuzzerAgent);
}

template<typename Func>
inline void VM::forEachDebugger(const Func& callback)
{
    if (m_debuggers.isEmpty()) [[likely]]
        return;

    for (auto* debugger = m_debuggers.head(); debugger; debugger = debugger->next())
        callback(*debugger);
}

template<typename Type, typename Functor>
Type& VM::ensureSideData(void* key, const Functor& functor)
{
    m_hasSideData = true;
    return sideDataRepository().ensure<Type>(this, key, functor);
}

} // namespace JSC
