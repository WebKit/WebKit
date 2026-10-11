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

#include "config.h"
#include "CorpseHeapWalk.h"

#if ENABLE(MYA) && HAVE(LLDB)

#include "CorpseError.h"
#include "CorpseSnapshot.h"
#include "CorpseSnapshotDebugInfo.h"
#include "CorpseTargetType.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace JSC {
namespace Corpse {

static unsigned long long forReport(Address address)
{
    return address.toTargetVMAddress();
}

// Whether the object has to be read through its vtable. A class only declared where it is held may be polymorphic.
static bool mayBePolymorphic(Snapshot& snapshot, SnapshotDebugInfo& debugInfo, const TargetObject& object)
{
    return object.type->isPolymorphic() || (object.type->isDeclaration() && debugInfo.startsWithVTablePointer(snapshot, object.address));
}

void walkHeap(Snapshot& snapshot, SnapshotDebugInfo& debugInfo, const TargetObject& root, HeapAnalyzer& analyzer)
{
    CORPSE_DIAGNOSTICS(diagnostics, "walking the heap from the %s at 0x%llx", root.type->name().utf8().legacyCStringPointer(), forReport(root.address));
    HashSet<std::pair<Address, const TargetType*>> visited;
    Vector<TargetObject> worklist;
    auto reach = [&](const TargetObject& object) {
        if (!visited.add({ object.address, object.type }).isNewEntry)
            return;
        analyzer.analyzeNode(object);
        worklist.append(object);
    };

    reach(root);
    while (!worklist.isEmpty()) {
        TargetObject from = worklist.takeLast();
        for (const TargetPointer& pointer : debugInfo.pointers(*from.type)) {
            auto value = snapshot.memory().ptr<uint64_t>(from.address + pointer.offset);
            if (!value) {
                CORPSE_REPORT("Could not read the %s at 0x%llx", from.type->name().utf8().legacyCStringPointer(), forReport(from.address));
                break;
            }
            if (!*value)
                continue;
            TargetObject to { Address { *value }, pointer.pointee };
            if (mayBePolymorphic(snapshot, debugInfo, to)) {
                Address completeObject;
                to.type = debugInfo.dynamicTypeAt(snapshot, to.address, completeObject);
                if (!to.type)
                    continue;
                to.address = completeObject;
            }
            reach(to);
            analyzer.analyzeEdge(from, to);
        }
    }
}

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA) && HAVE(LLDB)
