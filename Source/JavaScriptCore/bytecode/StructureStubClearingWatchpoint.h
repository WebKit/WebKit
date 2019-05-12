/*
 * Copyright (C) 2012, 2015 Apple Inc. All rights reserved.
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

#include "ObjectPropertyCondition.h"
#include "Watchpoint.h"

#if ENABLE(JIT)

#include <wtf/Bag.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace JSC {

class CodeBlock;
class StructureStubInfo;
class WatchpointsOnStructureStubInfo;

class StructureStubClearingWatchpoint final : public Watchpoint {
    WTF_MAKE_NONCOPYABLE(StructureStubClearingWatchpoint);
    WTF_MAKE_FAST_ALLOCATED;
public:
    StructureStubClearingWatchpoint(const ObjectPropertyCondition& key, WatchpointsOnStructureStubInfo& holder)
        : Watchpoint(Watchpoint::Type::StructureStubClearing)
        , m_holder(&holder)
        , m_key(key)
    {
    }

    void fireInternal(VM&, const FireDetail&);

private:
    // Own destructor may not be called. Keep members trivially destructible.
    JSC_WATCHPOINT_FIELD(PackedPtr<WatchpointsOnStructureStubInfo>, m_holder);
    JSC_WATCHPOINT_FIELD(ObjectPropertyCondition, m_key);
};

class WatchpointsOnStructureStubInfo {
    WTF_MAKE_NONCOPYABLE(WatchpointsOnStructureStubInfo);
    WTF_MAKE_FAST_ALLOCATED;
public:
    WatchpointsOnStructureStubInfo(CodeBlock* codeBlock, StructureStubInfo* stubInfo)
        : m_codeBlock(codeBlock)
        , m_stubInfo(stubInfo)
    {
    }
    
    StructureStubClearingWatchpoint* addWatchpoint(const ObjectPropertyCondition& key);
    
    static StructureStubClearingWatchpoint* ensureReferenceAndAddWatchpoint(
        std::unique_ptr<WatchpointsOnStructureStubInfo>& holderRef,
        CodeBlock*, StructureStubInfo*, const ObjectPropertyCondition& key);
    
    CodeBlock* codeBlock() const { return m_codeBlock; }
    StructureStubInfo* stubInfo() const { return m_stubInfo; }

    bool isValid() const;
    
private:
    CodeBlock* m_codeBlock;
    StructureStubInfo* m_stubInfo;
    Bag<StructureStubClearingWatchpoint> m_watchpoints;
};

} // namespace JSC

#endif // ENABLE(JIT)
