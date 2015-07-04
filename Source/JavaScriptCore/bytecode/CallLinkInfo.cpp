/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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
#include "CallLinkInfo.h"

#include "DFGOperations.h"
#include "DFGThunks.h"
#include "JSCInlines.h"
#include "Repatch.h"
#include "RepatchBuffer.h"
#include <wtf/ListDump.h>
#include <wtf/NeverDestroyed.h>

#if ENABLE(JIT)
namespace JSC {

void CallLinkInfo::clearStub()
{
    if (!stub())
        return;

    m_stub->clearCallNodesFor(this);
    m_stub = nullptr;
}

void CallLinkInfo::unlink(RepatchBuffer& repatchBuffer)
{
    if (!isLinked()) {
        // We could be called even if we're not linked anymore because of how polymorphic calls
        // work. Each callsite within the polymorphic call stub may separately ask us to unlink().
        RELEASE_ASSERT(!isOnList());
        return;
    }
    
    unlinkFor(
        repatchBuffer, *this,
        (m_callType == Construct || m_callType == ConstructVarargs)? CodeForConstruct : CodeForCall,
        m_isFTL ? MustPreserveRegisters : RegisterPreservationNotRequired);

    // It will be on a list if the callee has a code block.
    if (isOnList())
        remove();
}

void CallLinkInfo::visitWeak(RepatchBuffer& repatchBuffer)
{
    auto handleSpecificCallee = [&] (JSFunction* callee) {
        if (Heap::isMarked(callee->executable()))
            m_hasSeenClosure = true;
        else
            m_clearedByGC = true;
    };
    
    if (isLinked()) {
        if (stub()) {
            if (!stub()->visitWeak(repatchBuffer)) {
                if (Options::verboseOSR()) {
                    dataLog(
                        "Clearing closure call from ", *repatchBuffer.codeBlock(), " to ",
                        listDump(stub()->variants()), ", stub routine ", RawPointer(stub()),
                        ".\n");
                }
                unlink(repatchBuffer);
                m_clearedByGC = true;
            }
        } else if (!Heap::isMarked(m_callee.get())) {
            if (Options::verboseOSR()) {
                dataLog(
                    "Clearing call from ", *repatchBuffer.codeBlock(), " to ",
                    RawPointer(m_callee.get()), " (",
                    m_callee.get()->executable()->hashFor(specializationKind()),
                    ").\n");
            }
            handleSpecificCallee(m_callee.get());
            unlink(repatchBuffer);
        }
    }
    if (haveLastSeenCallee() && !Heap::isMarked(lastSeenCallee())) {
        handleSpecificCallee(lastSeenCallee());
        clearLastSeenCallee();
    }
}

CallLinkInfo& CallLinkInfo::dummy()
{
    static NeverDestroyed<CallLinkInfo> dummy;
    return dummy;
}

} // namespace JSC
#endif // ENABLE(JIT)

