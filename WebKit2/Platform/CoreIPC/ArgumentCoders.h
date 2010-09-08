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

#ifndef SimpleArgumentCoder_h
#define SimpleArgumentCoder_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <utility>
#include <wtf/Vector.h>

namespace CoreIPC {

// An argument coder works on POD types
template<typename T> struct SimpleArgumentCoder {
    static void encode(ArgumentEncoder* encoder, const T& t)
    {
        encoder->encodeBytes(reinterpret_cast<const uint8_t*>(&t), sizeof(T));
    }
    static bool decode(ArgumentDecoder* decoder, T& t)
    {
        return decoder->decodeBytes(reinterpret_cast<uint8_t*>(&t), sizeof(T));
    }
};

template<typename T, typename U> struct ArgumentCoder<std::pair<T, U> > {
    static void encode(ArgumentEncoder* encoder, const std::pair<T, U>& pair)
    {
        encoder->encode(pair.first);
        encoder->encode(pair.second);
    }

    static bool decode(ArgumentDecoder* decoder, std::pair<T, U>& pair)
    {
        T first;
        if (!decoder->decode(first))
            return false;

        U second;
        if (!decoder->decode(second))
            return false;

        pair.first = first;
        pair.second = second;
        return true;
    }
};

template<typename T> struct ArgumentCoder<Vector<T> > {
    static void encode(ArgumentEncoder* encoder, const Vector<T>& vector)
    {
        encoder->encodeUInt64(vector.size());
        for (size_t i = 0; i < vector.size(); ++i)
            encoder->encode(vector[i]);
    }

    static bool decode(ArgumentDecoder* decoder, Vector<T>& vector)
    {
        uint64_t size;
        if (!decoder->decodeUInt64(size))
            return false;

        // Before allocating the vector, make sure that the decoder buffer is big enough.
        if (!decoder->bufferIsLargeEnoughToContain<T>(size)) {
            decoder->markInvalid();
            return false;
        }

        Vector<T> tmp;
        tmp.reserveCapacity(size);

        for (size_t i = 0; i < size; ++i) {
            T element;
            if (!decoder->decode(element))
                return false;
            
            tmp.uncheckedAppend(element);
        }

        vector.swap(tmp);
        return true;
    }
};
    
} // namespace CoreIPC

#endif // SimpleArgumentCoder_h
