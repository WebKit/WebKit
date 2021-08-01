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

#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/FileSystem.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Span.h>
#include <wtf/ThreadSafeRefCounted.h>
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
#endif

namespace WTF {
namespace Persistence {
class Decoder;
}
}

namespace WebCore {

class SharedBufferDataView;

class WEBCORE_EXPORT SharedBuffer : public RefCounted<SharedBuffer> {
public:
    static Ref<SharedBuffer> create() { return adoptRef(*new SharedBuffer); }
    static Ref<SharedBuffer> create(const uint8_t* data, size_t size) { return adoptRef(*new SharedBuffer(data, size)); }
    static Ref<SharedBuffer> create(const char* data, size_t size) { return adoptRef(*new SharedBuffer(data, size)); }
    static Ref<SharedBuffer> create(FileSystem::MappedFileData&& mappedFileData) { return adoptRef(*new SharedBuffer(WTFMove(mappedFileData))); }

    enum class MayUseFileMapping : bool { No, Yes };
    static RefPtr<SharedBuffer> createWithContentsOfFile(const String& filePath, FileSystem::MappedFileMode = FileSystem::MappedFileMode::Shared, MayUseFileMapping = MayUseFileMapping::Yes);

    static Ref<SharedBuffer> create(Vector<uint8_t>&&);

#if USE(FOUNDATION)
    RetainPtr<NSData> createNSData() const;
    RetainPtr<NSArray> createNSDataArray() const;
    static Ref<SharedBuffer> create(NSData *);
    void append(NSData *);
#endif
#if USE(CF)
    RetainPtr<CFDataRef> createCFData() const;
    static Ref<SharedBuffer> create(CFDataRef);
    void append(CFDataRef);
#endif

#if USE(GLIB)
    static Ref<SharedBuffer> create(GBytes*);
    GRefPtr<GBytes> createGBytes() const;
#endif

#if USE(GSTREAMER)
    static Ref<SharedBuffer> create(GstMappedOwnedBuffer&);
#endif
    // Calling data() causes all the data segments to be copied into one segment if they are not already.
    // Iterate the segments using begin() and end() instead.
    // FIXME: Audit the call sites of this function and replace them with iteration if possible.
    const uint8_t* data() const;
    const char* dataAsCharPtr() const { return reinterpret_cast<const char*>(data()); }
    Vector<uint8_t> copyData() const;

    // Similar to copyData() but avoids copying and will take the data instead when it is safe (The SharedBuffer is not shared).
    Vector<uint8_t> extractData();

    // Creates an ArrayBuffer and copies this SharedBuffer's contents to that
    // ArrayBuffer without merging segmented buffers into a flat buffer.
    RefPtr<ArrayBuffer> tryCreateArrayBuffer() const;

    size_t size() const { return m_size; }

    bool isEmpty() const { return !size(); }

    void append(const SharedBuffer&);
    void append(const uint8_t*, size_t);
    void append(const char* data, size_t length) { append(reinterpret_cast<const uint8_t*>(data), length); }
    void append(Vector<uint8_t>&&);

    void clear();

    Ref<SharedBuffer> copy() const;
    void copyTo(void* destination, size_t length) const;

    // Data wrapped by a DataSegment should be immutable because it can be referenced by other objects.
    // To modify or combine the data, allocate a new DataSegment.
    class DataSegment : public ThreadSafeRefCounted<DataSegment> {
    public:
        WEBCORE_EXPORT const uint8_t* data() const;
        WEBCORE_EXPORT size_t size() const;

        static Ref<DataSegment> create(Vector<uint8_t>&& data)
        {
            data.shrinkToFit();
            return adoptRef(*new DataSegment(WTFMove(data)));
        }

#if USE(CF)
        static Ref<DataSegment> create(RetainPtr<CFDataRef>&& data) { return adoptRef(*new DataSegment(WTFMove(data))); }
#endif
#if USE(GLIB)
        static Ref<DataSegment> create(GRefPtr<GBytes>&& data) { return adoptRef(*new DataSegment(WTFMove(data))); }
#endif
#if USE(GSTREAMER)
        static Ref<DataSegment> create(RefPtr<GstMappedOwnedBuffer>&& data) { return adoptRef(*new DataSegment(WTFMove(data))); }
#endif
        static Ref<DataSegment> create(FileSystem::MappedFileData&& data) { return adoptRef(*new DataSegment(WTFMove(data))); }

#if USE(FOUNDATION)
        RetainPtr<NSData> createNSData() const;
#endif

