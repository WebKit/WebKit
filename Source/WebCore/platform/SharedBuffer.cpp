/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
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
#include <wtf/HexNumber.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/UTF8Conversion.h>

namespace WebCore {

SharedBuffer::SharedBuffer(const uint8_t* data, size_t size)
{
    append(data, size);
}

SharedBuffer::SharedBuffer(const char* data, size_t size)
{
    append(reinterpret_cast<const uint8_t*>(data), size);
}

SharedBuffer::SharedBuffer(FileSystem::MappedFileData&& fileData)
    : m_size(fileData.size())
{
    m_segments.append({0, DataSegment::create(WTFMove(fileData))});
}

SharedBuffer::SharedBuffer(Vector<uint8_t>&& data)
{
    append(WTFMove(data));
}

#if USE(GSTREAMER)
Ref<SharedBuffer> SharedBuffer::create(GstMappedOwnedBuffer& mappedBuffer)
{
    return adoptRef(*new SharedBuffer(mappedBuffer));
}

SharedBuffer::SharedBuffer(GstMappedOwnedBuffer& mappedBuffer)
    : m_size(mappedBuffer.size())
{
    m_segments.append({0, DataSegment::create(&mappedBuffer)});
}
#endif

RefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath, FileSystem::MappedFileMode mappedFileMode, MayUseFileMapping mayUseFileMapping)
{
    if (mayUseFileMapping == MayUseFileMapping::Yes) {
        bool mappingSuccess;
        FileSystem::MappedFileData mappedFileData(filePath, mappedFileMode, mappingSuccess);
        if (mappingSuccess)
            return adoptRef(new SharedBuffer(WTFMove(mappedFileData)));
    }
    return SharedBuffer::createFromReadingFile(filePath);
}

Ref<SharedBuffer> SharedBuffer::create(Vector<uint8_t>&& vector)
{
    return adoptRef(*new SharedBuffer(WTFMove(vector)));
}

static Vector<uint8_t> combineSegmentsData(const SharedBuffer::DataSegmentVector& segments, size_t size)
{
    Vector<uint8_t> combinedData;
    combinedData.reserveInitialCapacity(size);
    for (auto& segment : segments)
        combinedData.append(segment.segment->data(), segment.segment->size());
    ASSERT(combinedData.size() == size);
    return combinedData;
}

void SharedBuffer::combineIntoOneSegment() const
{
#if ASSERT_ENABLED
    // FIXME: We ought to be able to set this to true and have no assertions fire.
    // Remove all instances of appending after calling this, because they are all O(n^2) algorithms since r215686.
    // m_hasBeenCombinedIntoOneSegment = true;
#endif
    if (m_segments.size() <= 1)
        return;

    auto combinedData = combineSegmentsData(m_segments, m_size);
    m_segments.clear();
    m_segments.append({0, DataSegment::create(WTFMove(combinedData))});
    ASSERT(m_segments.size() == 1);
    ASSERT(internallyConsistent());
}

const uint8_t* SharedBuffer::data() const
{
    if (m_segments.isEmpty())
        return nullptr;
    combineIntoOneSegment();
    ASSERT(internallyConsistent());
    return m_segments[0].segment->data();
}

Vector<uint8_t> SharedBuffer::copyData() const
{
    Vector<uint8_t> data;
    data.reserveInitialCapacity(size());
    forEachSegment([&data](auto& span) {
        data.uncheckedAppend(span);
    });
    return data;
}

Vector<uint8_t> SharedBuffer::takeData()
{
    if (m_segments.isEmpty())
        return { };

    Vector<uint8_t> combinedData;
    if (hasOneSegment() && WTF::holds_alternative<Vector<uint8_t>>(m_segments[0].segment->m_immutableData))
        combinedData = std::exchange(WTF::get<Vector<uint8_t>>(m_segments[0].segment->m_immutableData), Vector<uint8_t>());
    else
        combinedData = combineSegmentsData(m_segments, m_size);

    clear();
    return combinedData;
}

SharedBufferDataView SharedBuffer::getSomeData(size_t position) const
{
    RELEASE_ASSERT(position < m_size);
    auto comparator = [](const size_t& position, const DataSegmentVectorEntry& entry) {
        return position < entry.beginPosition;
    };
    const DataSegmentVectorEntry* element = std::upper_bound(m_segments.begin(), m_segments.end(), position, comparator);
    element--; // std::upper_bound gives a pointer to the element that is greater than position. We want the element just before that.
    return { element->segment.copyRef(), position - element->beginPosition };
}

String SharedBuffer::toHexString() const
{
    StringBuilder stringBuilder;
    forEachSegment([&](auto& segment) {
        for (unsigned i = 0; i < segment.size(); ++i)
            stringBuilder.append(pad('0', 2, hex(segment[i])));
    });
    return stringBuilder.toString();
}

