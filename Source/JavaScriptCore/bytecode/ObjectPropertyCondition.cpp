/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "ObjectPropertyCondition.h"

#include "JSCJSValueInlines.h"
#include "TrackedReferences.h"

namespace JSC {

void ObjectPropertyCondition::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (!*this) {
        out.print("<invalid>");
        return;
    }
    
    out.print("<", inContext(JSValue(m_object), context), ": ", inContext(m_condition, context), ">");
}

void ObjectPropertyCondition::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

bool ObjectPropertyCondition::structureEnsuresValidityAssumingImpurePropertyWatchpoint(Concurrency concurrency) const
{
    if (!*this)
        return false;
    
    return m_condition.isStillValidAssumingImpurePropertyWatchpoint(concurrency, m_object->structure(), nullptr);
}

bool ObjectPropertyCondition::validityRequiresImpurePropertyWatchpoint(Structure* structure) const
{
    return m_condition.validityRequiresImpurePropertyWatchpoint(structure);
}

bool ObjectPropertyCondition::validityRequiresImpurePropertyWatchpoint() const
{
    if (!*this)
        return false;
    
    return validityRequiresImpurePropertyWatchpoint(m_object->structure());
}

bool ObjectPropertyCondition::isStillValidAssumingImpurePropertyWatchpoint(Concurrency concurrency, Structure* structure) const
{
    return m_condition.isStillValidAssumingImpurePropertyWatchpoint(concurrency, structure, m_object);
}

bool ObjectPropertyCondition::isStillValidAssumingImpurePropertyWatchpoint(Concurrency concurrency) const
{
    if (!*this)
        return false;

    return isStillValidAssumingImpurePropertyWatchpoint(concurrency, m_object->structure());
}


bool ObjectPropertyCondition::isStillValid(Concurrency concurrency, Structure* structure) const
{
    return m_condition.isStillValid(concurrency, structure, m_object);
}

bool ObjectPropertyCondition::isStillValid(Concurrency concurrency) const
{
    if (!*this)
        return false;
    
    return isStillValid(concurrency, m_object->structure());
}

bool ObjectPropertyCondition::structureEnsuresValidity(Concurrency concurrency, Structure* structure) const
{
    return m_condition.isStillValid(concurrency, structure);
}

bool ObjectPropertyCondition::structureEnsuresValidity(Concurrency concurrency) const
{
    if (!*this)
        return false;
    
    return structureEnsuresValidity(concurrency, m_object->structure());
}

bool ObjectPropertyCondition::isWatchableAssumingImpurePropertyWatchpoint(
    Structure* structure, PropertyCondition::WatchabilityEffort effort) const
{
    return m_condition.isWatchableAssumingImpurePropertyWatchpoint(structure, m_object, effort);
}

bool ObjectPropertyCondition::isWatchableAssumingImpurePropertyWatchpoint(
    PropertyCondition::WatchabilityEffort effort) const
{
    if (!*this)
        return false;
    
    return isWatchableAssumingImpurePropertyWatchpoint(m_object->structure(), effort);
}

bool ObjectPropertyCondition::isWatchable(
    Structure* structure, PropertyCondition::WatchabilityEffort effort) const
{
    return m_condition.isWatchable(structure, m_object, effort);
}

bool ObjectPropertyCondition::isWatchable(PropertyCondition::WatchabilityEffort effort) const
{
    if (!*this)
        return false;
    
    return isWatchable(m_object->structure(), effort);
}

bool ObjectPropertyCondition::isStillLive(VM& vm) const
{
    if (!*this)
        return false;
    
    bool isStillLive = true;
    forEachDependentCell([&](JSCell* cell) {
        isStillLive &= vm.heap.isMarked(cell);
    });
    return isStillLive;
}

void ObjectPropertyCondition::validateReferences(const TrackedReferences& tracked) const
{
    if (!*this)
        return;
    
    tracked.check(m_object);
    m_condition.validateReferences(tracked);
}

ObjectPropertyCondition ObjectPropertyCondition::attemptToMakeEquivalenceWithoutBarrier() const
{
    PropertyCondition result = condition().attemptToMakeEquivalenceWithoutBarrier(object());
    if (!result)
        return ObjectPropertyCondition();
    return ObjectPropertyCondition(object(), result);
}

ObjectPropertyCondition ObjectPropertyCondition::attemptToMakeReplacementWithoutBarrier() const
{
    PropertyCondition result = condition().attemptToMakeReplacementWithoutBarrier(object());
    if (!result)
        return ObjectPropertyCondition();
    return ObjectPropertyCondition(object(), result);
}

} // namespace JSC

