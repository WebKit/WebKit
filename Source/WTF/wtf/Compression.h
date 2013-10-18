/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef Compression_h
#define Compression_h

#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WTF {

class GenericCompressedData {
    WTF_MAKE_NONCOPYABLE(GenericCompressedData)
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE static PassOwnPtr<GenericCompressedData> create(const uint8_t*, size_t);
    uint32_t compressedSize() const { return m_compressedSize; }
    uint32_t originalSize() const { return m_originalSize; }

    WTF_EXPORT_PRIVATE bool decompress(uint8_t* destination, size_t bufferSize, size_t* decompressedByteCount = 0);
    
private:
    GenericCompressedData(size_t originalSize, size_t compressedSize)
    {
        UNUSED_PARAM(m_data);
        ASSERT(!m_originalSize);
        ASSERT(!m_compressedSize);
        m_originalSize = originalSize;
        m_compressedSize = compressedSize;
    }
    uint32_t m_originalSize;
    uint32_t m_compressedSize;
    uint8_t m_data[1];
};

template <typename T> class CompressedVector : public GenericCompressedData {
public:
    static PassOwnPtr<CompressedVector> create(const Vector<T>& source)
    {
        OwnPtr<GenericCompressedData> result = GenericCompressedData::create(reinterpret_cast<const uint8_t*>(source.data()), sizeof(T) * source.size());
        return adoptPtr(static_cast<CompressedVector<T>*>(result.leakPtr()));
    }

    void decompress(Vector<T>& destination)
    {
        Vector<T> output(originalSize() / sizeof(T));
        ASSERT(output.size() * sizeof(T) == originalSize());
        size_t decompressedByteCount = 0;
        GenericCompressedData::decompress(reinterpret_cast<uint8_t*>(output.data()), originalSize(), &decompressedByteCount);
        ASSERT(decompressedByteCount == originalSize());
        ASSERT(output.size() * sizeof(T) == decompressedByteCount);

        destination.swap(output);
    }

    size_t size() const { return originalSize() / sizeof(T); }
};

template <typename T> class CompressibleVector {
    WTF_MAKE_NONCOPYABLE(CompressibleVector)
public:
    CompressibleVector(size_t size = 0)
        : m_decompressedData(size)
    {
    }

    typedef typename Vector<T>::iterator iterator;
    typedef typename Vector<T>::const_iterator const_iterator;

    void shrinkToFit()
    {
        ASSERT(!m_compressedData);
        m_compressedData = CompressedVector<T>::create(m_decompressedData);
        if (m_compressedData)
            m_decompressedData.clear();
        else
            m_decompressedData.shrinkToFit();
    }

    size_t size()
    {
        if (m_compressedData)
            return m_compressedData->size();
        return m_decompressedData.size();
    }

    template <typename U> T& operator[](Checked<U> index) { return data().at(index); }
    template <typename U> const T& operator[](Checked<U> index) const { return data().at(index); }
    template <typename U> T& at(Checked<U> index) { return data().at(index); }
    template <typename U> const T& at(Checked<U> index) const { return data().at(index); }

    iterator begin() { return data().begin(); }
    iterator end() { return data().end(); }
    const_iterator begin() const { return data().begin(); }
    const_iterator end() const { return data().end(); }

    const Vector<T>& data() const
    {
        decompressIfNecessary();
        return m_decompressedData;
    }

    Vector<T>& data()
    {
        decompressIfNecessary();
        return m_decompressedData;
    }

private:
    void decompressIfNecessary() const
    {
        if (!m_compressedData)
            return;
        m_compressedData->decompress(m_decompressedData);
        m_compressedData.clear();
    }
    mutable Vector<T> m_decompressedData;
    mutable OwnPtr<CompressedVector<T>> m_compressedData;
};

}

using WTF::GenericCompressedData;
using WTF::CompressedVector;
using WTF::CompressibleVector;

#endif
