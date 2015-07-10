/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
#include "StructureSet.h"

#include "DFGAbstractValue.h"
#include "TrackedReferences.h"
#include <wtf/CommaPrinter.h>

namespace JSC {

#if ENABLE(DFG_JIT)

void StructureSet::filter(const DFG::StructureAbstractValue& other)
{
    genericFilter([&] (Structure* structure) -> bool { return other.contains(structure); });
}

void StructureSet::filter(SpeculatedType type)
{
    genericFilter(
        [&] (Structure* structure) -> bool {
            return type & speculationFromStructure(structure);
        });
}

void StructureSet::filterArrayModes(ArrayModes arrayModes)
{
    genericFilter(
        [&] (Structure* structure) -> bool {
            return arrayModes & arrayModeFromStructure(structure);
        });
}

void StructureSet::filter(const DFG::AbstractValue& other)
{
    filter(other.m_structure);
    filter(other.m_type);
    filterArrayModes(other.m_arrayModes);
}

#endif // ENABLE(DFG_JIT)

SpeculatedType StructureSet::speculationFromStructures() const
{
    SpeculatedType result = SpecNone;
    forEach(
        [&] (Structure* structure) {
            mergeSpeculation(result, speculationFromStructure(structure));
        });
    return result;
}

ArrayModes StructureSet::arrayModesFromStructures() const
{
    ArrayModes result = 0;
    forEach(
        [&] (Structure* structure) {
            mergeArrayModes(result, asArrayModes(structure->indexingType()));
        });
    return result;
}

void StructureSet::dumpInContext(PrintStream& out, DumpContext* context) const
{
    CommaPrinter comma;
    out.print("[");
    forEach([&] (Structure* structure) { out.print(comma, inContext(*structure, context)); });
    out.print("]");
}

void StructureSet::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

void StructureSet::validateReferences(const TrackedReferences& trackedReferences) const
{
    forEach(
        [&] (Structure* structure) {
            trackedReferences.check(structure);
        });
}

} // namespace JSC