RefPtr<ArrayBuffer> SharedBuffer::tryCreateArrayBuffer() const
{
    auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(static_cast<unsigned>(size()), 1);
    if (!arrayBuffer) {
        WTFLogAlways("SharedBuffer::tryCreateArrayBuffer Unable to create buffer. Requested size was %zu\n", size());
        return nullptr;
    }

    size_t position = 0;
    for (const auto& segment : m_segments) {
        memcpy(static_cast<uint8_t*>(arrayBuffer->data()) + position, segment.segment->data(), segment.segment->size());
        position += segment.segment->size();
    }

    ASSERT(position == m_size);
    ASSERT(internallyConsistent());
    return arrayBuffer;
}

void SharedBuffer::append(const SharedBuffer& data)
{
    ASSERT(!m_hasBeenCombinedIntoOneSegment);
    m_segments.reserveCapacity(m_segments.size() + data.m_segments.size());
    for (const auto& element : data.m_segments) {
        m_segments.uncheckedAppend({m_size, element.segment.copyRef()});
        m_size += element.segment->size();
    }
    ASSERT(internallyConsistent());
}

void SharedBuffer::append(const uint8_t* data, size_t length)
{
    ASSERT(!m_hasBeenCombinedIntoOneSegment);
    Vector<uint8_t> vector;
    vector.append(data, length);
    m_segments.append({m_size, DataSegment::create(WTFMove(vector))});
    m_size += length;
    ASSERT(internallyConsistent());
}

void SharedBuffer::append(Vector<uint8_t>&& data)
{
    ASSERT(!m_hasBeenCombinedIntoOneSegment);
    auto dataSize = data.size();
    m_segments.append({m_size, DataSegment::create(WTFMove(data))});
    m_size += dataSize;
    ASSERT(internallyConsistent());
}

void SharedBuffer::clear()
{
    m_size = 0;
    m_segments.clear();
    ASSERT(internallyConsistent());
}

Ref<SharedBuffer> SharedBuffer::copy() const
{
    Ref<SharedBuffer> clone = adoptRef(*new SharedBuffer);
    clone->m_size = m_size;
    clone->m_segments.reserveInitialCapacity(m_segments.size());
    for (const auto& element : m_segments)
        clone->m_segments.uncheckedAppend({element.beginPosition, element.segment.copyRef()});
    ASSERT(clone->internallyConsistent());
    ASSERT(internallyConsistent());
    return clone;
}

void SharedBuffer::forEachSegment(const Function<void(const Span<const uint8_t>&)>& apply) const
{
    for (auto& segment : m_segments)
        apply(Span { segment.segment->data(), segment.segment->size() });
}

bool SharedBuffer::startsWith(const Span<const uint8_t>& prefix) const
{
    if (prefix.empty())
        return true;

    if (size() < prefix.size())
        return false;

    const uint8_t* prefixPtr = prefix.data();
    size_t remaining = prefix.size();
    for (auto& segment : m_segments) {
        size_t amountToCompareThisTime = std::min(remaining, segment.segment->size());
        if (memcmp(prefixPtr, segment.segment->data(), amountToCompareThisTime))
            return false;
        remaining -= amountToCompareThisTime;
        if (!remaining)
            return true;
        prefixPtr += amountToCompareThisTime;
    }
    return false;
}

void SharedBuffer::copyTo(void* destination, size_t length) const
{
    ASSERT(length <= size());
    auto destinationPtr = static_cast<uint8_t*>(destination);
    auto remaining = std::min(length, size());
    for (auto& segment : m_segments) {
        size_t amountToCopyThisTime = std::min(remaining, segment.segment->size());
        memcpy(destinationPtr, segment.segment->data(), amountToCopyThisTime);
        remaining -= amountToCopyThisTime;
        if (!remaining)
            return;
        destinationPtr += amountToCopyThisTime;
    }
}

bool SharedBuffer::hasOneSegment() const
{
    auto it = begin();
    return it != end() && ++it == end();
}

#if ASSERT_ENABLED
bool SharedBuffer::internallyConsistent() const
{
    size_t position = 0;
    for (const auto& element : m_segments) {
        if (element.beginPosition != position)
            return false;
        position += element.segment->size();
    }
    return position == m_size;
}
#endif // ASSERT_ENABLED

