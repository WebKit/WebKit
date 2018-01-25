/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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

#include "VM.h"
#include "JSGlobalObject.h"
#include "ObjectPrototype.h"
#include "SlotVisitor.h"
#include "WriteBarrier.h"

namespace JSC {

class FunctionRareData;

class ObjectAllocationProfile {
    friend class LLIntOffsetsExtractor;
public:
    static ptrdiff_t offsetOfAllocator() { return OBJECT_OFFSETOF(ObjectAllocationProfile, m_allocator); }
    static ptrdiff_t offsetOfStructure() { return OBJECT_OFFSETOF(ObjectAllocationProfile, m_structure); }
    static ptrdiff_t offsetOfInlineCapacity() { return OBJECT_OFFSETOF(ObjectAllocationProfile, m_inlineCapacity); }

    ObjectAllocationProfile()
        : m_inlineCapacity(0)
    {
    }

    bool isNull() { return !m_structure; }

    void initializeProfile(VM&, JSGlobalObject*, JSCell* owner, JSObject* prototype, unsigned inferredInlineCapacity, JSFunction* constructor = nullptr, FunctionRareData* = nullptr);

    Structure* structure()
    {
        Structure* structure = m_structure.get();
        // Ensure that if we see the structure, it has been properly created
        WTF::loadLoadFence();
        return structure;
    }
    unsigned inlineCapacity() { return m_inlineCapacity; }

    void clear()
    {
        m_allocator = Allocator();
        m_structure.clear();
        m_inlineCapacity = 0;
        ASSERT(isNull());
    }

    void visitAggregate(SlotVisitor& visitor)
    {
        visitor.append(m_structure);
    }

private:
    unsigned possibleDefaultPropertyCount(VM&, JSObject* prototype);

    Allocator m_allocator; // Precomputed to make things easier for generated code.
    WriteBarrier<Structure> m_structure;
    unsigned m_inlineCapacity;
};

} // namespace JSC
