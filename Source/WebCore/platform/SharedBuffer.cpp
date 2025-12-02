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
#include <wtf/text/ParsingUtilities.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/UTF8Conversion.h>

#if USE(CF)
#include <wtf/cf/VectorCF.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GSpanExtras.h>
#endif

#if USE(SKIA)
#include "SkiaSpanExtras.h"
#endif

static constexpr size_t minimumPageSize = 4096;
#if USE(UNIX_DOMAIN_SOCKETS)
static constexpr bool useUnixDomainSockets = true;
#else
static constexpr bool useUnixDomainSockets = false;
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SharedBufferBuilder);

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
            SharedBufferBuilder builder;
            builder.appendSpans(data);
            return builder.take();
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

FragmentedSharedBuffer::FragmentedSharedBuffer(Ref<const DataSegment>&& segment)
    : m_size(segment->size())
    , m_segments(DataSegmentVector::from(DataSegmentVectorEntry { 0, WTFMove(segment) }))
    , m_contiguous(true)
{
}

FragmentedSharedBuffer::FragmentedSharedBuffer(size_t size, const DataSegmentVector& segments)
    : m_size(size)
    , m_segments(WTF::map<1>(segments, [](auto& element) {
        return DataSegmentVectorEntry { element.beginPosition, element.segment.copyRef() };
    }))
{
    ASSERT(internallyConsistent());
}

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
    if (RefPtr sharedBuffer = dynamicDowncast<SharedBuffer>(*const_cast<FragmentedSharedBuffer*>(this)))
        return sharedBuffer.releaseNonNull();
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
#if PLATFORM(COCOA)
    if (m_segments.size() == 1) {
        Ref segment = m_segments[0].segment;
        if (segment->containsMappedFileData())
            return SharedMemoryHandle::createVMShare(segment->span(), SharedMemory::Protection::ReadOnly);
    }
#endif
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
    if (segmentsCount() == 1 && std::holds_alternative<Vector<uint8_t>>(m_segments[0].segment->m_immutableData) && m_segments[0].segment->hasOneRef())
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

    for (skip(elements, 1); combinedData.size() < length && !elements.empty(); skip(elements, 1)) {
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
    return adoptRef(*new FragmentedSharedBuffer(m_size, m_segments));
}

void FragmentedSharedBuffer::forEachSegment(NOESCAPE const Function<void(std::span<const uint8_t>)>& apply) const
{
    auto segments = m_segments;
    for (auto& segment : segments)
        segment.segment->iterate(apply);
}

void DataSegment::iterate(NOESCAPE const Function<void(std::span<const uint8_t>)>& apply) const
{
#if USE(FOUNDATION)
    if (auto* data = std::get_if<RetainPtr<CFDataRef>>(&m_immutableData))
        return iterate(data->get(), apply);
#endif
    apply(span());
}

void FragmentedSharedBuffer::forEachSegmentAsSharedBuffer(NOESCAPE const Function<void(Ref<SharedBuffer>&&)>& apply) const
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
        if (!spanHasPrefix(segment.segment->span(), prefix.first(amountToCompareThisTime)))
            return false;
        remaining -= amountToCompareThisTime;
        if (!remaining)
            return true;
        skip(prefix, amountToCompareThisTime);
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
        skip(segments, 1);
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
        skip(segments, segment - m_segments.begin() - 1);
    }

    auto& segment = segments[0];
    size_t positionInSegment = offset - segment.beginPosition;
    size_t amountToCopyThisTime = std::min(remaining, segment.segment->size() - positionInSegment);
    memcpySpan(destination, segment.segment->span().subspan(positionInSegment, amountToCopyThisTime));
    remaining -= amountToCopyThisTime;
    if (!remaining)
        return;
    skip(destination, amountToCopyThisTime);

    // If we reach here, there must be at least another segment available as we have content left to be fetched.
    for (skip(segments, 1); !segments.empty(); skip(segments, 1)) {
        auto& segment = segments[0];
        size_t amountToCopyThisTime = std::min(remaining, segment.segment->size());
        memcpySpan(destination, segment.segment->span().first(amountToCopyThisTime));
        remaining -= amountToCopyThisTime;
        if (!remaining)
            return;
        skip(destination, amountToCopyThisTime);
    }
}

