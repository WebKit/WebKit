/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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

#if USE(SOUP)
#include "GUniquePtrSoup.h"
#endif

namespace WebCore {

SharedBuffer::SharedBuffer(const char* data, size_t size)
{
    append(data, size);
}

SharedBuffer::SharedBuffer(const unsigned char* data, size_t size)
{
    append(reinterpret_cast<const char*>(data), size);
}

SharedBuffer::SharedBuffer(MappedFileData&& fileData)
    : m_size(fileData.size())
{
    m_segments.append(DataSegment::create(WTFMove(fileData)));
}

SharedBuffer::SharedBuffer(Vector<char>&& data)
{
    append(WTFMove(data));
}

RefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    bool mappingSuccess;
    MappedFileData mappedFileData(filePath, mappingSuccess);

    if (!mappingSuccess)
        return SharedBuffer::createFromReadingFile(filePath);

    return adoptRef(new SharedBuffer(WTFMove(mappedFileData)));
}

Ref<SharedBuffer> SharedBuffer::create(Vector<char>&& vector)
{
    return adoptRef(*new SharedBuffer(WTFMove(vector)));
}

void SharedBuffer::combineIntoOneSegment() const
{
    if (m_segments.size() <= 1)
        return;

    Vector<char> combinedData;
    combinedData.reserveInitialCapacity(m_size);
    for (const auto& segment : m_segments)
        combinedData.append(segment->data(), segment->size());
    ASSERT(combinedData.size() == m_size);
    m_segments.clear();
    m_segments.append(DataSegment::create(WTFMove(combinedData)));
    ASSERT(m_segments.size() == 1);
}

const char* SharedBuffer::data() const
{
    if (!m_segments.size())
        return nullptr;
    combineIntoOneSegment();
    return m_segments[0]->data();
}

RefPtr<ArrayBuffer> SharedBuffer::tryCreateArrayBuffer() const
{
    RefPtr<ArrayBuffer> arrayBuffer = ArrayBuffer::createUninitialized(static_cast<unsigned>(size()), sizeof(char));
    if (!arrayBuffer) {
        WTFLogAlways("SharedBuffer::tryCreateArrayBuffer Unable to create buffer. Requested size was %d x %lu\n", size(), sizeof(char));
        return nullptr;
    }

    size_t position = 0;
    for (const auto& segment : m_segments) {
        memcpy(static_cast<char*>(arrayBuffer->data()) + position, segment->data(), segment->size());
        position += segment->size();
    }

    ASSERT(position == m_size);
    return arrayBuffer;
}

void SharedBuffer::append(const SharedBuffer& data)
{
    m_size += data.m_size;
    m_segments.reserveCapacity(m_segments.size() + data.m_segments.size());
    for (const auto& segment : data.m_segments)
        m_segments.uncheckedAppend(segment.copyRef());
}

void SharedBuffer::append(const char* data, size_t length)
{
    m_size += length;
    Vector<char> vector;
    vector.append(data, length);
    m_segments.append(DataSegment::create(WTFMove(vector)));
}

void SharedBuffer::append(Vector<char>&& data)
{
    m_size += data.size();
    m_segments.append(DataSegment::create(WTFMove(data)));
}

void SharedBuffer::clear()
{
    m_size = 0;
    m_segments.clear();
}

Ref<SharedBuffer> SharedBuffer::copy() const
{
    Ref<SharedBuffer> clone = adoptRef(*new SharedBuffer);
    clone->m_size = m_size;
    clone->m_segments.reserveInitialCapacity(m_segments.size());
    for (const auto& segment : m_segments)
        clone->m_segments.uncheckedAppend(segment.copyRef());
    return clone;
}

const char* SharedBuffer::DataSegment::data() const
{
    auto visitor = WTF::makeVisitor(
        [](const Vector<char>& data) { return data.data(); },
#if USE(CF)
        [](const RetainPtr<CFDataRef>& data) { return reinterpret_cast<const char*>(CFDataGetBytePtr(data.get())); },
#endif
#if USE(SOUP)
        [](const GUniquePtr<SoupBuffer>& data) { return data->data; },
#endif
        [](const MappedFileData& data) { return reinterpret_cast<const char*>(data.data()); }
    );
    return WTF::visit(visitor, m_immutableData);
}

#if !USE(CF)
void SharedBuffer::hintMemoryNotNeededSoon()
{
}
#endif

size_t SharedBuffer::DataSegment::size() const
{
    auto visitor = WTF::makeVisitor(
        [](const Vector<char>& data) { return data.size(); },
#if USE(CF)
        [](const RetainPtr<CFDataRef>& data) { return CFDataGetLength(data.get()); },
#endif
#if USE(SOUP)
        [](const GUniquePtr<SoupBuffer>& data) { return static_cast<size_t>(data->length); },
#endif
        [](const MappedFileData& data) { return data.size(); }
    );
    return WTF::visit(visitor, m_immutableData);
}

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
    return SharedBuffer::create(WTFMove(buffer));
}

} // namespace WebCore
