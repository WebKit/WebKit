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

#ifndef B3StackmapValue_h
#define B3StackmapValue_h

#if ENABLE(B3_JIT)

#include "B3ConstrainedValue.h"
#include "B3Value.h"
#include "B3ValueRep.h"
#include "CCallHelpers.h"
#include "RegisterSet.h"
#include <wtf/SharedTask.h>

namespace JSC { namespace B3 {

class StackmapValue;

struct StackmapGenerationParams {
    // This is the stackmap value that we're generating.
    StackmapValue* value;
    
    // This tells you the actual value representations that were chosen. This is usually different
    // from the constraints we supplied.
    Vector<ValueRep> reps;
    
    // This tells you the registers that were used.
    RegisterSet usedRegisters;
};

typedef void StackmapGeneratorFunction(CCallHelpers&, const StackmapGenerationParams&);
typedef SharedTask<StackmapGeneratorFunction> StackmapGenerator;

class JS_EXPORT_PRIVATE StackmapValue : public Value {
public:
    static bool accepts(Opcode opcode)
    {
        // This needs to include opcodes of all subclasses.
        switch (opcode) {
        case CheckAdd:
        case CheckSub:
        case CheckMul:
        case Check:
        case Patchpoint:
            return true;
        default:
            return false;
        }
    }

    ~StackmapValue();

    // Use this to add children. Note that you could also add children by doing
    // children().append(). That will work fine, but it's not recommended.
    void append(const ConstrainedValue&);

    // This is a helper for something you might do a lot of: append a value that should be constrained
    // to SomeRegister.
    void appendSomeRegister(Value*);

    const Vector<ValueRep>& reps() const { return m_reps; }

    void clobberEarly(const RegisterSet& set)
    {
        m_earlyClobbered.merge(set);
    }

    void clobberLate(const RegisterSet& set)
    {
        m_lateClobbered.merge(set);
    }

    void clobber(const RegisterSet& set)
    {
        clobberEarly(set);
        clobberLate(set);
    }

    const RegisterSet& earlyClobbered() const { return m_earlyClobbered; }
    const RegisterSet& lateClobbered() const { return m_lateClobbered; }

    void setGenerator(RefPtr<StackmapGenerator> generator)
    {
        m_generator = generator;
    }

    template<typename Functor>
    void setGenerator(const Functor& functor)
    {
        m_generator = createSharedTask<StackmapGeneratorFunction>(functor);
    }

    ConstrainedValue constrainedChild(unsigned index) const
    {
        return ConstrainedValue(child(index), index < m_reps.size() ? m_reps[index] : ValueRep());
    }

    void setConstrainedChild(unsigned index, const ConstrainedValue&);
    
    void setConstraint(unsigned index, const ValueRep&);

    class ConstrainedValueCollection {
    public:
        ConstrainedValueCollection(const StackmapValue& value)
            : m_value(value)
        {
        }

        unsigned size() const { return m_value.numChildren(); }
        
        ConstrainedValue at(unsigned index) const { return m_value.constrainedChild(index); }

        ConstrainedValue operator[](unsigned index) const { return at(index); }

        class iterator {
        public:
            iterator()
                : m_collection(nullptr)
                , m_index(0)
            {
            }

            iterator(const ConstrainedValueCollection& collection, unsigned index)
                : m_collection(&collection)
                , m_index(index)
            {
            }

            ConstrainedValue operator*() const
            {
                return m_collection->at(m_index);
            }

            iterator& operator++()
            {
                m_index++;
                return *this;
            }

            bool operator==(const iterator& other) const
            {
                ASSERT(m_collection == other.m_collection);
                return m_index == other.m_index;
            }

            bool operator!=(const iterator& other) const
            {
                return !(*this == other);
            }
            
        private:
            const ConstrainedValueCollection* m_collection;
            unsigned m_index;
        };

        iterator begin() const { return iterator(*this, 0); }
        iterator end() const { return iterator(*this, size()); }

    private:
        const StackmapValue& m_value;
    };

    ConstrainedValueCollection constrainedChildren() const
    {
        return ConstrainedValueCollection(*this);
    }

protected:
    void dumpChildren(CommaPrinter&, PrintStream&) const override;
    void dumpMeta(CommaPrinter&, PrintStream&) const override;

    StackmapValue(unsigned index, CheckedOpcodeTag, Opcode, Type, Origin);

private:
    friend class CheckSpecial;
    friend class PatchpointSpecial;
    friend class StackmapSpecial;
    
    Vector<ValueRep> m_reps;
    RefPtr<StackmapGenerator> m_generator;
    RegisterSet m_earlyClobbered;
    RegisterSet m_lateClobbered;
    RegisterSet m_usedRegisters; // Stackmaps could be further duplicated by Air, but that's unlikely, so we just merge the used registers sets if that were to happen.
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3StackmapValue_h

