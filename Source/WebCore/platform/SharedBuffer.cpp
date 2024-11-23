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

#include "SharedMemory.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <algorithm>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/UTF8Conversion.h>

#if USE(CF)
#include <wtf/cf/VectorCF.h>
#endif

static constexpr size_t minimumPageSize = 4096;
#if USE(UNIX_DOMAIN_SOCKETS)
static constexpr bool useUnixDomainSockets = true;
#else
static constexpr bool useUnixDomainSockets = false;
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SharedBufferBuilder);

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create()
{
    return adoptRef(*new FragmentedSharedBuffer);
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(std::span<const uint8_t> data)
{
    return adoptRef(*new FragmentedSharedBuffer(data));
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(FileSystem::MappedFileData&& mappedFileData)
{
    return adoptRef(*new FragmentedSharedBuffer(WTFMove(mappedFileData)));
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(Ref<SharedBuffer>&& buffer)
{
    return adoptRef(*new FragmentedSharedBuffer(WTFMove(buffer)));
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(Vector<uint8_t>&& vector)
{
    return adoptRef(*new FragmentedSharedBuffer(WTFMove(vector)));
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(DataSegment::Provider&& provider)
{
    return adoptRef(*new FragmentedSharedBuffer(WTFMove(provider)));
}

std::optional<Ref<FragmentedSharedBuffer>> FragmentedSharedBuffer::fromIPCData(IPCData&& ipcData)
{
    return WTF::switchOn(WTFMove(ipcData), [](Vector<std::span<const uint8_t>>&& data) -> std::optional<Ref<FragmentedSharedBuffer>> {
        if (!data.size())
            return SharedBuffer::create();

        CheckedSize size = 0;
        for (auto span : data)
            size += span.size();
        if (size.hasOverflowed())
            return std::nullopt;

        if (useUnixDomainSockets || size < minimumPageSize) {
            if (data.size() == 1)
                return SharedBuffer::create(data[0]);
            Ref sharedMemoryBuffer = FragmentedSharedBuffer::create();
            for (auto span : data)
                sharedMemoryBuffer->append(span);
            return sharedMemoryBuffer;
        }
        return std::nullopt;
    }, [](std::optional<WebCore::SharedMemoryHandle>&& handle) -> std::optional<Ref<FragmentedSharedBuffer>> {
        if (useUnixDomainSockets || !handle.has_value() || handle->size() < minimumPageSize)
            return std::nullopt;

        RefPtr sharedMemoryBuffer = SharedMemory::map(WTFMove(handle.value()), SharedMemory::Protection::ReadOnly);
        if (!sharedMemoryBuffer)
            return std::nullopt;
        return SharedBuffer::create(sharedMemoryBuffer->span());
    });
}

FragmentedSharedBuffer::FragmentedSharedBuffer() = default;

FragmentedSharedBuffer::FragmentedSharedBuffer(FileSystem::MappedFileData&& fileData)
    : m_size(fileData.size())
{
    m_segments.append({ 0, DataSegment::create(WTFMove(fileData)) });
}

FragmentedSharedBuffer::FragmentedSharedBuffer(DataSegment::Provider&& provider)
    : m_size(provider.span().size())
{
    m_segments.append({ 0, DataSegment::create(WTFMove(provider)) });
}

FragmentedSharedBuffer::FragmentedSharedBuffer(Ref<SharedBuffer>&& buffer)
{
    append(WTFMove(buffer));
}

#if USE(GSTREAMER)
Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::create(GstMappedOwnedBuffer& mappedBuffer)
{
    return adoptRef(*new FragmentedSharedBuffer(mappedBuffer));
}

FragmentedSharedBuffer::FragmentedSharedBuffer(GstMappedOwnedBuffer& mappedBuffer)
    : m_size(mappedBuffer.size())
{
    m_segments.append({ 0, DataSegment::create(&mappedBuffer) });
}
#endif

static Vector<uint8_t> combineSegmentsData(const FragmentedSharedBuffer::DataSegmentVector& segments, size_t size)
{
    Vector<uint8_t> combinedData;
    combinedData.reserveInitialCapacity(size);
    for (auto& segment : segments)
        combinedData.append(segment.segment->span());
    ASSERT(combinedData.size() == size);
    return combinedData;
}

Ref<SharedBuffer> FragmentedSharedBuffer::makeContiguous() const
{
    if (m_contiguous)
        return Ref { *static_cast<SharedBuffer*>(const_cast<FragmentedSharedBuffer*>(this)) };
    if (!m_segments.size())
        return SharedBuffer::create();
    if (m_segments.size() == 1)
        return SharedBuffer::create(m_segments[0].segment.copyRef());
    auto combinedData = combineSegmentsData(m_segments, m_size);
    return SharedBuffer::create(WTFMove(combinedData));
}

auto FragmentedSharedBuffer::toIPCData() const -> IPCData
{
    if (useUnixDomainSockets || size() < minimumPageSize) {
        return WTF::map(m_segments, [](auto& segment) {
            return segment.segment->span();
        });
    }

    RefPtr sharedMemoryBuffer = SharedMemory::copyBuffer(*this);
    return sharedMemoryBuffer->createHandle(SharedMemory::Protection::ReadOnly);
}

Vector<uint8_t> FragmentedSharedBuffer::copyData() const
{
    Vector<uint8_t> data;
    data.reserveInitialCapacity(size());
    forEachSegment([&data](auto span) {
        data.append(span);
    });
    return data;
}

Vector<uint8_t> FragmentedSharedBuffer::takeData()
{
    if (m_segments.isEmpty())
        return { };

    Vector<uint8_t> combinedData;
    if (hasOneSegment() && std::holds_alternative<Vector<uint8_t>>(m_segments[0].segment->m_immutableData) && m_segments[0].segment->hasOneRef())
        combinedData = std::exchange(std::get<Vector<uint8_t>>(const_cast<DataSegment&>(m_segments[0].segment.get()).m_immutableData), Vector<uint8_t>());
    else
        combinedData = combineSegmentsData(m_segments, m_size);

    clear();
    return combinedData;
}

SharedBufferDataView FragmentedSharedBuffer::getSomeData(size_t position) const
{
    auto& element = segmentForPosition(position).front();
    return { element.segment.copyRef(), position - element.beginPosition };
}

Ref<SharedBuffer> FragmentedSharedBuffer::getContiguousData(size_t position, size_t length) const
{
    if (position >= m_size)
        return SharedBuffer::create();
    length = std::min(m_size - position, length);
    auto elements = segmentForPosition(position);
    auto& element = elements[0];
    size_t offsetInSegment = position - element.beginPosition;
    ASSERT(element.segment->size() > offsetInSegment);
    if (element.segment->size() - offsetInSegment >= length)
        return SharedBufferDataView { element.segment.copyRef(), offsetInSegment, length }.createSharedBuffer();
    Vector<uint8_t> combinedData;
    combinedData.reserveInitialCapacity(length);
    combinedData.append(element.segment->span().subspan(offsetInSegment));

    for (elements = elements.subspan(1); combinedData.size() < length && !elements.empty(); elements = elements.subspan(1)) {
        auto& element = elements[0];
        auto canCopy = std::min(length - combinedData.size(), element.segment->size());
        combinedData.append(element.segment->span().first(canCopy));
    }
    return SharedBuffer::create(WTFMove(combinedData));
}

std::span<const FragmentedSharedBuffer::DataSegmentVectorEntry> FragmentedSharedBuffer::segmentForPosition(size_t position) const
{
    RELEASE_ASSERT(position < m_size);
    auto comparator = [](const size_t& position, const DataSegmentVectorEntry& entry) {
        return position < entry.beginPosition;
    };
    auto* element = std::upper_bound(m_segments.begin(), m_segments.end(), position, comparator);
    // std::upper_bound gives a pointer to the element that is greater than position. We want the element just before that.
    return m_segments.subspan(element - m_segments.begin() - 1);
}

String FragmentedSharedBuffer::toHexString() const
{
    StringBuilder stringBuilder;
    forEachSegment([&](auto segment) {
        for (auto byte : segment)
            stringBuilder.append(pad('0', 2, hex(byte)));
    });
    return stringBuilder.toString();
}

RefPtr<ArrayBuffer> FragmentedSharedBuffer::tryCreateArrayBuffer() const
{
    // FIXME: This check is no longer needed to avoid integer truncation. Consider removing it.
    if (size() > std::numeric_limits<unsigned>::max()) {
        WTFLogAlways("SharedBuffer::tryCreateArrayBuffer Unable to create buffer. Requested size is too large (%zu)\n", size());
        return nullptr;
    }
    auto arrayBuffer = ArrayBuffer::tryCreateUninitialized(size(), 1);
    if (!arrayBuffer) {
        WTFLogAlways("SharedBuffer::tryCreateArrayBuffer Unable to create buffer. Requested size was %zu\n", size());
        return nullptr;
    }
    copyTo(arrayBuffer->mutableSpan());
    ASSERT(internallyConsistent());
    return arrayBuffer;
}

void FragmentedSharedBuffer::append(const FragmentedSharedBuffer& data)
{
    ASSERT(!m_contiguous);
    m_segments.appendContainerWithMapping(data.m_segments, [&](auto& element) {
        DataSegmentVectorEntry entry { m_size, element.segment.copyRef() };
        m_size += element.segment->size();
        return entry;
    });
    ASSERT(internallyConsistent());
}

void FragmentedSharedBuffer::append(std::span<const uint8_t> data)
{
    ASSERT(!m_contiguous);
    m_segments.append({ m_size, DataSegment::create(data) });
    m_size += data.size();
    ASSERT(internallyConsistent());
}

void FragmentedSharedBuffer::append(Vector<uint8_t>&& data)
{
    ASSERT(!m_contiguous);
    auto dataSize = data.size();
    m_segments.append({ m_size, DataSegment::create(WTFMove(data)) });
    m_size += dataSize;
    ASSERT(internallyConsistent());
}

void FragmentedSharedBuffer::clear()
{
    m_size = 0;
    m_segments.clear();
    ASSERT(internallyConsistent());
}

Ref<FragmentedSharedBuffer> FragmentedSharedBuffer::copy() const
{
    if (m_contiguous)
        return m_segments.size() ? SharedBuffer::create(m_segments[0].segment.copyRef()) : SharedBuffer::create();
    Ref<FragmentedSharedBuffer> clone = adoptRef(*new FragmentedSharedBuffer);
    clone->m_size = m_size;
    clone->m_segments = WTF::map<1>(m_segments, [](auto& element) {
        return DataSegmentVectorEntry { element.beginPosition, element.segment.copyRef() };
    });
    ASSERT(clone->internallyConsistent());
    ASSERT(internallyConsistent());
    return clone;
}

void FragmentedSharedBuffer::forEachSegment(const Function<void(std::span<const uint8_t>)>& apply) const
{
    auto segments = m_segments;
    for (auto& segment : segments)
        segment.segment->iterate(apply);
}

void DataSegment::iterate(const Function<void(std::span<const uint8_t>)>& apply) const
{
#if USE(FOUNDATION)
    if (auto* data = std::get_if<RetainPtr<CFDataRef>>(&m_immutableData))
        return iterate(data->get(), apply);
#endif
    apply(span());
}

void FragmentedSharedBuffer::forEachSegmentAsSharedBuffer(const Function<void(Ref<SharedBuffer>&&)>& apply) const
{
    auto protectedThis = Ref { *this };
    for (auto& segment : m_segments)
        apply(SharedBuffer::create(segment.segment.copyRef()));
}

bool FragmentedSharedBuffer::startsWith(std::span<const uint8_t> prefix) const
{
    if (prefix.empty())
        return true;

    if (size() < prefix.size())
        return false;

    size_t remaining = prefix.size();
    for (auto& segment : m_segments) {
        size_t amountToCompareThisTime = std::min(remaining, segment.segment->size());
        if (!equalSpans(prefix.first(amountToCompareThisTime), segment.segment->span().first(amountToCompareThisTime)))
            return false;
        remaining -= amountToCompareThisTime;
        if (!remaining)
            return true;
        prefix = prefix.subspan(amountToCompareThisTime);
    }
    return false;
}

Vector<uint8_t> FragmentedSharedBuffer::read(size_t offset, size_t length) const
{
    Vector<uint8_t> data;
    if (offset >= size())
        return data;
    auto remaining = std::min(length, size() - offset);
    if (!remaining)
        return data;

    data.reserveInitialCapacity(remaining);
    auto segments = segmentForPosition(offset);
    auto& currentSegment = segments[0];
    size_t offsetInSegment = offset - currentSegment.beginPosition;
    size_t availableInSegment = std::min(currentSegment.segment->size() - offsetInSegment, remaining);
    data.append(currentSegment.segment->span().subspan(offsetInSegment, availableInSegment));

    remaining -= availableInSegment;

    while (remaining) {
        segments = segments.subspan(1);
        if (segments.empty())
            break;
        auto& currentSegment = segments[0];
        size_t lengthInSegment = std::min(currentSegment.segment->size(), remaining);
        data.append(currentSegment.segment->span().first(lengthInSegment));
        remaining -= lengthInSegment;
    }
    return data;
}

void FragmentedSharedBuffer::copyTo(std::span<uint8_t> destination) const
{
    return copyTo(destination, 0);
}

void FragmentedSharedBuffer::copyTo(std::span<uint8_t> destination, size_t offset) const
{
    if (offset >= size())
        return;
    auto remaining = std::min(destination.size(), size() - offset);
    if (!remaining)
        return;

    auto segments = m_segments.span();
    if (offset >= segments[0].segment->size()) {
        auto comparator = [](const size_t& position, const DataSegmentVectorEntry& entry) {
            return position < entry.beginPosition;
        };
        auto* segment = std::upper_bound(m_segments.begin(), m_segments.end(), offset, comparator);
        // std::upper_bound gives a pointer to the segment that is greater than offset. We want the segment just before that.
        segments = segments.subspan(segment - m_segments.begin() - 1);
    }

    auto& segment = segments[0];
    size_t positionInSegment = offset - segment.beginPosition;
    size_t amountToCopyThisTime = std::min(remaining, segment.segment->size() - positionInSegment);
    memcpySpan(destination, segment.segment->span().subspan(positionInSegment, amountToCopyThisTime));
    remaining -= amountToCopyThisTime;
    if (!remaining)
        return;
    destination = destination.subspan(amountToCopyThisTime);

    // If we reach here, there must be at least another segment available as we have content left to be fetched.
    for (segments = segments.subspan(1); !segments.empty(); segments = segments.subspan(1)) {
        auto& segment = segments[0];
        size_t amountToCopyThisTime = std::min(remaining, segment.segment->size());
        memcpySpan(destination, segment.segment->span().first(amountToCopyThisTime));
        remaining -= amountToCopyThisTime;
        if (!remaining)
            return;
        destination = destination.subspan(amountToCopyThisTime);
    }
}

#if ASSERT_ENABLED
bool FragmentedSharedBuffer::internallyConsistent() const
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

#if !USE(CF)
void FragmentedSharedBuffer::hintMemoryNotNeededSoon() const
{
}
#endif

bool FragmentedSharedBuffer::operator==(const FragmentedSharedBuffer& other) const
{
    if (this == &other)
        return true;

    if (m_size != other.m_size)
        return false;

    auto thisSpan = m_segments.span();
    size_t thisOffset = 0;
    auto otherSpan = other.m_segments.span();
    size_t otherOffset = 0;

    while (!thisSpan.empty() && !otherSpan.empty()) {
        auto& thisSegment = thisSpan[0].segment.get();
        auto& otherSegment = otherSpan[0].segment.get();

        if (&thisSegment == &otherSegment && !thisOffset && !otherOffset) {
            thisSpan = thisSpan.subspan(1);
            otherSpan = otherSpan.subspan(1);
            continue;
        }

        ASSERT(thisOffset <= thisSegment.size());
        ASSERT(otherOffset <= otherSegment.size());

        size_t thisRemaining = thisSegment.size() - thisOffset;
        size_t otherRemaining = otherSegment.size() - otherOffset;
        size_t remaining = std::min(thisRemaining, otherRemaining);

        if (!equalSpans(thisSegment.span().subspan(thisOffset, remaining), otherSegment.span().subspan(otherOffset, remaining)))
            return false;

        thisOffset += remaining;
        otherOffset += remaining;

        if (thisOffset == thisSegment.size()) {
            thisSpan = thisSpan.subspan(1);
            thisOffset = 0;
        }

        if (otherOffset == otherSegment.size()) {
            otherSpan = otherSpan.subspan(1);
            otherOffset = 0;
        }
    }
    return true;
}

SharedBuffer::SharedBuffer()
{
    m_contiguous = true;
}

SharedBuffer::SharedBuffer(Ref<const DataSegment>&& segment)
{
    m_size = segment->size();
    m_segments.append({ 0, WTFMove(segment) });
    m_contiguous = true;
}

SharedBuffer::SharedBuffer(Ref<FragmentedSharedBuffer>&& contiguousBuffer)
{
    ASSERT(contiguousBuffer->hasOneSegment() || contiguousBuffer->isEmpty());
    m_size = contiguousBuffer->size();
    if (contiguousBuffer->hasOneSegment())
        m_segments.append({ 0, contiguousBuffer->m_segments[0].segment.copyRef() });
    m_contiguous = true;
}

SharedBuffer::SharedBuffer(FileSystem::MappedFileData&& data)
    : FragmentedSharedBuffer(WTFMove(data))
{
    m_contiguous = true;
}

RefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath, FileSystem::MappedFileMode mappedFileMode, MayUseFileMapping mayUseFileMapping)
{
    if (mayUseFileMapping == MayUseFileMapping::Yes) {
        bool mappingSuccess;
        FileSystem::MappedFileData mappedFileData(filePath, mappedFileMode, mappingSuccess);
        if (mappingSuccess)
            return adoptRef(new SharedBuffer(WTFMove(mappedFileData)));
    }

    auto buffer = FileSystem::readEntireFile(filePath);
    if (!buffer)
        return nullptr;

    return SharedBuffer::create(WTFMove(*buffer));
}

std::span<const uint8_t> SharedBuffer::span() const
{
    if (m_segments.isEmpty())
        return { };
    return m_segments[0].segment->span();
}

const uint8_t& SharedBuffer::operator[](size_t i) const
{
    RELEASE_ASSERT(i < size() && !m_segments.isEmpty());
    return m_segments[0].segment->span()[i];
}

WTF::Persistence::Decoder SharedBuffer::decoder() const
{
    return { span() };
}

Ref<DataSegment> DataSegment::create(Vector<uint8_t>&& data)
{
    data.shrinkToFit();
    return adoptRef(*new DataSegment(WTFMove(data)));
}

#if USE(CF)
Ref<DataSegment> DataSegment::create(RetainPtr<CFDataRef>&& data)
{
    return adoptRef(*new DataSegment(WTFMove(data)));
}
#endif

#if USE(GLIB)
Ref<DataSegment> DataSegment::create(GRefPtr<GBytes>&& data)
{
    return adoptRef(*new DataSegment(WTFMove(data)));
}
#endif

#if USE(GSTREAMER)
Ref<DataSegment> DataSegment::create(RefPtr<GstMappedOwnedBuffer>&& data)
{
    return adoptRef(*new DataSegment(WTFMove(data)));
}
#endif

#if USE(SKIA)
Ref<DataSegment> DataSegment::create(sk_sp<SkData>&& data)
{
    return adoptRef(*new DataSegment(WTFMove(data)));
}
#endif

Ref<DataSegment> DataSegment::create(FileSystem::MappedFileData&& data)
{
    return adoptRef(*new DataSegment(WTFMove(data)));
}

Ref<DataSegment> DataSegment::create(Provider&& provider)
{
    return adoptRef(*new DataSegment(WTFMove(provider)));
}

std::span<const uint8_t> DataSegment::span() const
{
    auto visitor = WTF::makeVisitor(
        [](const Vector<uint8_t>& data) { return data.span(); },
#if USE(CF)
        [](const RetainPtr<CFDataRef>& data) { return WTF::span(data.get()); },
#endif
#if USE(GLIB)
        [](const GRefPtr<GBytes>& data) -> std::span<const uint8_t> { return { static_cast<const uint8_t*>(g_bytes_get_data(data.get(), nullptr)), g_bytes_get_size(data.get()) }; },
#endif
#if USE(GSTREAMER)
        [](const RefPtr<GstMappedOwnedBuffer>& data) -> std::span<const uint8_t> { return { data->data(), data->size() }; },
#endif
#if USE(SKIA)
        [](const sk_sp<SkData>& data) -> std::span<const uint8_t> { return { data->bytes(), data->size() }; },
#endif
        [](const FileSystem::MappedFileData& data) { return data.span(); },
        [](const Provider& provider) { return provider.span(); }
    );
    return std::visit(visitor, m_immutableData);
}

bool DataSegment::containsMappedFileData() const
{
    return std::holds_alternative<FileSystem::MappedFileData>(m_immutableData);
}

SharedBufferBuilder::SharedBufferBuilder(RefPtr<FragmentedSharedBuffer>&& buffer)
{
    if (!buffer)
        return;
    initialize(buffer.releaseNonNull());
}

SharedBufferBuilder& SharedBufferBuilder::operator=(RefPtr<FragmentedSharedBuffer>&& buffer)
{
    if (!buffer) {
        m_buffer = nullptr;
        return *this;
    }
    m_buffer = nullptr;
    initialize(buffer.releaseNonNull());
    return *this;
}

void SharedBufferBuilder::initialize(Ref<FragmentedSharedBuffer>&& buffer)
{
    ASSERT(!m_buffer);
    // We do not want to take a reference to the SharedBuffer as all SharedBuffer should be immutable
    // once created.
    if (buffer->hasOneRef() && !buffer->isContiguous()) {
        m_buffer = WTFMove(buffer);
        return;
    }
    append(buffer);
}

RefPtr<ArrayBuffer> SharedBufferBuilder::tryCreateArrayBuffer() const
{
    return m_buffer ? m_buffer->tryCreateArrayBuffer() : ArrayBuffer::tryCreate();
}

Ref<FragmentedSharedBuffer> SharedBufferBuilder::take()
{
    return m_buffer ? m_buffer.releaseNonNull() : FragmentedSharedBuffer::create();
}

Ref<SharedBuffer> SharedBufferBuilder::takeAsContiguous()
{
    return take()->makeContiguous();
}

RefPtr<ArrayBuffer> SharedBufferBuilder::takeAsArrayBuffer()
{
    if (!m_buffer)
        return ArrayBuffer::tryCreate();
    return take()->tryCreateArrayBuffer();
}

void SharedBufferBuilder::ensureBuffer()
{
    if (!m_buffer)
        m_buffer = FragmentedSharedBuffer::create();
}

SharedBufferDataView::SharedBufferDataView(Ref<const DataSegment>&& segment, size_t positionWithinSegment, std::optional<size_t> size)
    : m_segment(WTFMove(segment))
    , m_positionWithinSegment(positionWithinSegment)
    , m_size(size ? *size : m_segment->size() - positionWithinSegment)
{
    RELEASE_ASSERT(m_positionWithinSegment < m_segment->size());
    RELEASE_ASSERT(m_size <= m_segment->size() - m_positionWithinSegment);
}

SharedBufferDataView::SharedBufferDataView(const SharedBufferDataView& other, size_t newSize)
    : SharedBufferDataView(other.m_segment.copyRef(), other.m_positionWithinSegment, newSize)
{
}

Ref<SharedBuffer> SharedBufferDataView::createSharedBuffer() const
{
    return SharedBuffer::create(DataSegment::Provider {
        [segment = m_segment, data = span()]() { return data; }
    });
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
    WTF::Unicode::ConversionResult<char8_t> result;
    if (length) {
        if (string.is8Bit())
            result = WTF::Unicode::convert(string.span8(), spanReinterpretCast<char8_t>(buffer.mutableSpan()));
        else
            result = WTF::Unicode::convert(string.span16(), spanReinterpretCast<char8_t>(buffer.mutableSpan()));
        if (result.code != WTF::Unicode::ConversionResultCode::Success)
            return nullptr;
    }

    buffer.shrink(result.buffer.size());
    return SharedBuffer::create(WTFMove(buffer));
}

Ref<SharedBuffer> SharedBuffer::create(Ref<FragmentedSharedBuffer>&& fragmentedBuffer)
{
    return fragmentedBuffer->makeContiguous();
}

} // namespace WebCore
