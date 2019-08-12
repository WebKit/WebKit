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

#pragma once

#include "ObjectPropertyConditionSet.h"
#include "StructureSet.h"

namespace JSC {

class InstanceOfStatus;

class InstanceOfVariant {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InstanceOfVariant() = default;
    InstanceOfVariant(const StructureSet&, const ObjectPropertyConditionSet&, JSObject* prototype, bool isHit);
    
    explicit operator bool() const { return !!m_structureSet.size(); }
    
    const StructureSet& structureSet() const { return m_structureSet; }
    StructureSet& structureSet() { return m_structureSet; }
    
    const ObjectPropertyConditionSet& conditionSet() const { return m_conditionSet; }
    
    JSObject* prototype() const { return m_prototype; }
    
    bool isHit() const { return m_isHit; }
    
    bool attemptToMerge(const InstanceOfVariant& other);
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    
private:
    friend class InstanceOfStatus;
    
    StructureSet m_structureSet;
    ObjectPropertyConditionSet m_conditionSet;
    JSObject* m_prototype { nullptr };
    bool m_isHit { false };
};

} // namespace JSC

