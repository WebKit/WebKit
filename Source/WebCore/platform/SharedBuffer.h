/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
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

#include <JavaScriptCore/Forward.h>
#include <span>
#include <utility>
#include <variant>
#include <wtf/FileSystem.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/MappedFileData.h>
#include <wtf/RawPtrTraits.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/TypeCasts.h>
#include <wtf/TypeTraits.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#if USE(GLIB)
#include <wtf/glib/GRefPtr.h>
typedef struct _GBytes GBytes;
#endif

#if USE(GSTREAMER)
#include "GStreamerCommon.h"
#endif

#if USE(FOUNDATION)
OBJC_CLASS NSArray;
OBJC_CLASS NSData;
typedef struct OpaqueCMBlockBuffer* CMBlockBufferRef;
#endif

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkData.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WTF {
namespace Persistence {
class Decoder;
}
}

namespace WebCore {

class SharedBuffer;
class SharedBufferDataView;
class SharedMemoryHandle;

// Data wrapped by a DataSegment should be immutable because it can be referenced by other objects.
// To modify or combine the data, allocate a new DataSegment.
class DataSegment : public ThreadSafeRefCounted<DataSegment> {
public:
    size_t size() const { return span().size(); }
    WEBCORE_EXPORT std::span<const uint8_t> span() const LIFETIME_BOUND;

    WEBCORE_EXPORT static Ref<DataSegment> create(Vector<uint8_t>&&);

#if USE(CF)
    WEBCORE_EXPORT static Ref<DataSegment> create(RetainPtr<CFDataRef>&&);
#endif
#if USE(GLIB)
    WEBCORE_EXPORT static Ref<DataSegment> create(GRefPtr<GBytes>&&);
#endif
#if USE(GSTREAMER)
    WEBCORE_EXPORT static Ref<DataSegment> create(RefPtr<GstMappedOwnedBuffer>&&);
#endif
#if USE(SKIA)
    WEBCORE_EXPORT static Ref<DataSegment> create(sk_sp<SkData>&&);
#endif
    WEBCORE_EXPORT static Ref<DataSegment> create(FileSystem::MappedFileData&&);

    struct Provider {
        Function<std::span<const uint8_t>()> span;
    };
    WEBCORE_EXPORT static Ref<DataSegment> create(Provider&&);

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData() const;
#endif

    WEBCORE_EXPORT bool containsMappedFileData() const;

private:
    void iterate(NOESCAPE const Function<void(std::span<const uint8_t>)>& apply) const;
#if USE(FOUNDATION)
    void iterate(CFDataRef, NOESCAPE const Function<void(std::span<const uint8_t>)>& apply) const;
#endif

    explicit DataSegment(Vector<uint8_t>&& data)
        : m_immutableData(WTFMove(data)) { }
#if USE(CF)
    explicit DataSegment(RetainPtr<CFDataRef>&& data)
        : m_immutableData(WTFMove(data)) { }
#endif
#if USE(GLIB)
    explicit DataSegment(GRefPtr<GBytes>&& data)
        : m_immutableData(WTFMove(data)) { }
#endif
#if USE(GSTREAMER)
    explicit DataSegment(RefPtr<GstMappedOwnedBuffer>&& data)
        : m_immutableData(WTFMove(data)) { }
#endif
#if USE(SKIA)
    explicit DataSegment(sk_sp<SkData>&& data)
        : m_immutableData(WTFMove(data)) { }
#endif
    explicit DataSegment(FileSystem::MappedFileData&& data)
        : m_immutableData(WTFMove(data)) { }
    explicit DataSegment(Provider&& provider)
        : m_immutableData(WTFMove(provider)) { }

    Variant<Vector<uint8_t>,
#if USE(CF)
        RetainPtr<CFDataRef>,
#endif
#if USE(GLIB)
        GRefPtr<GBytes>,
#endif
#if USE(GSTREAMER)
        RefPtr<GstMappedOwnedBuffer>,
#endif
#if USE(SKIA)
        sk_sp<SkData>,
#endif
        FileSystem::MappedFileData,
        Provider> m_immutableData;

    friend class FragmentedSharedBuffer;
    friend class SharedBuffer; // For createCFData
};

class FragmentedSharedBuffer : public ThreadSafeRefCounted<FragmentedSharedBuffer> {
public:
    using IPCData = Variant<std::optional<WebCore::SharedMemoryHandle>, Vector<std::span<const uint8_t>>>;

