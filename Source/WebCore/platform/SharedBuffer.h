/*
 * Copyright (C) 2006-2019 Apple Inc. All rights reserved.
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
#include <variant>
#include <wtf/FileSystem.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Span.h>
#include <wtf/ThreadSafeRefCounted.h>
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
#endif

namespace WTF {
namespace Persistence {
class Decoder;
}
}

namespace WebCore {

class ContiguousSharedBuffer;
class SharedBufferDataView;

// Data wrapped by a DataSegment should be immutable because it can be referenced by other objects.
// To modify or combine the data, allocate a new DataSegment.
class DataSegment : public ThreadSafeRefCounted<DataSegment> {
public:
    WEBCORE_EXPORT const uint8_t* data() const;
    WEBCORE_EXPORT size_t size() const;

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
    WEBCORE_EXPORT static Ref<DataSegment> create(FileSystem::MappedFileData&&);

    struct Provider {
        Function<const uint8_t*()> data;
        Function<size_t()> size;
    };

    WEBCORE_EXPORT static Ref<DataSegment> create(Provider&&);

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData() const;
#endif

    WEBCORE_EXPORT bool containsMappedFileData() const;

private:
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
    explicit DataSegment(FileSystem::MappedFileData&& data)
        : m_immutableData(WTFMove(data)) { }
    explicit DataSegment(Provider&& provider)
        : m_immutableData(WTFMove(provider)) { }

    std::variant<Vector<uint8_t>,
#if USE(CF)
        RetainPtr<CFDataRef>,
#endif
#if USE(GLIB)
        GRefPtr<GBytes>,
#endif
#if USE(GSTREAMER)
        RefPtr<GstMappedOwnedBuffer>,
#endif
        FileSystem::MappedFileData,
        Provider> m_immutableData;
    friend class SharedBuffer;
    friend class ContiguousSharedBuffer; // For createCFData
};

class SharedBuffer : public ThreadSafeRefCounted<SharedBuffer> {
public:
    WEBCORE_EXPORT static Ref<SharedBuffer> create();
    WEBCORE_EXPORT static Ref<SharedBuffer> create(const uint8_t*, size_t);
    static Ref<SharedBuffer> create(const char* data, size_t size) { return create(reinterpret_cast<const uint8_t*>(data), size); }
    WEBCORE_EXPORT static Ref<SharedBuffer> create(FileSystem::MappedFileData&&);
    WEBCORE_EXPORT static Ref<SharedBuffer> create(Ref<ContiguousSharedBuffer>&&);
    WEBCORE_EXPORT static Ref<SharedBuffer> create(Vector<uint8_t>&&);
    WEBCORE_EXPORT static Ref<SharedBuffer> create(DataSegment::Provider&&);

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSArray> createNSDataArray() const;
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
    WEBCORE_EXPORT Vector<uint8_t> copyData() const;
    WEBCORE_EXPORT Vector<uint8_t> read(size_t offset, size_t length) const;

    // Similar to copyData() but avoids copying and will take the data instead when it is safe (The SharedBuffer is not shared).
    Vector<uint8_t> extractData();

    // Creates an ArrayBuffer and copies this SharedBuffer's contents to that
    // ArrayBuffer without merging segmented buffers into a flat buffer.
    WEBCORE_EXPORT RefPtr<ArrayBuffer> tryCreateArrayBuffer() const;

    size_t size() const { return m_size; }
    bool isEmpty() const { return !size(); }
    bool isContiguous() const { return m_contiguous; }

    WEBCORE_EXPORT void append(const SharedBuffer&);
    WEBCORE_EXPORT void append(const uint8_t*, size_t);
    void append(const char* data, size_t length) { append(reinterpret_cast<const uint8_t*>(data), length); }
    WEBCORE_EXPORT void append(Vector<uint8_t>&&);
#if USE(FOUNDATION)
    WEBCORE_EXPORT void append(NSData *);
#endif
#if USE(CF)
    WEBCORE_EXPORT void append(CFDataRef);
#endif

    WEBCORE_EXPORT void clear();

    WEBCORE_EXPORT Ref<SharedBuffer> copy() const;
    WEBCORE_EXPORT void copyTo(void* destination, size_t length) const;
    WEBCORE_EXPORT void copyTo(void* destination, size_t offset, size_t length) const;

    WEBCORE_EXPORT void forEachSegment(const Function<void(const Span<const uint8_t>&)>&) const;
    WEBCORE_EXPORT bool startsWith(const Span<const uint8_t>& prefix) const;

    using DataSegment = WebCore::DataSegment; // To keep backward compatibility when using SharedBuffer::DataSegment

    struct DataSegmentVectorEntry {
        size_t beginPosition;
        Ref<DataSegment> segment;
    };
    using DataSegmentVector = Vector<DataSegmentVectorEntry, 1>;
    DataSegmentVector::const_iterator begin() const { return m_segments.begin(); }
    DataSegmentVector::const_iterator end() const { return m_segments.end(); }
    bool hasOneSegment() const { return m_segments.size() == 1; }

    // begin and end take O(1) time, this takes O(log(N)) time.
    WEBCORE_EXPORT SharedBufferDataView getSomeData(size_t position) const;

    WEBCORE_EXPORT String toHexString() const;

    void hintMemoryNotNeededSoon() const;

    WEBCORE_EXPORT bool operator==(const SharedBuffer&) const;
    bool operator!=(const SharedBuffer& other) const { return !operator==(other); }

    WEBCORE_EXPORT Ref<ContiguousSharedBuffer> makeContiguous() const;

protected:
    friend class ContiguousSharedBuffer;

    DataSegmentVector m_segments;
    bool m_contiguous { false };

    WEBCORE_EXPORT SharedBuffer();
    explicit SharedBuffer(const uint8_t* data, size_t size) { append(data, size); }
    explicit SharedBuffer(const char* data, size_t size) { append(data, size); }
    explicit SharedBuffer(Vector<uint8_t>&& data) { append(WTFMove(data)); }
    WEBCORE_EXPORT explicit SharedBuffer(FileSystem::MappedFileData&&);
    WEBCORE_EXPORT explicit SharedBuffer(DataSegment::Provider&&);
    WEBCORE_EXPORT explicit SharedBuffer(Ref<ContiguousSharedBuffer>&&);
#if USE(CF)
    WEBCORE_EXPORT explicit SharedBuffer(CFDataRef);
#endif
#if USE(GLIB)
    WEBCORE_EXPORT explicit SharedBuffer(GBytes*);
#endif
#if USE(GSTREAMER)
    WEBCORE_EXPORT explicit SharedBuffer(GstMappedOwnedBuffer&);
#endif
    size_t m_size { 0 };

private:
    // Combines all the segments into a Vector and returns that vector after clearing the SharedBuffer.
    WEBCORE_EXPORT Vector<uint8_t> takeData();
    const DataSegmentVectorEntry* getSegmentForPosition(size_t positition) const;

#if ASSERT_ENABLED
    bool internallyConsistent() const;
#endif
};

// A ContiguousSharedBuffer is a SharedBuffer that allows to directly access its content via the data() and related methods.
class ContiguousSharedBuffer : public SharedBuffer {
public:
    template <typename... Args>
    static Ref<ContiguousSharedBuffer> create(Args&&... args)
    {
        if constexpr (!sizeof...(Args))
            return adoptRef(*new ContiguousSharedBuffer());
        else if constexpr (sizeof...(Args) == 1
            && (std::is_same_v<Args, Ref<DataSegment>> &&...))
            return adoptRef(*new ContiguousSharedBuffer(std::forward<Args>(args)...));
        else {
            auto buffer = SharedBuffer::create(std::forward<Args>(args)...);
            return adoptRef(*new ContiguousSharedBuffer(WTFMove(buffer)));
        }
    }

    // Ensure that you can't call append on a ContiguousSharedBuffer directly.
    // When called from the base class, it will assert.
    template <typename... Args>
    void append(Args&&...) = delete;

    WEBCORE_EXPORT const uint8_t* data() const;
    const char* dataAsCharPtr() const { return reinterpret_cast<const char*>(data()); }
    WTF::Persistence::Decoder decoder() const;

    enum class MayUseFileMapping : bool { No, Yes };
    WEBCORE_EXPORT static RefPtr<ContiguousSharedBuffer> createWithContentsOfFile(const String& filePath, FileSystem::MappedFileMode = FileSystem::MappedFileMode::Shared, MayUseFileMapping = MayUseFileMapping::Yes);

#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData() const;
#endif
#if USE(CF)
    WEBCORE_EXPORT RetainPtr<CFDataRef> createCFData() const;
#endif
#if USE(GLIB)
    WEBCORE_EXPORT GRefPtr<GBytes> createGBytes() const;
#endif

private:
    WEBCORE_EXPORT ContiguousSharedBuffer();
    WEBCORE_EXPORT explicit ContiguousSharedBuffer(FileSystem::MappedFileData&&);
    WEBCORE_EXPORT explicit ContiguousSharedBuffer(Ref<DataSegment>&&);
    WEBCORE_EXPORT explicit ContiguousSharedBuffer(Ref<SharedBuffer>&&);

    WEBCORE_EXPORT static RefPtr<ContiguousSharedBuffer> createFromReadingFile(const String& filePath);
};

inline Vector<uint8_t> SharedBuffer::extractData()
{
    if (hasOneRef())
        return takeData();
    return copyData();
}

class SharedBufferDataView {
public:
    WEBCORE_EXPORT SharedBufferDataView(Ref<DataSegment>&&, size_t positionWithinSegment, std::optional<size_t> newSize = std::nullopt);
    WEBCORE_EXPORT SharedBufferDataView(const SharedBufferDataView&, size_t newSize);
    size_t size() const { return m_size; }
    const uint8_t* data() const { return m_segment->data() + m_positionWithinSegment; }
    const char* dataAsCharPtr() const { return reinterpret_cast<const char*>(data()); }

    WEBCORE_EXPORT Ref<ContiguousSharedBuffer> createSharedBuffer() const;
#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData() const;
#endif
private:
    const Ref<DataSegment> m_segment;
    const size_t m_positionWithinSegment;
    const size_t m_size;
};

RefPtr<ContiguousSharedBuffer> utf8Buffer(const String&);

} // namespace WebCore
