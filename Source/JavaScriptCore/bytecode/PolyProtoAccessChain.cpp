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

#include "CacheableIdentifierInlines.h"
#include "JSCInlines.h"

namespace JSC {

RefPtr<PolyProtoAccessChain> PolyProtoAccessChain::tryCreate(JSGlobalObject* globalObject, JSCell* base, CacheableIdentifier propertyName, const PropertySlot& slot)
{
    JSObject* target = slot.isUnset() ? nullptr : slot.slotBase();
    return tryCreate(globalObject, base, propertyName, target);
}

RefPtr<PolyProtoAccessChain> PolyProtoAccessChain::tryCreate(JSGlobalObject* globalObject, JSCell* base, CacheableIdentifier propertyName, JSObject* target)
{
    JSCell* current = base;

    bool found = false;

    Vector<StructureID> chain;
    for (unsigned iterationNumber = 0; true; ++iterationNumber) {
        Structure* structure = current->structure();

        if (structure->isDictionary())
            return nullptr;

        if (!structure->propertyAccessesAreCacheable())
            return nullptr;

        if (structure->isProxy())
            return nullptr;

        // To save memory, we don't include the base in the chain. We let
        // AccessCase provide the base to us as needed.
        if (iterationNumber)
            chain.append(structure->id());
        else
            RELEASE_ASSERT(current == base);

        if (current == target) {
            found = true;
            break;
        }

        // TypedArray has an ability to stop [[Prototype]] traversing for numeric index string (e.g. "0.1").
        // If we found it, then traverse should stop for Unset case.
        // https://262.ecma-international.org/9.0/#_ref_2826
        if (!target && isTypedArrayType(structure->typeInfo().type()) && isCanonicalNumericIndexString(propertyName.uid())) {
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

    return adoptRef(*new PolyProtoAccessChain(WTFMove(chain)));
}

bool PolyProtoAccessChain::needImpurePropertyWatchpoint(VM&) const
{
    for (StructureID structureID : m_chain) {
        if (structureID.decode()->needImpurePropertyWatchpoint())
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