    WEBCORE_EXPORT static std::optional<Ref<FragmentedSharedBuffer>> fromIPCData(IPCData&&);

    virtual ~FragmentedSharedBuffer() = default;

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSArray> createNSDataArray() const;
    WEBCORE_EXPORT RetainPtr<CMBlockBufferRef> createCMBlockBuffer() const;
#endif

    WEBCORE_EXPORT Vector<uint8_t> copyData() const;
    WEBCORE_EXPORT Vector<uint8_t> read(size_t offset, size_t length) const;

    // Similar to copyData() but avoids copying and will take the data instead when it is safe (The FragmentedSharedBuffer is not shared).
    Vector<uint8_t> extractData();

    WEBCORE_EXPORT RefPtr<ArrayBuffer> tryCreateArrayBuffer() const;

    size_t size() const { return m_size; }
    bool isEmpty() const { return !size(); }
    bool isContiguous() const { return m_contiguous; }

    WEBCORE_EXPORT Ref<FragmentedSharedBuffer> copy() const;
    WEBCORE_EXPORT void copyTo(std::span<uint8_t> destination) const;
    WEBCORE_EXPORT void copyTo(std::span<uint8_t> destination, size_t offset) const;

    WEBCORE_EXPORT void forEachSegment(NOESCAPE const Function<void(std::span<const uint8_t>)>&) const;
    WEBCORE_EXPORT bool startsWith(std::span<const uint8_t> prefix) const;
    WEBCORE_EXPORT void forEachSegmentAsSharedBuffer(NOESCAPE const Function<void(Ref<SharedBuffer>&&)>&) const;

    using DataSegment = WebCore::DataSegment; // To keep backward compatibility when using FragmentedSharedBuffer::DataSegment

    struct DataSegmentVectorEntry {
        size_t beginPosition;
        const Ref<const DataSegment> segment;

        bool operator==(const DataSegmentVectorEntry&) const = default;
    };
    using DataSegmentVector = Vector<DataSegmentVectorEntry, 1>;
    DataSegmentVector::const_iterator begin() const LIFETIME_BOUND { return m_segments.begin(); }
    DataSegmentVector::const_iterator end() const LIFETIME_BOUND { return m_segments.end(); }
    size_t segmentsCount() const { return m_segments.size(); }

    // begin and end take O(1) time, this takes O(log(N)) time.
    WEBCORE_EXPORT SharedBufferDataView getSomeData(size_t position) const;
    WEBCORE_EXPORT Ref<SharedBuffer> getContiguousData(size_t position, size_t length) const;

    WEBCORE_EXPORT String toHexString() const;

    void hintMemoryNotNeededSoon() const;

    WEBCORE_EXPORT bool operator==(const FragmentedSharedBuffer&) const;

    WEBCORE_EXPORT Ref<SharedBuffer> makeContiguous() const;

    WEBCORE_EXPORT IPCData toIPCData() const;

protected:
    enum class Contiguous : bool {
        No,
        Yes
    };
    explicit FragmentedSharedBuffer(Contiguous contiguous)
        : m_contiguous(contiguous == Contiguous::Yes) { }
    // To be used only by SharedBuffer constructor, set m_contiguous to true.
    WEBCORE_EXPORT explicit FragmentedSharedBuffer(Ref<const DataSegment>&&);
    const DataSegmentVector& segments() const { return m_segments; }

private:
    friend class SharedBufferBuilder;

    WEBCORE_EXPORT FragmentedSharedBuffer(size_t, const DataSegmentVector&);

    WEBCORE_EXPORT void clear();

    // Combines all the segments into a Vector and returns that vector after clearing the FragmentedSharedBuffer.
    WEBCORE_EXPORT Vector<uint8_t> takeData();
    std::span<const DataSegmentVectorEntry> segmentForPosition(size_t position) const;

    static bool haveIdenticalContent(const DataSegmentVector&, const DataSegmentVector&);

