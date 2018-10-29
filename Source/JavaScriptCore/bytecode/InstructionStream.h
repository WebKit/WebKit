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

#include "Instruction.h"
#include <wtf/Vector.h>

namespace JSC {

struct Instruction;

class InstructionStream {
    WTF_MAKE_FAST_ALLOCATED;

    using InstructionBuffer = Vector<uint8_t, 0, UnsafeVectorOverflow>;

    friend class InstructionStreamWriter;
public:
    size_t sizeInBytes() const;

    using Offset = unsigned;

private:
    template<class InstructionBuffer>
    class BaseRef {
        WTF_MAKE_FAST_ALLOCATED;

        friend class InstructionStream;

    public:
        BaseRef(const BaseRef<InstructionBuffer>& other)
            : m_instructions(other.m_instructions)
            ,  m_index(other.m_index)
        { }

        void operator=(const BaseRef<InstructionBuffer>& other)
        {
            m_instructions = other.m_instructions;
            m_index = other.m_index;
        }

        inline const Instruction* operator->() const { return unwrap(); }
        inline const Instruction* ptr() const { return unwrap(); }

        bool operator!=(const BaseRef<InstructionBuffer>& other) const
        {
            return &m_instructions != &other.m_instructions || m_index != other.m_index;
        }

        BaseRef next() const
        {
            return BaseRef { m_instructions, m_index + ptr()->size() };
        }

        inline Offset offset() const
        {
            return m_index;
        }

        bool isValid() const
        {
            return m_index < m_instructions.size();
        }

    private:
        inline const Instruction* unwrap() const { return reinterpret_cast<const Instruction*>(&m_instructions[m_index]); }

    protected:
        BaseRef(InstructionBuffer& instructions, size_t index)
            : m_instructions(instructions)
            , m_index(index)
        { }

        InstructionBuffer& m_instructions;
        Offset m_index;
    };

public:
    using Ref = BaseRef<const InstructionBuffer>;

    class MutableRef : public BaseRef<InstructionBuffer> {
        friend class InstructionStreamWriter;

    protected:
        using BaseRef<InstructionBuffer>::BaseRef;

    public:
        Ref freeze() const  { return Ref { m_instructions, m_index }; }
        inline Instruction* operator->() { return unwrap(); }
        inline Instruction* ptr() { return unwrap(); }
        inline operator Ref()
        {
            return Ref { m_instructions, m_index };
        }

    private:
        inline Instruction* unwrap() { return reinterpret_cast<Instruction*>(&m_instructions[m_index]); }
    };

private:
    class iterator : public Ref {
        friend class InstructionStream;

    public:
        using Ref::Ref;

        Ref& operator*()
        {
            return *this;
        }

        iterator operator++()
        {
            m_index += ptr()->size();
            return *this;
        }
    };

public:
    inline iterator begin() const
    {
        return iterator { m_instructions, 0 };
    }

    inline iterator end() const
    {
        return iterator { m_instructions, m_instructions.size() };
    }

    inline const Ref at(Offset offset) const
    {
        ASSERT(offset < m_instructions.size());
        return Ref { m_instructions, offset };
    }

    inline size_t size() const
    {
        return m_instructions.size();
    }

protected:
    explicit InstructionStream(InstructionBuffer&&);

    InstructionBuffer m_instructions;
};

class InstructionStreamWriter : public InstructionStream {
    friend class BytecodeRewriter;
public:
    InstructionStreamWriter()
        : InstructionStream({ })
    { }

    inline MutableRef ref(Offset offset)
    {
        ASSERT(offset < m_instructions.size());
        return MutableRef { m_instructions, offset };
    }

    void seek(unsigned position)
    {
        ASSERT(position <= m_instructions.size());
        m_position = position;
    }

    unsigned position()
    {
        return m_position;
    }

    void write(uint8_t byte)
    {
        ASSERT(!m_finalized);
        if (m_position < m_instructions.size())
            m_instructions[m_position++] = byte;
        else {
            m_instructions.append(byte);
            m_position++;
        }
    }
    void write(uint32_t i)
    {
        ASSERT(!m_finalized);
        union {
            uint32_t i;
            uint8_t bytes[4];
        } u { i };
#if CPU(BIG_ENDIAN)
        write(u.bytes[3]);
        write(u.bytes[2]);
        write(u.bytes[1]);
        write(u.bytes[0]);
#else // !CPU(BIG_ENDIAN)
        write(u.bytes[0]);
        write(u.bytes[1]);
        write(u.bytes[2]);
        write(u.bytes[3]);
#endif // !CPU(BIG_ENDIAN)
    }

    void rewind(MutableRef& ref)
    {
        ASSERT(ref.offset() < m_instructions.size());
        m_instructions.shrink(ref.offset());
        m_position = ref.offset();
    }

    std::unique_ptr<InstructionStream> finalize()
    {
        m_finalized = true;
        m_instructions.shrinkToFit();
        return std::unique_ptr<InstructionStream> { new InstructionStream(WTFMove(m_instructions)) };
    }

    MutableRef ref()
    {
        return MutableRef { m_instructions, m_instructions.size() };
    }

    void swap(InstructionStreamWriter& other)
    {
        std::swap(m_finalized, other.m_finalized);
        std::swap(m_position, other.m_position);
        m_instructions.swap(other.m_instructions);
    }

private:
    class iterator : public MutableRef {
        friend class InstructionStreamWriter;

    protected:
        using MutableRef::MutableRef;

    public:
        MutableRef& operator*()
        {
            return *this;
        }

        iterator operator++()
        {
            m_index += ptr()->size();
            return *this;
        }
    };

public:
    iterator begin()
    {
        return iterator { m_instructions, 0 };
    }

    iterator end()
    {
        return iterator { m_instructions, m_instructions.size() };
    }

private:
    unsigned m_position { 0 };
    bool m_finalized { false };
};


} // namespace JSC
