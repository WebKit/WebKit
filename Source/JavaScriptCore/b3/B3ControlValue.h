/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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

#ifndef B3ControlValue_h
#define B3ControlValue_h

#if ENABLE(B3_JIT)

#include "B3FrequentedBlock.h"
#include "B3SuccessorCollection.h"
#include "B3Value.h"

namespace JSC { namespace B3 {

class BasicBlock;

class JS_EXPORT_PRIVATE ControlValue : public Value {
public:
    static bool accepts(Opcode opcode)
    {
        switch (opcode) {
        case Jump:
        case Branch:
        case Return:
        case Oops:
            return true;
        case Switch: 
            // This is here because SwitchValue is a subclass of ControlValue, so we want
            // switchValue->as<ControlValue>() to return non-null.
            return true;
        default:
            return false;
        }
    }

    typedef Vector<FrequentedBlock, 2> SuccessorList;

    ~ControlValue();

    unsigned numSuccessors() const { return m_successors.size(); }
    const FrequentedBlock& successor(unsigned index) const { return m_successors[index]; }
    FrequentedBlock& successor(unsigned index) { return m_successors[index]; }
    const SuccessorList& successors() const { return m_successors; }
    SuccessorList& successors() { return m_successors; }

    BasicBlock* successorBlock(unsigned index) const { return successor(index).block(); }
    BasicBlock*& successorBlock(unsigned index) { return successor(index).block(); }
    SuccessorCollection<BasicBlock, SuccessorList> successorBlocks()
    {
        return SuccessorCollection<BasicBlock, SuccessorList>(successors());
    }
    SuccessorCollection<const BasicBlock, const SuccessorList> successorBlocks() const
    {
        return SuccessorCollection<const BasicBlock, const SuccessorList>(successors());
    }

    bool replaceSuccessor(BasicBlock* from, BasicBlock* to);

    const FrequentedBlock& taken() const
    {
        ASSERT(opcode() == Jump || opcode() == Branch);
        return successor(0);
    }
    FrequentedBlock& taken()
    {
        ASSERT(opcode() == Jump || opcode() == Branch);
        return successor(0);
    }
    const FrequentedBlock& notTaken() const
    {
        ASSERT(opcode() == Branch);
        return successor(1);
    }
    FrequentedBlock& notTaken()
    {
        ASSERT(opcode() == Branch);
        return successor(1);
    }

    void convertToJump(BasicBlock* destination);
    void convertToOops();

protected:
    void dumpMeta(CommaPrinter&, PrintStream&) const override;

    Value* cloneImpl() const override;

    // Use this for subclasses.
    template<typename... Arguments>
    ControlValue(Opcode opcode, Type type, Origin origin, Arguments... arguments)
        : Value(CheckedOpcode, opcode, type, origin, arguments...)
    {
        ASSERT(accepts(opcode));
    }

    // Subclasses will populate this.
    SuccessorList m_successors;

private:
    friend class Procedure;

    // Use this for Oops.
    ControlValue(Opcode opcode, Origin origin)
        : Value(CheckedOpcode, opcode, Void, origin)
    {
        ASSERT(opcode == Oops);
    }

    // Use this for Return.
    ControlValue(Opcode opcode, Origin origin, Value* result)
        : Value(CheckedOpcode, opcode, Void, origin, result)
    {
        ASSERT(opcode == Return);
    }

    // Use this for Jump.
    ControlValue(Opcode opcode, Origin origin, const FrequentedBlock& target)
        : Value(CheckedOpcode, opcode, Void, origin)
    {
        ASSERT(opcode == Jump);
        m_successors.append(target);
    }

    // Use this for Branch.
    ControlValue(
        Opcode opcode, Origin origin, Value* predicate,
        const FrequentedBlock& yes, const FrequentedBlock& no)
        : Value(CheckedOpcode, opcode, Void, origin, predicate)
    {
        ASSERT(opcode == Branch);
        m_successors.append(yes);
        m_successors.append(no);
    }
};

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3ControlValue_h

