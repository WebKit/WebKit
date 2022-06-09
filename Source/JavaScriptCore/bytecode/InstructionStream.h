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

#include "BytecodeIndex.h"
#include "Instruction.h"
#include <wtf/Vector.h>

namespace JSC {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(InstructionStream);

class InstructionStream {
    WTF_MAKE_FAST_ALLOCATED;

    friend class InstructionStreamWriter;
    friend class CachedInstructionStream;
public:
    using InstructionBuffer = Vector<uint8_t, 0, UnsafeVectorOverflow, 16, InstructionStreamMalloc>;

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

        inline Offset offset() const { return m_index; }
        inline BytecodeIndex index() const { return BytecodeIndex(offset()); }

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
        inline const Instruction* operator->() const { return unwrap(); }
        inline Instruction* ptr() { return unwrap(); }
        inline const Instruction* ptr() const { return unwrap(); }
        inline operator Ref()
        {
            return Ref { m_instructions, m_index };
        }

    private:
        inline Instruction* unwrap() { return reinterpret_cast<Instruction*>(&m_instructions[m_index]); }
        inline const Instruction* unwrap() const { return reinterpret_cast<const Instruction*>(&m_instructions[m_index]); }
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

        iterator& operator+=(size_t size)
        {
            m_index += size;
            return *this;
        }

        iterator& operator++()
        {
            return *this += ptr()->size();
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

    inline const Ref at(BytecodeIndex index) const { return at(index.offset()); }
    inline const Ref at(Offset offset) const
    {
        ASSERT(offset < m_instructions.size());
        return Ref { m_instructions, offset };
    }

    inline size_t size() const
    {
        return m_instructions.size();
    }

    const void* rawPointer() const
    {
        return m_instructions.data();
    }

    bool contains(Instruction*) const;

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

    void setInstructionBuffer(InstructionBuffer&& buffer)
    {
        RELEASE_ASSERT(!m_instructions.size());
        RELEASE_ASSERT(!buffer.size());
        m_instructions = WTFMove(buffer);
    }

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

    void write(uint16_t h)
    {
        ASSERT(!m_finalized);
        uint8_t bytes[2];
        std::memcpy(bytes, &h, sizeof(h));

        // Though not always obvious, we don't have to invert the order of the
        // bytes written here for CPU(BIG_ENDIAN). This is because the incoming
        // i value is already ordered in big endian on CPU(BIG_EDNDIAN) platforms.
        write(bytes[0]);
        write(bytes[1]);
    }

    void write(uint32_t i)
    {
        ASSERT(!m_finalized);
        uint8_t bytes[4];
        std::memcpy(bytes, &i, sizeof(i));

        // Though not always obvious, we don't have to invert the order of the
        // bytes written here for CPU(BIG_ENDIAN). This is because the incoming
        // i value is already ordered in big endian on CPU(BIG_EDNDIAN) platforms.
        write(bytes[0]);
        write(bytes[1]);
        write(bytes[2]);
        write(bytes[3]);
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

    std::unique_ptr<InstructionStream> finalize(InstructionBuffer& usedBuffer)
    {
        m_finalized = true;

        InstructionBuffer resultBuffer(m_instructions.size());
        RELEASE_ASSERT(m_instructions.sizeInBytes() == resultBuffer.sizeInBytes());
        memcpy(resultBuffer.data(), m_instructions.data(), m_instructions.sizeInBytes());

        usedBuffer = WTFMove(m_instructions);

        return std::unique_ptr<InstructionStream> { new InstructionStream(WTFMove(resultBuffer)) };
    }

    MutableRef ref()
    {
        return MutableRef { m_instructions, m_position };
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

        iterator& operator+=(size_t size)
        {
            m_index += size;
            return *this;
        }

        iterator& operator++()
        {
            return *this += ptr()->size();
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
