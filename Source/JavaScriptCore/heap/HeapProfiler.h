/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace JSC {

class HeapSnapshot;
class HeapAnalyzer;
class VM;

typedef unsigned NodeIdentifier;

class HeapProfiler {
    WTF_MAKE_TZONE_ALLOCATED(HeapProfiler);
public:
    HeapProfiler(VM&);
    ~HeapProfiler();

    VM& vm() const { return m_vm; }

    NODELETE HeapSnapshot* mostRecentSnapshot();
    void appendSnapshot(std::unique_ptr<HeapSnapshot>);
    void clearSnapshots();

    // Not atomic: only the active HeapAnalyzer may call this, while holding its own node lock.
    NodeIdentifier takeNextObjectIdentifier() { return m_nextObjectIdentifier++; }

    HeapAnalyzer* activeHeapAnalyzer() const { return m_activeAnalyzer; }
    NODELETE void setActiveHeapAnalyzer(HeapAnalyzer*);

private:
    VM& m_vm;
    Vector<std::unique_ptr<HeapSnapshot>> m_snapshots;
    HeapAnalyzer* m_activeAnalyzer { nullptr };

    // Never reset, not even when the snapshots are cleared, because clients may keep older snapshots
    // and rely on an identifier never naming a different cell. (In theory it can wrap around after
    // 2^32 identifiers; 0 is the root.)
    NodeIdentifier m_nextObjectIdentifier { 1 };
};

} // namespace JSC
