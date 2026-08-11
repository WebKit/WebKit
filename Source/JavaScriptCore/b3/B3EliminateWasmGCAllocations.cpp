/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "B3EliminateWasmGCAllocations.h"

#if ENABLE(B3_JIT)

#include "B3PhaseScope.h"
#include "B3Procedure.h"
#include "B3ValueInlines.h"
#include <optional>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

bool eliminateWasmGCAllocations(Procedure& proc)
{
    PhaseScope phaseScope(proc, "eliminateWasmGCAllocations"_s);

    // Key insight is that we only need to handle WasmStructSet, since a non-escaping struct's
    // WasmStructGet reads have already been forwarded away by CSE.
    //
    // Map every WasmStructNew to its users, looking through Identity nodes (which just
    // forward the pointer) via foldIdentity. Each user is classified as we record it: an
    // Identity wrapper or a field store into the allocation itself keeps it a removal
    // candidate; any other use escapes the allocation, which we mark by resetting its entry
    // to nullopt. This way a single pass over the users decides removability.
    unsigned removableCount = 0;
    HashMap<Value*, std::optional<Vector<Value*>>> users;
    for (Value* value : proc.values()) {
        // Ensure zero-user allocations (trivially dead) get a candidate entry too.
        if (value->opcode() == WasmStructNew) {
            if (users.ensure(value, [] { return std::optional<Vector<Value*>>(std::in_place); }).isNewEntry)
                ++removableCount;
        }

        for (Value* child : value->children()) {
            Value* base = child->foldIdentity();
            if (base->opcode() != WasmStructNew)
                continue;

            auto result = users.ensure(base, [] { return std::optional<Vector<Value*>>(std::in_place); });
            if (result.isNewEntry)
                ++removableCount;
            auto& entry = result.iterator->value;
            if (!entry)
                continue; // Already known to escape.

            Opcode opcode = value->opcode();
            if (opcode == Identity || (opcode == WasmStructSet && value->child(0)->foldIdentity() == base))
                entry->append(value);
            else {
                entry = std::nullopt; // Observable use: the allocation escapes.
                --removableCount;
            }
        }
    }

    if (!removableCount)
        return false;

    // Delete each dead allocation and everything that only existed to serve it (its Identity
    // wrappers and field stores are all recorded as its users).
    for (auto& pair : users) {
        auto& maybeUsers = pair.value;
        if (!maybeUsers)
            continue;
        for (Value* user : *maybeUsers)
            user->replaceWithNopIgnoringType();
        pair.key->replaceWithNopIgnoringType();
    }

    return true;
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)