const uint8_t* SharedBuffer::DataSegment::data() const
{
    auto visitor = WTF::makeVisitor(
        [](const Vector<uint8_t>& data) { return data.data(); },
#if USE(CF)
        [](const RetainPtr<CFDataRef>& data) { return CFDataGetBytePtr(data.get()); },
#endif
#if USE(GLIB)
        [](const GRefPtr<GBytes>& data) { return static_cast<const uint8_t*>(g_bytes_get_data(data.get(), nullptr)); },
#endif
#if USE(GSTREAMER)
        [](const RefPtr<GstMappedOwnedBuffer>& data) { return data->data(); },
#endif
        [](const FileSystem::MappedFileData& data) { return static_cast<const uint8_t*>(data.data()); }
    );
    return WTF::visit(visitor, m_immutableData);
}

bool SharedBuffer::DataSegment::containsMappedFileData() const
{
    return WTF::holds_alternative<FileSystem::MappedFileData>(m_immutableData);
}

#if !USE(CF)
void SharedBuffer::hintMemoryNotNeededSoon() const
{
}
#endif

WTF::Persistence::Decoder SharedBuffer::decoder() const
{
    return {{ reinterpret_cast<const uint8_t*>(data()), size() }};
}

bool SharedBuffer::operator==(const SharedBuffer& other) const
{
    if (this == &other)
        return true;

    if (m_size != other.m_size)
        return false;

    auto thisIterator = begin();
    size_t thisOffset = 0;
    auto otherIterator = other.begin();
    size_t otherOffset = 0;

    while (thisIterator != end() && otherIterator != other.end()) {
        auto& thisSegment = thisIterator->segment.get();
        auto& otherSegment = otherIterator->segment.get();

        if (&thisSegment == &otherSegment && !thisOffset && !otherOffset) {
            ++thisIterator;
            ++otherIterator;
            continue;
        }

        ASSERT(thisOffset <= thisSegment.size());
        ASSERT(otherOffset <= otherSegment.size());

        size_t thisRemaining = thisSegment.size() - thisOffset;
        size_t otherRemaining = otherSegment.size() - otherOffset;
        size_t remaining = std::min(thisRemaining, otherRemaining);

        if (memcmp(thisSegment.data() + thisOffset, otherSegment.data() + otherOffset, remaining))
            return false;

        thisOffset += remaining;
        otherOffset += remaining;

        if (thisOffset == thisSegment.size()) {
            ++thisIterator;
            thisOffset = 0;
        }

        if (otherOffset == otherSegment.size()) {
            ++otherIterator;
            otherOffset = 0;
        }
    }
    return true;
}

size_t SharedBuffer::DataSegment::size() const
{
    auto visitor = WTF::makeVisitor(
        [](const Vector<uint8_t>& data) { return data.size(); },
#if USE(CF)
        [](const RetainPtr<CFDataRef>& data) { return CFDataGetLength(data.get()); },
#endif
#if USE(GLIB)
        [](const GRefPtr<GBytes>& data) { return g_bytes_get_size(data.get()); },
#endif
#if USE(GSTREAMER)
        [](const RefPtr<GstMappedOwnedBuffer>& data) { return data->size(); },
#endif
        [](const FileSystem::MappedFileData& data) { return data.size(); }
    );
    return WTF::visit(visitor, m_immutableData);
}

SharedBufferDataView::SharedBufferDataView(Ref<SharedBuffer::DataSegment>&& segment, size_t positionWithinSegment)
    : m_positionWithinSegment(positionWithinSegment)
    , m_segment(WTFMove(segment))
{
    RELEASE_ASSERT(positionWithinSegment < m_segment->size());
}

size_t SharedBufferDataView::size() const
{
    return m_segment->size() - m_positionWithinSegment;
}

const uint8_t* SharedBufferDataView::data() const
{
    return m_segment->data() + m_positionWithinSegment;
}

RefPtr<SharedBuffer> utf8Buffer(const String& string)
{
    // Allocate a buffer big enough to hold all the characters.
    const size_t length = string.length();
    if constexpr (String::MaxLength > std::numeric_limits<size_t>::max() / 3) {
        if (length > std::numeric_limits<size_t>::max() / 3)
            return nullptr;
    }

    Vector<uint8_t> buffer(length * 3);

    // Convert to runs of 8-bit characters.
    char* p = reinterpret_cast<char*>(buffer.data());
    if (length) {
        if (string.is8Bit()) {
            const LChar* d = string.characters8();
            if (!WTF::Unicode::convertLatin1ToUTF8(&d, d + length, &p, p + buffer.size()))
                return nullptr;
        } else {
            const UChar* d = string.characters16();
            if (WTF::Unicode::convertUTF16ToUTF8(&d, d + length, &p, p + buffer.size()) != WTF::Unicode::ConversionOK)
                return nullptr;
        }
    }

    buffer.shrink(p - reinterpret_cast<char*>(buffer.data()));
    return SharedBuffer::create(WTFMove(buffer));
}

} // namespace WebCore