    size_t m_size { 0 };
    DataSegmentVector m_segments;
    const bool m_contiguous { false };

#if ASSERT_ENABLED
    bool internallyConsistent() const;
    static bool internallyConsistent(size_t, const DataSegmentVector&);
#endif
};

// A SharedBuffer is a FragmentedSharedBuffer that allows to directly access its content via the data() and related methods.
class SharedBuffer : public FragmentedSharedBuffer {
public:
    static Ref<SharedBuffer> create() { return adoptRef(*new SharedBuffer()); }
    static Ref<SharedBuffer> create(Vector<uint8_t>&& vector) { return adoptRef(*new SharedBuffer(WTFMove(vector))); }
    static Ref<SharedBuffer> create(std::span<const uint8_t> data) { return adoptRef(*new SharedBuffer(data)); }
    static Ref<SharedBuffer> create(Ref<const DataSegment>&& segment) { return adoptRef(*new SharedBuffer(WTFMove(segment))); }
    static Ref<SharedBuffer> create(FileSystem::MappedFileData&& mappedFileData) { return adoptRef(*new SharedBuffer(WTFMove(mappedFileData))); }
    static Ref<SharedBuffer> create(DataSegment::Provider&& provider) { return adoptRef(*new SharedBuffer(WTFMove(provider))); }
    static Ref<SharedBuffer> create(Ref<FragmentedSharedBuffer>&& fragmentedBuffer) { return fragmentedBuffer->makeContiguous(); }

#if USE(FOUNDATION)
    WEBCORE_EXPORT static Ref<SharedBuffer> create(NSData*);
#endif
#if USE(CF)
    WEBCORE_EXPORT static Ref<SharedBuffer> create(CFDataRef);
#endif
#if USE(GLIB)
    WEBCORE_EXPORT static Ref<SharedBuffer> create(GBytes*);
#endif
#if USE(GSTREAMER)
    WEBCORE_EXPORT static Ref<SharedBuffer> create(GstMappedOwnedBuffer&);
#endif
#if USE(SKIA)
    WEBCORE_EXPORT static Ref<SharedBuffer> create(sk_sp<SkData>&&);
#endif

    WEBCORE_EXPORT uint8_t operator[](size_t) const;
    WEBCORE_EXPORT std::span<const uint8_t> span() const;
    WTF::Persistence::Decoder decoder() const;

    enum class MayUseFileMapping : bool { No, Yes };
    WEBCORE_EXPORT static RefPtr<SharedBuffer> createWithContentsOfFile(const String& filePath, FileSystem::MappedFileMode = FileSystem::MappedFileMode::Shared, MayUseFileMapping = MayUseFileMapping::Yes);

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData() const;
#endif
#if USE(CF)
    WEBCORE_EXPORT RetainPtr<CFDataRef> createCFData() const;
#endif
#if USE(GLIB)
    WEBCORE_EXPORT GRefPtr<GBytes> createGBytes() const;
#endif
#if USE(SKIA)
    WEBCORE_EXPORT sk_sp<SkData> createSkData() const;
#endif

    Ref<FragmentedSharedBuffer> asFragmentedSharedBuffer() const { return const_cast<SharedBuffer&>(*this); }

private:
    friend class SharedBufferBuilder;

    SharedBuffer()
        : FragmentedSharedBuffer(Contiguous::Yes) { }
    explicit SharedBuffer(const DataSegment& segment)
        : SharedBuffer(Ref<const DataSegment> { segment }) { }
    WEBCORE_EXPORT explicit SharedBuffer(FileSystem::MappedFileData&&);
    WEBCORE_EXPORT explicit SharedBuffer(Ref<const DataSegment>&&);
    WEBCORE_EXPORT explicit SharedBuffer(std::span<const uint8_t> data);
    WEBCORE_EXPORT explicit SharedBuffer(Vector<uint8_t>&& data);
    WEBCORE_EXPORT explicit SharedBuffer(DataSegment::Provider&&);
#if USE(CF)
    WEBCORE_EXPORT explicit SharedBuffer(CFDataRef);
#endif
#if USE(GLIB)
    WEBCORE_EXPORT explicit SharedBuffer(GBytes*);
#endif
#if USE(GSTREAMER)
    WEBCORE_EXPORT explicit SharedBuffer(GstMappedOwnedBuffer&);
#endif
#if USE(SKIA)
    WEBCORE_EXPORT explicit SharedBuffer(sk_sp<SkData>&&);
#endif

    WEBCORE_EXPORT static RefPtr<SharedBuffer> createFromReadingFile(const String& filePath);
};

class SharedBufferBuilder {
    WTF_MAKE_TZONE_ALLOCATED(SharedBufferBuilder);
public:
    SharedBufferBuilder() = default;
    SharedBufferBuilder(SharedBufferBuilder&&) = default;
    SharedBufferBuilder(const FragmentedSharedBuffer& buffer) { append(buffer); }

