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

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseTargetType.h>

namespace JSC {
namespace Corpse {

class Snapshot;
class SnapshotDebugInfo;

// Like JSC::HeapAnalyzer, for the C++ objects of a snapshot.
class HeapAnalyzer {
public:
    virtual ~HeapAnalyzer() = default;

    // Each node the walk reaches, once. An address hosts a node for each type it is read as.
    virtual void analyzeNode(const TargetObject&) = 0;

    // A non-null pointer or reference from one node to another, once for each one `from` holds. An edge through a
    // pointer to a class that `from`'s image only declares comes after every node, once the walk has reached the
    // object through an image that defines the class.
    virtual void analyzeEdge(const TargetObject& from, const TargetCanonicalPointer& to) = 0;

    // A subobject of `from`, or `from` itself, whose class needs a reader. The walk does not look inside it.
    virtual void analyzeNeedsReader(const TargetObject& from, const TargetObject& subobject) = 0;

    // A non-null pointer at `address` in `from` to a value that holds nothing the walk follows, such as a
    // scalar or a function.
    virtual void analyzeOpaqueEdge(const TargetObject& from, Address, const TargetOpaque& pointee) = 0;
};

// Walks every node reachable from `root`, reading each member of each object in turn. Each non-null pointer
// leads to the node SnapshotDebugInfo::canonicalPointer() gives. What cannot be read or described is reported,
// as is each object that only a pointer to a declared class reaches.
void walkHeap(Snapshot&, SnapshotDebugInfo&, const TargetObject& root, HeapAnalyzer&);

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)
