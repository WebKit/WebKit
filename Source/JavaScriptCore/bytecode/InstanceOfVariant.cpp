/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "InstanceOfVariant.h"

#include "JSCJSValueInlines.h"

namespace JSC {

InstanceOfVariant::InstanceOfVariant(
    const StructureSet& structureSet, const ObjectPropertyConditionSet& conditionSet,
    JSObject* prototype, bool isHit)
    : m_structureSet(structureSet)
    , m_conditionSet(conditionSet)
    , m_prototype(prototype)
    , m_isHit(isHit)
{
}

bool InstanceOfVariant::attemptToMerge(const InstanceOfVariant& other)
{
    if (m_prototype != other.m_prototype)
        return false;
    
    if (m_isHit != other.m_isHit)
        return false;
    
    ObjectPropertyConditionSet mergedConditionSet = m_conditionSet.mergedWith(other.m_conditionSet);
    if (!mergedConditionSet.isValid())
        return false;
    m_conditionSet = mergedConditionSet;
    
    m_structureSet.merge(other.m_structureSet);
    
    return true;
}

void InstanceOfVariant::dump(PrintStream& out) const
{
    dumpInContext(out, nullptr);
}

void InstanceOfVariant::dumpInContext(PrintStream& out, DumpContext* context) const
{
    if (!*this) {
        out.print("<empty>");
        return;
    }
    
    out.print(
        "<", inContext(structureSet(), context), ", ", inContext(m_conditionSet, context), ","
        " prototype = ", inContext(JSValue(m_prototype), context), ","
        " isHit = ", m_isHit, ">");
}

} // namespace JSC