        bool containsMappedFileData() const;

    private:
        DataSegment(Vector<uint8_t>&& data)
            : m_immutableData(WTFMove(data)) { }
#if USE(CF)
        DataSegment(RetainPtr<CFDataRef>&& data)
            : m_immutableData(WTFMove(data)) { }
#endif
#if USE(GLIB)
        DataSegment(GRefPtr<GBytes>&& data)
            : m_immutableData(WTFMove(data)) { }
#endif
#if USE(GSTREAMER)
        DataSegment(RefPtr<GstMappedOwnedBuffer>&& data)
            : m_immutableData(WTFMove(data)) { }
#endif
        DataSegment(FileSystem::MappedFileData&& data)
            : m_immutableData(WTFMove(data)) { }

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
            FileSystem::MappedFileData> m_immutableData;
        friend class SharedBuffer;
    };

    void forEachSegment(const Function<void(const Span<const uint8_t>&)>&) const;
    bool startsWith(const Span<const uint8_t>& prefix) const;

    struct DataSegmentVectorEntry {
        size_t beginPosition;
        Ref<DataSegment> segment;
    };
    using DataSegmentVector = Vector<DataSegmentVectorEntry, 1>;
    DataSegmentVector::const_iterator begin() const { return m_segments.begin(); }
    DataSegmentVector::const_iterator end() const { return m_segments.end(); }
    bool hasOneSegment() const;
    
    // begin and end take O(1) time, this takes O(log(N)) time.
    SharedBufferDataView getSomeData(size_t position) const;

    String toHexString() const;

    void hintMemoryNotNeededSoon() const;

    WTF::Persistence::Decoder decoder() const;

    bool operator==(const SharedBuffer&) const;
    bool operator!=(const SharedBuffer& other) const { return !operator==(other); }

private:
    explicit SharedBuffer() = default;
    explicit SharedBuffer(const uint8_t*, size_t);
    explicit SharedBuffer(const char*, size_t);
    explicit SharedBuffer(Vector<uint8_t>&&);
    explicit SharedBuffer(FileSystem::MappedFileData&&);
#if USE(CF)
    explicit SharedBuffer(CFDataRef);
#endif
#if USE(GLIB)
    explicit SharedBuffer(GBytes*);
#endif
#if USE(GSTREAMER)
    explicit SharedBuffer(GstMappedOwnedBuffer&);
#endif

    void combineIntoOneSegment() const;

    // Combines all the segments into a Vector and returns that vector after clearing the SharedBuffer.
    Vector<uint8_t> takeData();
    
    static RefPtr<SharedBuffer> createFromReadingFile(const String& filePath);

    size_t m_size { 0 };
    mutable DataSegmentVector m_segments;

#if ASSERT_ENABLED
    mutable bool m_hasBeenCombinedIntoOneSegment { false };
    bool internallyConsistent() const;
#endif
};

inline Vector<uint8_t> SharedBuffer::extractData()
{
    if (hasOneRef())
        return takeData();
    return copyData();
}

class WEBCORE_EXPORT SharedBufferDataView {
public:
    SharedBufferDataView(Ref<SharedBuffer::DataSegment>&&, size_t);
    size_t size() const;
    const uint8_t* data() const;
    const char* dataAsCharPtr() const { return reinterpret_cast<const char*>(data()); }
#if USE(FOUNDATION)
    RetainPtr<NSData> createNSData() const;
#endif
private:
    size_t m_positionWithinSegment;
    Ref<SharedBuffer::DataSegment> m_segment;
};

RefPtr<SharedBuffer> utf8Buffer(const String&);

} // namespace WebCore