    template <typename... Args>
    SharedBufferBuilder(std::in_place_t, Args&&... arg)
    {
        m_segments.reserveInitialCapacity(sizeof...(Args));
        (append(std::forward<Args>(arg)), ...);
    }

    SharedBufferBuilder& operator=(SharedBufferBuilder&&) = default;

    WEBCORE_EXPORT void append(const FragmentedSharedBuffer&);
    WEBCORE_EXPORT void append(std::span<const uint8_t>);
    WEBCORE_EXPORT void append(Vector<uint8_t>&&);
    WEBCORE_EXPORT void appendSpans(const Vector<std::span<const uint8_t>>&);
#if USE(FOUNDATION)
    WEBCORE_EXPORT void append(NSData *);
#endif
#if USE(CF)
    WEBCORE_EXPORT void append(CFDataRef);
#endif

    explicit operator bool() const { return !isNull(); }
    bool isNull() const { return m_state == State::Null; }
    bool isEmpty() const { return !size(); }
    size_t size() const { return m_size; }
    bool isContiguous() const { return m_segments.size() <= 1; }
    bool hasOneSegment() const { return m_segments.size() == 1; }
    SharedBuffer::DataSegmentVector::const_iterator begin() const LIFETIME_BOUND { return m_segments.begin(); }
    SharedBuffer::DataSegmentVector::const_iterator end() const LIFETIME_BOUND { return m_segments.end(); }

    void reset()
    {
        empty();
        m_state = State::Null;
    }
    void empty()
    {
        m_state = State::Stale;
        m_segments.clear();
        m_size = 0;
        m_buffer = nullptr;
    }

    RefPtr<FragmentedSharedBuffer> get() const
    {
        updateBufferIfNeeded();
        return m_buffer;
    }
    Ref<FragmentedSharedBuffer> copy() const { return createBuffer(); }

    WEBCORE_EXPORT RefPtr<ArrayBuffer> tryCreateArrayBuffer() const;

    WEBCORE_EXPORT Ref<FragmentedSharedBuffer> take();
    WEBCORE_EXPORT Ref<SharedBuffer> takeAsContiguous();
    WEBCORE_EXPORT RefPtr<ArrayBuffer> takeAsArrayBuffer();

    WEBCORE_EXPORT bool operator==(const SharedBufferBuilder&) const;

private:
    friend class ScriptBuffer;
    friend class FetchBodyConsumer;

    WEBCORE_EXPORT SharedBufferBuilder(const SharedBufferBuilder&);
    WEBCORE_EXPORT SharedBufferBuilder& operator=(const SharedBufferBuilder&);

    WEBCORE_EXPORT void initialize(Ref<FragmentedSharedBuffer>&&);
    WEBCORE_EXPORT void updateBufferIfNeeded() const;
    WEBCORE_EXPORT void appendDataSegment(Ref<DataSegment>&&);
    WEBCORE_EXPORT Ref<FragmentedSharedBuffer> createBuffer() const;

    enum class State {
        Null,
        Stale,
        Fresh
    };
    mutable State m_state { State::Null };
    mutable RefPtr<FragmentedSharedBuffer> m_buffer;
    size_t m_size { 0 };
    SharedBuffer::DataSegmentVector m_segments;
};

inline Vector<uint8_t> FragmentedSharedBuffer::extractData()
{
    if (hasOneRef())
        return takeData();
    return copyData();
}

class SharedBufferDataView {
public:
    WEBCORE_EXPORT SharedBufferDataView(Ref<const DataSegment>&&, size_t positionWithinSegment, std::optional<size_t> newSize = std::nullopt);
    WEBCORE_EXPORT SharedBufferDataView(const SharedBufferDataView&, size_t newSize);
    size_t size() const { return m_size; }
    std::span<const uint8_t> span() const { return { m_segment->span().subspan(m_positionWithinSegment, size()) }; }

    WEBCORE_EXPORT Ref<SharedBuffer> createSharedBuffer() const;
#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData() const;
#endif
private:
    const Ref<const DataSegment> m_segment;
    const size_t m_positionWithinSegment;
    const size_t m_size;
};

RefPtr<SharedBuffer> utf8Buffer(const String&);

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SharedBuffer)
    static bool isType(const WebCore::FragmentedSharedBuffer& buffer) { return buffer.isContiguous(); }
SPECIALIZE_TYPE_TRAITS_END()
