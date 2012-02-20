/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ArgumentEncoder_h
#define ArgumentEncoder_h

#include "ArgumentCoder.h"
#include "Attachment.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/TypeTraits.h>
#include <wtf/Vector.h>

namespace CoreIPC {

class ArgumentEncoder;
class DataReference;

class ArgumentEncoder {
public:
    static PassOwnPtr<ArgumentEncoder> create(uint64_t destinationID);
    ~ArgumentEncoder();

    void encodeFixedLengthData(const uint8_t*, size_t, unsigned alignment);
    void encodeVariableLengthByteArray(const DataReference&);

    void encodeBool(bool);
    void encodeUInt16(uint16_t);
    void encodeUInt32(uint32_t);
    void encodeUInt64(uint64_t);
    void encodeInt32(int32_t);
    void encodeInt64(int64_t);
    void encodeFloat(float);
    void encodeDouble(double);

    template<typename T> void encodeEnum(T t)
    {
        COMPILE_ASSERT(sizeof(T) <= sizeof(uint64_t), enum_type_must_not_be_larger_than_64_bits);

        encodeUInt64(static_cast<uint64_t>(t));
    }
    
    // Generic type encode function.
    template<typename T> void encode(const T& t)
    {
        ArgumentCoder<T>::encode(this, t);
    }

    uint8_t* buffer() const { return m_buffer; }
    size_t bufferSize() const { return m_bufferSize; }

    void addAttachment(const Attachment&);
    Vector<Attachment> releaseAttachments();

#ifndef NDEBUG
    void debug();
#endif

private:
    explicit ArgumentEncoder(uint64_t destinationID);
    uint8_t* grow(unsigned alignment, size_t size);
    
    uint8_t* m_buffer;
    uint8_t* m_bufferPointer;
    
    size_t m_bufferSize;
    size_t m_bufferCapacity;

    Vector<Attachment> m_attachments;
};

template<> inline void ArgumentEncoder::encode(const bool& n)
{
    encodeBool(n);
}

template<> inline void ArgumentEncoder::encode(const uint16_t& n)
{
    encodeUInt16(n);
}

template<> inline void ArgumentEncoder::encode(const uint32_t& n)
{
    encodeUInt32(n);
}

template<> inline void ArgumentEncoder::encode(const uint64_t& n)
{
    encodeUInt64(n);
}

template<> inline void ArgumentEncoder::encode(const int32_t& n)
{
    encodeInt32(n);
}

template<> inline void ArgumentEncoder::encode(const int64_t& n)
{
    encodeInt64(n);
}

template<> inline void ArgumentEncoder::encode(const float& n)
{
    encodeFloat(n);
}

template<> inline void ArgumentEncoder::encode(const double& n)
{
    encodeDouble(n);
}

} // namespace CoreIPC

#endif // ArgumentEncoder_h
