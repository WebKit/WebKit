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

#ifndef DataReference_h
#define DataReference_h

namespace CoreIPC {

class ArgumentDecoder;
class ArgumentEncoder;
    
class DataReference {
public:
    DataReference()
        : m_data(0)
        , m_size(0)
    {
    }

    DataReference(const uint8_t* data, size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    bool isEmpty() const { return size() == 0; }

    size_t size() const { return m_size; }
    const uint8_t* data() const 
    { 
        ASSERT(!isEmpty());
        
        return m_data; 
    }

    void encode(ArgumentEncoder* encoder) const;
    static bool decode(ArgumentDecoder* decoder, DataReference& dataReference);

private:
    const uint8_t* m_data;
    size_t m_size;
};

} // namespace CoreIPC

#endif // DataReference_h
