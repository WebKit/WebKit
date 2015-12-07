/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FTLExceptionHandlerManager.h"

#if ENABLE(FTL_JIT) && !FTL_USES_B3

#include "FTLState.h"

namespace JSC { namespace FTL {

ExceptionHandlerManager::ExceptionHandlerManager(State& state)
    : m_state(state)
{
}

void ExceptionHandlerManager::addNewExit(uint32_t stackmapRecordIndex, size_t osrExitIndex)
{
    m_map.add(stackmapRecordIndex, osrExitIndex);
    OSRExit& exit = m_state.jitCode->osrExit[osrExitIndex];
    RELEASE_ASSERT(exit.willArriveAtExitFromIndirectExceptionCheck());
}

void ExceptionHandlerManager::addNewCallOperationExit(uint32_t stackmapRecordIndex, size_t osrExitIndex)
{
    m_callOperationMap.add(stackmapRecordIndex, osrExitIndex);
    OSRExit& exit = m_state.jitCode->osrExit[osrExitIndex];
    RELEASE_ASSERT(exit.willArriveAtExitFromIndirectExceptionCheck());
}

CodeLocationLabel ExceptionHandlerManager::callOperationExceptionTarget(uint32_t stackmapRecordIndex)
{
#if FTL_USES_B3
    UNUSED_PARAM(stackmapRecordIndex);
    RELEASE_ASSERT_NOT_REACHED();
    return CodeLocationLabel();
#else // FTL_USES_B3
    auto findResult = m_callOperationMap.find(stackmapRecordIndex);
    if (findResult == m_callOperationMap.end())
        return CodeLocationLabel();

    size_t osrExitIndex = findResult->value;
    RELEASE_ASSERT(m_state.jitCode->osrExit[osrExitIndex].willArriveAtOSRExitFromCallOperation());
    OSRExitCompilationInfo& info = m_state.finalizer->osrExit[osrExitIndex];
    RELEASE_ASSERT(info.m_thunkLabel.isSet());
    return m_state.finalizer->exitThunksLinkBuffer->locationOf(info.m_thunkLabel);
#endif // FTL_USES_B3
}

CodeLocationLabel ExceptionHandlerManager::lazySlowPathExceptionTarget(uint32_t stackmapRecordIndex)
{
#if FTL_USES_B3
    UNUSED_PARAM(stackmapRecordIndex);
    RELEASE_ASSERT_NOT_REACHED();
    return CodeLocationLabel();
#else // FTL_USES_B3
    auto findResult = m_map.find(stackmapRecordIndex);
    if (findResult == m_map.end())
        return CodeLocationLabel();

    size_t osrExitIndex = findResult->value;
    RELEASE_ASSERT(m_state.jitCode->osrExit[osrExitIndex].m_exceptionType == ExceptionType::LazySlowPath);
    OSRExitCompilationInfo& info = m_state.finalizer->osrExit[osrExitIndex];
    RELEASE_ASSERT(info.m_thunkLabel.isSet());
    return m_state.finalizer->exitThunksLinkBuffer->locationOf(info.m_thunkLabel);
#endif // FTL_USES_B3
}

OSRExit* ExceptionHandlerManager::callOperationOSRExit(uint32_t stackmapRecordIndex)
{
    auto findResult = m_callOperationMap.find(stackmapRecordIndex);
    if (findResult == m_callOperationMap.end())
        return nullptr;
    size_t osrExitIndex = findResult->value;
    OSRExit* exit = &m_state.jitCode->osrExit[osrExitIndex];
    // We may have more than one exit for the same stackmap record index (i.e, for GetByIds and PutByIds).
    // Therefore we need to make sure this exit really is a callOperation OSR exit.
    RELEASE_ASSERT(exit->willArriveAtOSRExitFromCallOperation());
    return exit; 
}

OSRExit* ExceptionHandlerManager::getCallOSRExitCommon(uint32_t stackmapRecordIndex)
{
    auto findResult = m_map.find(stackmapRecordIndex);
    if (findResult == m_map.end())
        return nullptr;
    size_t osrExitIndex = findResult->value;
    OSRExit* exit = &m_state.jitCode->osrExit[osrExitIndex];
    RELEASE_ASSERT(exit->m_exceptionType == ExceptionType::JSCall);
    return exit; 
}

OSRExit* ExceptionHandlerManager::getCallOSRExit(uint32_t stackmapRecordIndex, const JSCall&)
{
    return getCallOSRExitCommon(stackmapRecordIndex);
}

OSRExit* ExceptionHandlerManager::getCallOSRExit(uint32_t stackmapRecordIndex, const JSCallVarargs&)
{
    return getCallOSRExitCommon(stackmapRecordIndex);
}

OSRExit* ExceptionHandlerManager::getCallOSRExit(uint32_t stackmapRecordIndex, const JSTailCall& call)
{
    UNUSED_PARAM(stackmapRecordIndex);
    UNUSED_PARAM(call);
    // A call can't be in tail position inside a try block.
    ASSERT(m_map.find(stackmapRecordIndex) == m_map.end());
    return nullptr;
}

CallSiteIndex ExceptionHandlerManager::procureCallSiteIndex(uint32_t stackmapRecordIndex, CodeOrigin origin)
{
    auto findResult = m_map.find(stackmapRecordIndex);
    if (findResult == m_map.end())
        return m_state.jitCode->common.addUniqueCallSiteIndex(origin);
    size_t osrExitIndex = findResult->value;
    OSRExit& exit = m_state.jitCode->osrExit[osrExitIndex];
    RELEASE_ASSERT(exit.m_exceptionHandlerCallSiteIndex);
    return exit.m_exceptionHandlerCallSiteIndex;
}

CallSiteIndex ExceptionHandlerManager::procureCallSiteIndex(uint32_t stackmapRecordIndex, const JSCall& call)
{
    return procureCallSiteIndex(stackmapRecordIndex, call.callSiteDescriptionOrigin());
}

CallSiteIndex ExceptionHandlerManager::procureCallSiteIndex(uint32_t stackmapRecordIndex, const JSCallVarargs& call)
{
    return procureCallSiteIndex(stackmapRecordIndex, call.callSiteDescriptionOrigin());
}

CallSiteIndex ExceptionHandlerManager::procureCallSiteIndex(uint32_t stackmapRecordIndex, const JSTailCall& call)
{
    UNUSED_PARAM(stackmapRecordIndex);
    UNUSED_PARAM(call);
    // We don't need to generate a call site index for tail calls
    // because they don't store a CallSiteIndex on the call frame.
    return CallSiteIndex();
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT) && !FTL_USES_B3
