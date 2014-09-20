/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include <wtf/CommaPrinter.h>

namespace JSC {

void StructureSet::clear()
{
    deleteStructureListIfNecessary();
    setEmpty();
}

bool StructureSet::add(Structure* structure)
{
    ASSERT(structure);
    if (isThin()) {
        if (singleStructure() == structure)
            return false;
        if (!singleStructure()) {
            set(structure);
            return true;
        }
        OutOfLineList* list = OutOfLineList::create(defaultStartingSize);
        list->m_length = 2;
        list->list()[0] = singleStructure();
        list->list()[1] = structure;
        set(list);
        return true;
    }
    
    return addOutOfLine(structure);
}

bool StructureSet::remove(Structure* structure)
{
    if (isThin()) {
        if (singleStructure() == structure) {
            setEmpty();
            return true;
        }
        return false;
    }
    
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i) {
        if (list->list()[i] != structure)
            continue;
        list->list()[i] = list->list()[--list->m_length];
        if (!list->m_length) {
            OutOfLineList::destroy(list);
            setEmpty();
        }
        return true;
    }
    return false;
}

bool StructureSet::contains(Structure* structure) const
{
    if (isThin())
        return singleStructure() == structure;

    return containsOutOfLine(structure);
}

bool StructureSet::merge(const StructureSet& other)
{
    if (other.isThin()) {
        if (other.singleStructure())
            return add(other.singleStructure());
        return false;
    }
    
    OutOfLineList* list = other.structureList();
    if (list->m_length >= 2) {
        if (isThin()) {
            OutOfLineList* myNewList = OutOfLineList::create(
                list->m_length + !!singleStructure());
            if (singleStructure()) {
                myNewList->m_length = 1;
                myNewList->list()[0] = singleStructure();
            }
            set(myNewList);
        }
        bool changed = false;
        for (unsigned i = 0; i < list->m_length; ++i)
            changed |= addOutOfLine(list->list()[i]);
        return changed;
    }
    
    ASSERT(list->m_length);
    return add(list->list()[0]);
}

void StructureSet::filter(const StructureSet& other)
{
    if (other.isThin()) {
        if (!other.singleStructure() || !contains(other.singleStructure()))
            clear();
        else {
            clear();
            set(other.singleStructure());
        }
        return;
    }
    
    ContainsOutOfLine containsOutOfLine(other);
    genericFilter(containsOutOfLine);
}

void StructureSet::exclude(const StructureSet& other)
{
    if (other.isThin()) {
        if (other.singleStructure())
            remove(other.singleStructure());
        return;
    }
    
    if (isThin()) {
        if (!singleStructure())
            return;
        if (other.contains(singleStructure()))
            clear();
        return;
    }
    
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i) {
        if (!other.containsOutOfLine(list->list()[i]))
            continue;
        list->list()[i--] = list->list()[--list->m_length];
    }
    if (!list->m_length)
        clear();
}

#if ENABLE(DFG_JIT)

namespace {

class StructureAbstractValueContains {
public:
    StructureAbstractValueContains(const DFG::StructureAbstractValue& value)
        : m_value(value)
    {
    }
    
    bool operator()(Structure* structure)
    {
        return m_value.contains(structure);
    }
private:
    const DFG::StructureAbstractValue& m_value;
};

class SpeculatedTypeContains {
public:
    SpeculatedTypeContains(SpeculatedType type)
        : m_type(type)
    {
    }
    
    bool operator()(Structure* structure)
    {
        return m_type & speculationFromStructure(structure);
    }
private:
    SpeculatedType m_type;
};

class ArrayModesContains {
public:
    ArrayModesContains(ArrayModes arrayModes)
        : m_arrayModes(arrayModes)
    {
    }
    
    bool operator()(Structure* structure)
    {
        return m_arrayModes & arrayModeFromStructure(structure);
    }
private:
    ArrayModes m_arrayModes;
};

} // anonymous namespace

void StructureSet::filter(const DFG::StructureAbstractValue& other)
{
    StructureAbstractValueContains functor(other);
    genericFilter(functor);
}

void StructureSet::filter(SpeculatedType type)
{
    SpeculatedTypeContains functor(type);
    genericFilter(functor);
}

