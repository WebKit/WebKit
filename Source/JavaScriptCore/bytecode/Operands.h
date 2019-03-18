/*
 * Copyright (C) 2011-2018 Apple Inc. All rights reserved.
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

#include "CallFrame.h"
#include "VirtualRegister.h"

#include <wtf/PrintStream.h>
#include <wtf/Vector.h>

namespace JSC {

template<typename T> struct OperandValueTraits;

enum OperandKind { ArgumentOperand, LocalOperand };

enum OperandsLikeTag { OperandsLike };

template<typename T>
class Operands {
public:
    Operands()
        : m_numArguments(0) { }
    
    explicit Operands(size_t numArguments, size_t numLocals)
        : m_numArguments(numArguments)
    {
        if (WTF::VectorTraits<T>::needsInitialization) {
            m_values.resize(numArguments + numLocals);
        } else {
            m_values.fill(T(), numArguments + numLocals);
        }
    }

    explicit Operands(size_t numArguments, size_t numLocals, const T& initialValue)
        : m_numArguments(numArguments)
    {
        m_values.fill(initialValue, numArguments + numLocals);
    }
    
    template<typename U>
    explicit Operands(OperandsLikeTag, const Operands<U>& other)
        : m_numArguments(other.numberOfArguments())
    {
        m_values.fill(T(), other.numberOfArguments() + other.numberOfLocals());
    }
    
    size_t numberOfArguments() const { return m_numArguments; }
    size_t numberOfLocals() const { return m_values.size() - m_numArguments; }
    
    size_t argumentIndex(size_t idx) const
    {
        ASSERT(idx < m_numArguments);
        return idx;
    }
    
    size_t localIndex(size_t idx) const
    {
        return m_numArguments + idx;
    }
    
    T& argument(size_t idx)
    {
        return m_values[argumentIndex(idx)];
    }
    const T& argument(size_t idx) const
    {
        return m_values[argumentIndex(idx)];
    }
    
    T& local(size_t idx) { return m_values[localIndex(idx)]; }
    const T& local(size_t idx) const { return m_values[localIndex(idx)]; }
    
    template<OperandKind operandKind>
    size_t sizeFor() const
    {
        if (operandKind == ArgumentOperand)
            return numberOfArguments();
        return numberOfLocals();
    }
    template<OperandKind operandKind>
    T& atFor(size_t idx)
    {
        if (operandKind == ArgumentOperand)
            return argument(idx);
        return local(idx);
    }
    template<OperandKind operandKind>
    const T& atFor(size_t idx) const
    {
        if (operandKind == ArgumentOperand)
            return argument(idx);
        return local(idx);
    }
    
    void ensureLocals(size_t size)
    {
        size_t oldSize = m_values.size();
        size_t newSize = m_numArguments + size;
        if (newSize <= oldSize)
            return;

        m_values.grow(newSize);
        if (!WTF::VectorTraits<T>::needsInitialization) {
            for (size_t i = oldSize; i < m_values.size(); ++i)
                m_values[i] = T();
        }
    }

    void ensureLocals(size_t size, const T& ensuredValue)
    {
        size_t oldSize = m_values.size();
        size_t newSize = m_numArguments + size;
        if (newSize <= oldSize)
            return;

        m_values.grow(newSize);
        for (size_t i = oldSize; i < m_values.size(); ++i)
            m_values[i] = ensuredValue;
    }
    
    void setLocal(size_t idx, const T& value)
    {
        ensureLocals(idx + 1);
        local(idx) = value;
    }
    
    T getLocal(size_t idx)
    {
        return idx >= numberOfLocals() ? T() : local(idx);
    }
    
    void setArgumentFirstTime(size_t idx, const T& value)
    {
        ASSERT(m_values[idx] == T());
        argument(idx) = value;
    }
    
    void setLocalFirstTime(size_t idx, const T& value)
    {
        ASSERT(idx >= numberOfLocals() || local(idx) == T());
        setLocal(idx, value);
    }
    
    size_t operandIndex(int operand) const
    {
        if (operandIsArgument(operand))
            return argumentIndex(VirtualRegister(operand).toArgument());
        return localIndex(VirtualRegister(operand).toLocal());
    }
    
    size_t operandIndex(VirtualRegister virtualRegister) const
    {
        return operandIndex(virtualRegister.offset());
    }
    
    T& operand(int operand)
    {
        if (operandIsArgument(operand))
            return argument(VirtualRegister(operand).toArgument());
        return local(VirtualRegister(operand).toLocal());
    }

    T& operand(VirtualRegister virtualRegister)
    {
        return operand(virtualRegister.offset());
    }

    const T& operand(int operand) const { return const_cast<const T&>(const_cast<Operands*>(this)->operand(operand)); }
    const T& operand(VirtualRegister operand) const { return const_cast<const T&>(const_cast<Operands*>(this)->operand(operand)); }
    
    bool hasOperand(int operand) const
    {
        if (operandIsArgument(operand))
            return true;
        return static_cast<size_t>(VirtualRegister(operand).toLocal()) < numberOfLocals();
    }
    bool hasOperand(VirtualRegister reg) const
    {
        return hasOperand(reg.offset());
    }
    
    void setOperand(int operand, const T& value)
    {
        this->operand(operand) = value;
    }
    
    void setOperand(VirtualRegister virtualRegister, const T& value)
    {
        setOperand(virtualRegister.offset(), value);
    }

    size_t size() const { return m_values.size(); }
    const T& at(size_t index) const { return m_values[index]; }
    T& at(size_t index) { return m_values[index]; }
    const T& operator[](size_t index) const { return at(index); }
    T& operator[](size_t index) { return at(index); }

    bool isArgument(size_t index) const { return index < m_numArguments; }
    bool isLocal(size_t index) const { return !isArgument(index); }
    int operandForIndex(size_t index) const
    {
        if (index < numberOfArguments())
            return virtualRegisterForArgument(index).offset();
        return virtualRegisterForLocal(index - numberOfArguments()).offset();
    }
    VirtualRegister virtualRegisterForIndex(size_t index) const
    {
        return VirtualRegister(operandForIndex(index));
    }
    
    void setOperandFirstTime(int operand, const T& value)
    {
        if (operandIsArgument(operand)) {
            setArgumentFirstTime(VirtualRegister(operand).toArgument(), value);
            return;
        }
        
        setLocalFirstTime(VirtualRegister(operand).toLocal(), value);
    }
    
    void fill(T value)
    {
        for (size_t i = 0; i < m_values.size(); ++i)
            m_values[i] = value;
    }
    
    void clear()
    {
        fill(T());
    }
    
    bool operator==(const Operands& other) const
    {
        ASSERT(numberOfArguments() == other.numberOfArguments());
        ASSERT(numberOfLocals() == other.numberOfLocals());
        
        return m_values == other.m_values;
    }
    
    void dumpInContext(PrintStream& out, DumpContext* context) const;
    void dump(PrintStream& out) const;
    
private:
    // The first m_numArguments of m_values are arguments, the rest are locals.
    Vector<T, 0, UnsafeVectorOverflow> m_values;
    unsigned m_numArguments;
};

} // namespace JSC
