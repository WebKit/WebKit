/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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

#include "config.h"
#include "SharedBuffer.h"

#include <algorithm>
#include <wtf/unicode/UTF8.h>

namespace WebCore {

#if !USE(NETWORK_CFDATA_ARRAY_CALLBACK)

static const unsigned segmentSize = 0x1000;
static const unsigned segmentPositionMask = 0x0FFF;

static inline unsigned segmentIndex(unsigned position)
{
    return position / segmentSize;
}

static inline unsigned offsetInSegment(unsigned position)
{
    return position & segmentPositionMask;
}

static inline char* allocateSegment() WARN_UNUSED_RETURN;
static inline char* allocateSegment()
{
    return static_cast<char*>(fastMalloc(segmentSize));
}

static inline void freeSegment(char* p)
{
    fastFree(p);
}

#endif

SharedBuffer::SharedBuffer()
    : m_buffer(adoptRef(new DataBuffer))
{
}

SharedBuffer::SharedBuffer(unsigned size)
    : m_size(size)
    , m_buffer(adoptRef(new DataBuffer))
{
}

SharedBuffer::SharedBuffer(const char* data, unsigned size)
    : m_buffer(adoptRef(new DataBuffer))
{
    append(data, size);
}

SharedBuffer::SharedBuffer(const unsigned char* data, unsigned size)
    : m_buffer(adoptRef(new DataBuffer))
{
    append(reinterpret_cast<const char*>(data), size);
}

SharedBuffer::SharedBuffer(MappedFileData&& fileData)
    : m_buffer(adoptRef(new DataBuffer))
    , m_fileData(WTFMove(fileData))
{
}

SharedBuffer::~SharedBuffer()
{
    clear();
}

RefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    bool mappingSuccess;
    MappedFileData mappedFileData(filePath, mappingSuccess);

    if (!mappingSuccess)
        return SharedBuffer::createFromReadingFile(filePath);

    return adoptRef(new SharedBuffer(WTFMove(mappedFileData)));
}

PassRefPtr<SharedBuffer> SharedBuffer::adoptVector(Vector<char>& vector)
{
    RefPtr<SharedBuffer> buffer = create();
    buffer->m_buffer->data.swap(vector);
    buffer->m_size = buffer->m_buffer->data.size();
    return buffer.release();
}

unsigned SharedBuffer::size() const
{
    if (hasPlatformData())
        return platformDataSize();
    
    if (m_fileData)
        return m_fileData.size();

    return m_size;
}

const char* SharedBuffer::data() const
{
    if (hasPlatformData())
        return platformData();

    if (m_fileData)
        return static_cast<const char*>(m_fileData.data());

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    if (const char* buffer = singleDataArrayBuffer())
        return buffer;
#endif

    return this->buffer().data();
}

RefPtr<ArrayBuffer> SharedBuffer::createArrayBuffer() const
{
    RefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::createUninitialized(static_cast<unsigned>(size()), sizeof(char));

    const char* segment = 0;
    unsigned position = 0;
    while (unsigned segmentSize = getSomeData(segment, position)) {
        memcpy(static_cast<char*>(arrayBuffer->data()) + position, segment, segmentSize);
        position += segmentSize;
    }

    if (position != arrayBuffer->byteLength()) {
        ASSERT_NOT_REACHED();
        // Don't return the incomplete ArrayBuffer.
        return nullptr;
    }

    return arrayBuffer;
}

void SharedBuffer::append(SharedBuffer* data)
{
    if (maybeAppendPlatformData(data))
        return;
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    if (maybeAppendDataArray(data))
        return;
#endif

    const char* segment;
    size_t position = 0;
    while (size_t length = data->getSomeData(segment, position)) {
        append(segment, length);
        position += length;
    }
}

void SharedBuffer::append(const char* data, unsigned length)
{
    if (!length)
        return;

    maybeTransferMappedFileData();
    maybeTransferPlatformData();

#if !USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    unsigned positionInSegment = offsetInSegment(m_size - m_buffer->data.size());
    m_size += length;

    if (m_size <= segmentSize) {
        // No need to use segments for small resource data
        if (m_buffer->data.isEmpty())
            m_buffer->data.reserveInitialCapacity(length);
        appendToDataBuffer(data, length);
        return;
    }

    char* segment;
    if (!positionInSegment) {
        segment = allocateSegment();
        m_segments.append(segment);
    } else
        segment = m_segments.last() + positionInSegment;

    unsigned segmentFreeSpace = segmentSize - positionInSegment;
    unsigned bytesToCopy = std::min(length, segmentFreeSpace);

    for (;;) {
        memcpy(segment, data, bytesToCopy);
        if (static_cast<unsigned>(length) == bytesToCopy)
            break;

        length -= bytesToCopy;
        data += bytesToCopy;
        segment = allocateSegment();
        m_segments.append(segment);
        bytesToCopy = std::min(length, segmentSize);
    }
#else
    m_size += length;
    if (m_buffer->data.isEmpty())
        m_buffer->data.reserveInitialCapacity(length);
    appendToDataBuffer(data, length);
#endif
}

void SharedBuffer::append(const Vector<char>& data)
{
    append(data.data(), data.size());
}

void SharedBuffer::clear()
{
    m_fileData = { };

    clearPlatformData();
    
#if !USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    for (char* segment : m_segments)
        freeSegment(segment);
    m_segments.clear();
#else
    m_dataArray.clear();
#endif

    m_size = 0;
    clearDataBuffer();
}

