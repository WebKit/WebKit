/*
 * Copyright (C) 2012-2018 Apple Inc. All rights reserved.
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

#include "JSCPtrTag.h"
#include "JSFunction.h"
#include "MacroAssemblerCodeRef.h"
#include <wtf/SentinelLinkedList.h>

namespace JSC {

struct Instruction;

class LLIntCallLinkInfo : public PackedRawSentinelNode<LLIntCallLinkInfo> {
public:
    friend class LLIntOffsetsExtractor;

    static constexpr uintptr_t unlinkedBit = 0x1;

    LLIntCallLinkInfo() = default;
    
    ~LLIntCallLinkInfo()
    {
        if (isOnList())
            remove();
    }
    
    bool isLinked() const { return !(m_calleeOrLastSeenCalleeWithLinkBit & unlinkedBit); }
    

    void link(VM& vm, JSCell* owner, JSObject* callee, MacroAssemblerCodePtr<JSEntryPtrTag> codePtr)
    {
        if (isOnList())
            remove();
        m_calleeOrLastSeenCalleeWithLinkBit = bitwise_cast<uintptr_t>(callee);
        vm.heap.writeBarrier(owner, callee);
        m_machineCodeTarget = codePtr;
    }

    void unlink()
    {
        // Make link invalidated. It works because LLInt tests the given callee with this pointer. But it is still valid as lastSeenCallee!
        m_calleeOrLastSeenCalleeWithLinkBit |= unlinkedBit;
        m_machineCodeTarget = MacroAssemblerCodePtr<JSEntryPtrTag>();
        if (isOnList())
            remove();
    }

    JSObject* callee() const
    {
        if (!isLinked())
            return nullptr;
        return bitwise_cast<JSObject*>(m_calleeOrLastSeenCalleeWithLinkBit);
    }

    JSObject* lastSeenCallee() const
    {
        return bitwise_cast<JSObject*>(m_calleeOrLastSeenCalleeWithLinkBit & ~unlinkedBit);
    }

    void clearLastSeenCallee()
    {
        m_calleeOrLastSeenCalleeWithLinkBit = unlinkedBit;
    }

    ArrayProfile m_arrayProfile;

private:
    uintptr_t m_calleeOrLastSeenCalleeWithLinkBit { unlinkedBit };
    MacroAssemblerCodePtr<JSEntryPtrTag> m_machineCodeTarget;
};

} // namespace JSC
