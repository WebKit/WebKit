/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "PolyProtoAccessChain.h"

#include "JSCInlines.h"
#include "JSObject.h"

namespace JSC {

std::unique_ptr<PolyProtoAccessChain> PolyProtoAccessChain::create(JSGlobalObject* globalObject, JSCell* base, const PropertySlot& slot)
{
    JSObject* target = slot.isUnset() ? nullptr : slot.slotBase();
    return create(globalObject, base, target);
}

std::unique_ptr<PolyProtoAccessChain> PolyProtoAccessChain::create(JSGlobalObject* globalObject, JSCell* base, JSObject* target)
{
    JSCell* current = base;
    VM& vm = base->vm();

    bool found = false;

    std::unique_ptr<PolyProtoAccessChain> result(new PolyProtoAccessChain());

    for (unsigned iterationNumber = 0; true; ++iterationNumber) {
        Structure* structure = current->structure(vm);

        if (structure->isDictionary())
            return nullptr;

        if (!structure->propertyAccessesAreCacheable())
            return nullptr;

        if (structure->isProxy())
            return nullptr;

        // To save memory, we don't include the base in the chain. We let
        // AccessCase provide the base to us as needed.
        if (iterationNumber)
            result->m_chain.append(structure->id());
        else
            RELEASE_ASSERT(current == base);

        if (current == target) {
            found = true;
            break;
        }

        JSValue prototype = structure->prototypeForLookup(globalObject, current);
        if (prototype.isNull())
            break;
        current = asObject(prototype);
    }

    if (!found && !!target)
        return nullptr;

    result->m_chain.shrinkToFit();
    return result;
}

bool PolyProtoAccessChain::needImpurePropertyWatchpoint(VM& vm) const
{
    for (StructureID structureID : m_chain) {
        if (vm.getStructure(structureID)->needImpurePropertyWatchpoint())
            return true;
    }
    return false;
}

bool PolyProtoAccessChain::operator==(const PolyProtoAccessChain& other) const
{
    return m_chain == other.m_chain;
}

void PolyProtoAccessChain::dump(Structure* baseStructure, PrintStream& out) const
{
    out.print("PolyPolyProtoAccessChain: [\n");
    forEach(baseStructure->vm(), baseStructure, [&] (Structure* structure, bool) {
        out.print("\t");
        structure->dump(out);
        out.print("\n");
    });
}

}