#if ASSERT_ENABLED
bool FragmentedSharedBuffer::internallyConsistent() const
{
    if (isContiguous() && segmentsCount() > 1)
        return false;
    return internallyConsistent(m_size, m_segments);
}

bool FragmentedSharedBuffer::internallyConsistent(size_t size, const DataSegmentVector& segments)
{
    size_t position = 0;
    for (const auto& element : segments) {
        if (element.beginPosition != position)
            return false;
        position += element.segment->size();
    }
    return position == size;
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

    return haveIdenticalContent(m_segments, other.m_segments);
}

bool FragmentedSharedBuffer::haveIdenticalContent(const DataSegmentVector& a, const DataSegmentVector& b)
{
    // Fast comparison.
    if (a == b)
        return true;

    auto thisSpan = a.span();
    size_t thisOffset = 0;
    auto otherSpan = b.span();
    size_t otherOffset = 0;

    while (!thisSpan.empty() && !otherSpan.empty()) {
        auto& thisSegment = thisSpan[0].segment.get();
        auto& otherSegment = otherSpan[0].segment.get();

        if (&thisSegment == &otherSegment && !thisOffset && !otherOffset) {
            skip(thisSpan, 1);
            skip(otherSpan, 1);
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
            skip(thisSpan, 1);
            thisOffset = 0;
        }

        if (otherOffset == otherSegment.size()) {
            skip(otherSpan, 1);
            otherOffset = 0;
        }
    }
    return true;
}

SharedBuffer::SharedBuffer(Ref<const DataSegment>&& segment)
    : FragmentedSharedBuffer(WTFMove(segment))
{
}

SharedBuffer::SharedBuffer(FileSystem::MappedFileData&& fileData)
    : FragmentedSharedBuffer(DataSegment::create(WTFMove(fileData)))
{
}

SharedBuffer::SharedBuffer(DataSegment::Provider&& provider)
    : FragmentedSharedBuffer(DataSegment::create(WTFMove(provider)))
{
}

SharedBuffer::SharedBuffer(std::span<const uint8_t> data)
    : FragmentedSharedBuffer(DataSegment::create(data))
{
}

SharedBuffer::SharedBuffer(Vector<uint8_t>&& data)
    : FragmentedSharedBuffer(DataSegment::create(WTFMove(data)))
{
}

#if USE(GSTREAMER)
Ref<SharedBuffer> SharedBuffer::create(GstMappedOwnedBuffer& mappedBuffer)
{
    return adoptRef(*new SharedBuffer(mappedBuffer));
}

SharedBuffer::SharedBuffer(GstMappedOwnedBuffer& mappedBuffer)
    : FragmentedSharedBuffer(DataSegment::create(&mappedBuffer))
{
}
#endif

RefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath, FileSystem::MappedFileMode mappedFileMode, MayUseFileMapping mayUseFileMapping)
{
    if (mayUseFileMapping == MayUseFileMapping::Yes) {
        if (auto mappedFileData = FileSystem::mapFile(filePath, mappedFileMode))
            return adoptRef(new SharedBuffer(WTFMove(*mappedFileData)));
    }

    auto buffer = FileSystem::readEntireFile(filePath);
    if (!buffer)
        return nullptr;

    return SharedBuffer::create(WTFMove(*buffer));
}

std::span<const uint8_t> SharedBuffer::span() const
{
    if (!segmentsCount())
        return { };
    return segments()[0].segment->span();
}

uint8_t SharedBuffer::operator[](size_t i) const
{
    return segments()[0].segment->span()[i];
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
        [](const GRefPtr<GBytes>& data) -> std::span<const uint8_t> { return WTF::span(data); },
#endif
#if USE(GSTREAMER)
        [](const RefPtr<GstMappedOwnedBuffer>& data) -> std::span<const uint8_t> { return data->span<uint8_t>(); },
#endif
#if USE(SKIA)
        [](const sk_sp<SkData>& data) -> std::span<const uint8_t> { return WebCore::span(data); },
#endif
        [](const FileSystem::MappedFileData& data) { return data.span(); },
        [](const Provider& provider) { return provider.span(); }
    );
    return WTF::visit(visitor, m_immutableData);
}

bool DataSegment::containsMappedFileData() const
{
    return std::holds_alternative<FileSystem::MappedFileData>(m_immutableData);
}

