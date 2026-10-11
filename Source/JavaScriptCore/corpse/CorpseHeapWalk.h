/*
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA) && HAVE(LLDB)

#include <JavaScriptCore/CorpseAddress.h>

namespace JSC {
namespace Corpse {

class Snapshot;
class SnapshotDebugInfo;
class TargetType;

// A C++ object in a snapshot, as its complete type when it is polymorphic.
struct TargetObject {
    Address address;
    const TargetType* type;
};

// Like JSC::HeapAnalyzer, for the C++ objects of a snapshot.
class HeapAnalyzer {
public:
    virtual ~HeapAnalyzer() = default;

    // Each object the walk reaches, once for each type it is reached as. Types from different images
    // may describe one object, such as a JSC::VM held by a class of the executable and by JavaScriptCore's.
    virtual void analyzeNode(const TargetObject&) = 0;

    // A non-null pointer or reference from one object to another, once for each one `from` holds.
    virtual void analyzeEdge(const TargetObject& from, const TargetObject& to) = 0;
};

// Walks every object reachable from `root` through the pointers that SnapshotDebugInfo::pointers() gives.
void walkHeap(Snapshot&, SnapshotDebugInfo&, const TargetObject& root, HeapAnalyzer&);

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA) && HAVE(LLDB)
