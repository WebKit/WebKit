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

#ifndef B3BasicBlock_h
#define B3BasicBlock_h

#if ENABLE(B3_JIT)

#include "B3FrequentedBlock.h"
#include "B3Origin.h"
#include "B3SuccessorCollection.h"
#include "B3Type.h"
#include <wtf/Vector.h>

namespace JSC { namespace B3 {

class BlockInsertionSet;
class InsertionSet;
class Procedure;
class Value;

class BasicBlock {
    WTF_MAKE_NONCOPYABLE(BasicBlock);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef Vector<Value*> ValueList;
    typedef Vector<BasicBlock*, 2> PredecessorList;
    typedef Vector<FrequentedBlock, 2> SuccessorList; // This matches ControlValue::SuccessorList

    static const char* const dumpPrefix;

    ~BasicBlock();

    unsigned index() const { return m_index; }

    ValueList::iterator begin() { return m_values.begin(); }
    ValueList::iterator end() { return m_values.end(); }
    ValueList::const_iterator begin() const { return m_values.begin(); }
    ValueList::const_iterator end() const { return m_values.end(); }

    size_t size() const { return m_values.size(); }
    Value* at(size_t index) const { return m_values[index]; }
    Value*& at(size_t index) { return m_values[index]; }

    Value* last() const { return m_values.last(); }
    Value*& last() { return m_values.last(); }

    const ValueList& values() const { return m_values; }
    ValueList& values() { return m_values; }

    JS_EXPORT_PRIVATE void append(Value*);
    JS_EXPORT_PRIVATE void replaceLast(Procedure&, Value*);

    template<typename ValueType, typename... Arguments>
    ValueType* appendNew(Procedure&, Arguments...);

    Value* appendIntConstant(Procedure&, Origin, Type, int64_t value);
    Value* appendIntConstant(Procedure&, Value* likeValue, int64_t value);

    void removeLast(Procedure&);
    
    template<typename ValueType, typename... Arguments>
    ValueType* replaceLastWithNew(Procedure&, Arguments...);

    unsigned numSuccessors() const;
    const FrequentedBlock& successor(unsigned index) const;
    FrequentedBlock& successor(unsigned index);
    const SuccessorList& successors() const;
    SuccessorList& successors();

    BasicBlock* successorBlock(unsigned index) const;
    BasicBlock*& successorBlock(unsigned index);
    SuccessorCollection<BasicBlock, SuccessorList> successorBlocks();
    SuccessorCollection<const BasicBlock, const SuccessorList> successorBlocks() const;

    bool replaceSuccessor(BasicBlock* from, BasicBlock* to);

    unsigned numPredecessors() const { return m_predecessors.size(); }
    BasicBlock* predecessor(unsigned index) const { return m_predecessors[index]; }
    BasicBlock*& predecessor(unsigned index) { return m_predecessors[index]; }
    const PredecessorList& predecessors() const { return m_predecessors; }
    PredecessorList& predecessors() { return m_predecessors; }
    bool containsPredecessor(BasicBlock* block) { return m_predecessors.contains(block); }

    bool addPredecessor(BasicBlock*);
    bool removePredecessor(BasicBlock*);
    bool replacePredecessor(BasicBlock* from, BasicBlock* to);

    // Update predecessors starting with the successors of this block.
    void updatePredecessorsAfter();

    double frequency() const { return m_frequency; }

    void dump(PrintStream&) const;
    void deepDump(const Procedure&, PrintStream&) const;

private:
    friend class BlockInsertionSet;
    friend class InsertionSet;
    friend class Procedure;
    
    // Instantiate via Procedure.
    BasicBlock(unsigned index, double frequency);

    unsigned m_index;
    ValueList m_values;
    PredecessorList m_predecessors;
    double m_frequency;
};

class DeepBasicBlockDump {
public:
    DeepBasicBlockDump(const Procedure& proc, const BasicBlock* block)
        : m_proc(proc)
        , m_block(block)
    {
    }

    void dump(PrintStream& out) const
    {
        if (m_block)
            m_block->deepDump(m_proc, out);
        else
            out.print("<null>");
    }

private:
    const Procedure& m_proc;
    const BasicBlock* m_block;
};

inline DeepBasicBlockDump deepDump(const Procedure& proc, const BasicBlock* block)
{
    return DeepBasicBlockDump(proc, block);
}

} } // namespace JSC::B3

#endif // ENABLE(B3_JIT)

#endif // B3BasicBlock_h