Ref<SharedBuffer> SharedBuffer::copy() const
{
    Ref<SharedBuffer> clone { adoptRef(*new SharedBuffer) };

    if (hasPlatformData() || m_fileData) {
        clone->append(data(), size());
        return clone;
    }

    clone->m_size = m_size;
    clone->m_buffer->data.reserveCapacity(m_size);
    clone->m_buffer->data.append(m_buffer->data.data(), m_buffer->data.size());

#if !USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    for (char* segment : m_segments)
        clone->m_buffer->data.append(segment, segmentSize);
#else
    for (auto& data : m_dataArray)
        clone->m_dataArray.append(data.get());
#endif
    ASSERT(clone->size() == size());

    return clone;
}

void SharedBuffer::duplicateDataBufferIfNecessary() const
{
    size_t currentCapacity = m_buffer->data.capacity();
    if (m_buffer->hasOneRef() || m_size <= currentCapacity)
        return;

    size_t newCapacity = std::max(static_cast<size_t>(m_size), currentCapacity * 2);
    RefPtr<DataBuffer> newBuffer = adoptRef(new DataBuffer);
    newBuffer->data.reserveInitialCapacity(newCapacity);
    newBuffer->data = m_buffer->data;
    m_buffer = newBuffer.release();
}

void SharedBuffer::appendToDataBuffer(const char *data, unsigned length) const
{
    duplicateDataBufferIfNecessary();
    m_buffer->data.append(data, length);
}

void SharedBuffer::clearDataBuffer()
{
    if (!m_buffer->hasOneRef())
        m_buffer = adoptRef(new DataBuffer);
    else
        m_buffer->data.clear();
}

#if !USE(CF)
void SharedBuffer::hintMemoryNotNeededSoon()
{
}
#endif

#if !USE(NETWORK_CFDATA_ARRAY_CALLBACK)

void SharedBuffer::copyBufferAndClear(char* destination, unsigned bytesToCopy) const
{
    for (char* segment : m_segments) {
        unsigned effectiveBytesToCopy = std::min(bytesToCopy, segmentSize);
        memcpy(destination, segment, effectiveBytesToCopy);
        destination += effectiveBytesToCopy;
        bytesToCopy -= effectiveBytesToCopy;
        freeSegment(segment);
    }
    m_segments.clear();
}

#endif

const Vector<char>& SharedBuffer::buffer() const
{
    unsigned bufferSize = m_buffer->data.size();
    if (m_size > bufferSize) {
        duplicateDataBufferIfNecessary();
        m_buffer->data.resize(m_size);
        copyBufferAndClear(m_buffer->data.data() + bufferSize, m_size - bufferSize);
    }
    return m_buffer->data;
}

unsigned SharedBuffer::getSomeData(const char*& someData, unsigned position) const
{
    unsigned totalSize = size();
    if (position >= totalSize) {
        someData = 0;
        return 0;
    }

    if (hasPlatformData() || m_fileData) {
        ASSERT_WITH_SECURITY_IMPLICATION(position < size());
        someData = data() + position;
        return totalSize - position;
    }

    ASSERT_WITH_SECURITY_IMPLICATION(position < m_size);
    unsigned consecutiveSize = m_buffer->data.size();
    if (position < consecutiveSize) {
        someData = m_buffer->data.data() + position;
        return consecutiveSize - position;
    }
 
    position -= consecutiveSize;
#if !USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    unsigned segments = m_segments.size();
    unsigned maxSegmentedSize = segments * segmentSize;
    unsigned segment = segmentIndex(position);
    if (segment < segments) {
        unsigned bytesLeft = totalSize - consecutiveSize;
        unsigned segmentedSize = std::min(maxSegmentedSize, bytesLeft);

        unsigned positionInSegment = offsetInSegment(position);
        someData = m_segments[segment] + positionInSegment;
        return segment == segments - 1 ? segmentedSize - position : segmentSize - positionInSegment;
    }
    ASSERT_NOT_REACHED();
    return 0;
#else
    return copySomeDataFromDataArray(someData, position);
#endif
}

void SharedBuffer::maybeTransferMappedFileData()
{
    if (m_fileData) {
        auto fileData = WTFMove(m_fileData);
        append(static_cast<const char*>(fileData.data()), fileData.size());
    }
}

#if !USE(CF) && !USE(SOUP)

inline void SharedBuffer::clearPlatformData()
{
}

inline void SharedBuffer::maybeTransferPlatformData()
{
}

inline bool SharedBuffer::hasPlatformData() const
{
    return false;
}

inline const char* SharedBuffer::platformData() const
{
    ASSERT_NOT_REACHED();

    return 0;
}

inline unsigned SharedBuffer::platformDataSize() const
{
    ASSERT_NOT_REACHED();
    
    return 0;
}

inline bool SharedBuffer::maybeAppendPlatformData(SharedBuffer*)
{
    return false;
}

#endif

RefPtr<SharedBuffer> utf8Buffer(const String& string)
{
    // Allocate a buffer big enough to hold all the characters.
    const int length = string.length();
    Vector<char> buffer(length * 3);

    // Convert to runs of 8-bit characters.
    char* p = buffer.data();
    WTF::Unicode::ConversionResult result;
    if (length) {
        if (string.is8Bit()) {
            const LChar* d = string.characters8();
            result = WTF::Unicode::convertLatin1ToUTF8(&d, d + length, &p, p + buffer.size());
        } else {
            const UChar* d = string.characters16();
            result = WTF::Unicode::convertUTF16ToUTF8(&d, d + length, &p, p + buffer.size(), true);
        }
        if (result != WTF::Unicode::conversionOK)
            return nullptr;
    }

    buffer.shrink(p - buffer.data());
    return SharedBuffer::adoptVector(buffer);
}

} // namespace WebCore