void StructureSet::filterArrayModes(ArrayModes arrayModes)
{
    ArrayModesContains functor(arrayModes);
    genericFilter(functor);
}

void StructureSet::filter(const DFG::AbstractValue& other)
{
    filter(other.m_structure);
    filter(other.m_type);
    filterArrayModes(other.m_arrayModes);
}

#endif // ENABLE(DFG_JIT)

bool StructureSet::isSubsetOf(const StructureSet& other) const
{
    if (isThin()) {
        if (!singleStructure())
            return true;
        return other.contains(singleStructure());
    }
    
    if (other.isThin()) {
        if (!other.singleStructure())
            return false;
        OutOfLineList* list = structureList();
        if (list->m_length >= 2)
            return false;
        if (list->list()[0] == other.singleStructure())
            return true;
        return false;
    }
    
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i) {
        if (!other.containsOutOfLine(list->list()[i]))
            return false;
    }
    return true;
}

bool StructureSet::overlaps(const StructureSet& other) const
{
    if (isThin()) {
        if (!singleStructure())
            return false;
        return other.contains(singleStructure());
    }
    
    if (other.isThin()) {
        if (!other.singleStructure())
            return false;
        return containsOutOfLine(other.singleStructure());
    }
    
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i) {
        if (other.containsOutOfLine(list->list()[i]))
            return true;
    }
    return false;
}

bool StructureSet::operator==(const StructureSet& other) const
{
    if (size() != other.size())
        return false;
    return isSubsetOf(other);
}

SpeculatedType StructureSet::speculationFromStructures() const
{
    if (isThin()) {
        if (!singleStructure())
            return SpecNone;
        return speculationFromStructure(singleStructure());
    }
    
    SpeculatedType result = SpecNone;
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i)
        mergeSpeculation(result, speculationFromStructure(list->list()[i]));
    return result;
}

ArrayModes StructureSet::arrayModesFromStructures() const
{
    if (isThin()) {
        if (!singleStructure())
            return 0;
        return asArrayModes(singleStructure()->indexingType());
    }
    
    ArrayModes result = 0;
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i)
        mergeArrayModes(result, asArrayModes(list->list()[i]->indexingType()));
    return result;
}

void StructureSet::dumpInContext(PrintStream& out, DumpContext* context) const
{
    CommaPrinter comma;
    out.print("[");
    for (size_t i = 0; i < size(); ++i)
        out.print(comma, inContext(*at(i), context));
    out.print("]");
}

void StructureSet::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

bool StructureSet::addOutOfLine(Structure* structure)
{
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i) {
        if (list->list()[i] == structure)
            return false;
    }
    
    if (list->m_length < list->m_capacity) {
        list->list()[list->m_length++] = structure;
        return true;
    }
    
    OutOfLineList* newList = OutOfLineList::create(list->m_capacity * 2);
    newList->m_length = list->m_length + 1;
    for (unsigned i = list->m_length; i--;)
        newList->list()[i] = list->list()[i];
    newList->list()[list->m_length] = structure;
    OutOfLineList::destroy(list);
    set(newList);
    return true;
}

bool StructureSet::containsOutOfLine(Structure* structure) const
{
    OutOfLineList* list = structureList();
    for (unsigned i = 0; i < list->m_length; ++i) {
        if (list->list()[i] == structure)
            return true;
    }
    return false;
}

void StructureSet::copyFromOutOfLine(const StructureSet& other)
{
    ASSERT(!other.isThin() && other.m_pointer != reservedValue);
    OutOfLineList* otherList = other.structureList();
    OutOfLineList* myList = OutOfLineList::create(otherList->m_length);
    myList->m_length = otherList->m_length;
    for (unsigned i = otherList->m_length; i--;)
        myList->list()[i] = otherList->list()[i];
    set(myList);
}

StructureSet::OutOfLineList* StructureSet::OutOfLineList::create(unsigned capacity)
{
    return new (NotNull, fastMalloc(sizeof(OutOfLineList) + capacity * sizeof(Structure*))) OutOfLineList(0, capacity);
}

void StructureSet::OutOfLineList::destroy(OutOfLineList* list)
{
    fastFree(list);
}

} // namespace JSC