RefPtr<ArrayBuffer> SharedBufferBuilder::tryCreateArrayBuffer() const
{
    if (isEmpty())
        return ArrayBuffer::tryCreate();
    updateBufferIfNeeded();
    return RefPtr { m_buffer }->tryCreateArrayBuffer();
}

Ref<FragmentedSharedBuffer> SharedBufferBuilder::take()
{
    if (isEmpty())
        return SharedBuffer::create();
    updateBufferIfNeeded();
    Ref buffer = m_buffer.releaseNonNull();
    reset();
    return buffer;
}

Ref<SharedBuffer> SharedBufferBuilder::takeAsContiguous()
{
    return take()->makeContiguous();
}

RefPtr<ArrayBuffer> SharedBufferBuilder::takeAsArrayBuffer()
{
    if (isEmpty())
        return ArrayBuffer::tryCreate();
    return take()->tryCreateArrayBuffer();
}

void SharedBufferBuilder::updateBufferIfNeeded() const
{
    if (m_state == State::Null) {
        ASSERT(!m_buffer);
        return;
    }
    if (m_state == State::Fresh)
        return;
    m_buffer = createBuffer();
    m_state = State::Fresh;
}

Ref<FragmentedSharedBuffer> SharedBufferBuilder::createBuffer() const
{
    if (isEmpty())
        return SharedBuffer::create();
    if (m_segments.size() == 1)
        return SharedBuffer::create(m_segments[0].segment.copyRef());
    return adoptRef(*new FragmentedSharedBuffer(m_size, m_segments));
}

void SharedBufferBuilder::appendDataSegment(Ref<DataSegment>&& segment)
{
    m_state = State::Stale;
    size_t size = segment->size();
    m_segments.append(SharedBuffer::DataSegmentVectorEntry { m_size, WTFMove(segment) });
    m_size += size;
}

void SharedBufferBuilder::append(const FragmentedSharedBuffer& data)
{
    m_state = State::Stale;
    m_segments.appendContainerWithMapping(data.m_segments, [&](auto& element) {
        SharedBuffer::DataSegmentVectorEntry entry { m_size, element.segment.copyRef() };
        m_size += element.segment->size();
        return entry;
    });
    ASSERT(FragmentedSharedBuffer::internallyConsistent(m_size, m_segments));
}

void SharedBufferBuilder::append(std::span<const uint8_t> data)
{
    if (data.size())
        appendDataSegment(DataSegment::create(data));
}

void SharedBufferBuilder::append(Vector<uint8_t>&& data)
{
    if (data.size())
        appendDataSegment(DataSegment::create(WTFMove(data)));
}

void SharedBufferBuilder::appendSpans(const Vector<std::span<const uint8_t>>& spans)
{
    m_state = State::Stale;
    m_segments.appendContainerWithMapping(spans, [&](auto& span) {
        SharedBuffer::DataSegmentVectorEntry entry { m_size, DataSegment::create(span) };
        m_size += span.size();
        return entry;
    });
    ASSERT(FragmentedSharedBuffer::internallyConsistent(m_size, m_segments));
}

SharedBufferBuilder::SharedBufferBuilder(const SharedBufferBuilder& other)
    : m_state(other.m_state)
    , m_buffer(other.m_buffer)
    , m_size(other.m_size)
    , m_segments(WTF::map<1>(other.m_segments, [](auto& element) {
        return SharedBuffer::DataSegmentVectorEntry { element.beginPosition, element.segment.copyRef() };
    }))
{
}

SharedBufferBuilder& SharedBufferBuilder::operator=(const SharedBufferBuilder& other)
{
    reset();
    m_state = other.m_state;
    m_buffer = other.m_buffer;
    m_size = other.m_size;
    m_segments.appendContainerWithMapping(other.m_segments, [&](auto& element) {
        return SharedBuffer::DataSegmentVectorEntry { element.beginPosition, element.segment.copyRef() };
    });
    return *this;
}

bool SharedBufferBuilder::operator==(const SharedBufferBuilder& other) const
{
    if (this == &other)
        return true;

    if (m_size != other.m_size)
        return false;

    return FragmentedSharedBuffer::haveIdenticalContent(m_segments, other.m_segments);
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

} // namespace WebCore
