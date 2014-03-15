/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#ifndef SharedBuffer_h
#define SharedBuffer_h

#include <runtime/ArrayBuffer.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include <wtf/RetainPtr.h>
#endif

#if USE(SOUP)
#include "GUniquePtrSoup.h"
#endif

#if USE(FOUNDATION)
OBJC_CLASS NSData;
#endif

namespace WebCore {
    
class PurgeableBuffer;

class SharedBuffer : public RefCounted<SharedBuffer> {
public:
    static PassRefPtr<SharedBuffer> create() { return adoptRef(new SharedBuffer); }
    static PassRefPtr<SharedBuffer> create(unsigned size) { return adoptRef(new SharedBuffer(size)); }
    static PassRefPtr<SharedBuffer> create(const char* c, unsigned i) { return adoptRef(new SharedBuffer(c, i)); }
    static PassRefPtr<SharedBuffer> create(const unsigned char* c, unsigned i) { return adoptRef(new SharedBuffer(c, i)); }

    static PassRefPtr<SharedBuffer> createWithContentsOfFile(const String& filePath);

    static PassRefPtr<SharedBuffer> adoptVector(Vector<char>& vector);
    
    // The buffer must be in non-purgeable state before adopted to a SharedBuffer. 
    // It will stay that way until released.
    static PassRefPtr<SharedBuffer> adoptPurgeableBuffer(PassOwnPtr<PurgeableBuffer>);
    
    ~SharedBuffer();
    
#if USE(FOUNDATION)
    // FIXME: This class exists as a temporary workaround so that code that does:
    // [buffer->createNSData() autorelease] will fail to compile.
    // Once both Mac and iOS builds with this change we can change the return type to be RetainPtr<NSData>,
    // since we're mostly worried about existing code breaking (it's unlikely that we'd use retain/release together
    // with RetainPtr in new code.
    class NSDataRetainPtrWithoutImplicitConversionOperator : public RetainPtr<NSData*> {
    public:
        template<typename T>
        NSDataRetainPtrWithoutImplicitConversionOperator(RetainPtr<T*>&& other)
            : RetainPtr<NSData*>(std::move(other))
        {
        }

        explicit operator PtrType() = delete;
    };

    NSDataRetainPtrWithoutImplicitConversionOperator createNSData();
    static PassRefPtr<SharedBuffer> wrapNSData(NSData *data);
#endif
#if USE(CF)
    RetainPtr<CFDataRef> createCFData();
    static PassRefPtr<SharedBuffer> wrapCFData(CFDataRef);
#endif

#if USE(SOUP)
    static PassRefPtr<SharedBuffer> wrapSoupBuffer(SoupBuffer*);
#endif

    // Calling this function will force internal segmented buffers
    // to be merged into a flat buffer. Use getSomeData() whenever possible
    // for better performance.
    const char* data() const;
    // Creates an ArrayBuffer and copies this SharedBuffer's contents to that
    // ArrayBuffer without merging segmented buffers into a flat buffer.
    PassRefPtr<ArrayBuffer> createArrayBuffer() const;

    unsigned size() const;


    bool isEmpty() const { return !size(); }

    void append(SharedBuffer*);
    void append(const char*, unsigned);
    void append(const Vector<char>&);

    void clear();
    const char* platformData() const;
    unsigned platformDataSize() const;

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    void append(CFDataRef);
#endif

    PassRefPtr<SharedBuffer> copy() const;
    
    bool hasPurgeableBuffer() const { return m_purgeableBuffer.get(); }

    // Ensure this buffer has no other clients before calling this.
    PassOwnPtr<PurgeableBuffer> releasePurgeableBuffer();

    // Return the number of consecutive bytes after "position". "data"
    // points to the first byte.
    // Return 0 when no more data left.
    // When extracting all data with getSomeData(), the caller should
    // repeat calling it until it returns 0.
    // Usage:
    //      const char* segment;
    //      unsigned pos = 0;
    //      while (unsigned length = sharedBuffer->getSomeData(segment, pos)) {
    //          // Use the data. for example: decoder->decode(segment, length);
    //          pos += length;
    //      }
    unsigned getSomeData(const char*& data, unsigned position = 0) const;

    void shouldUsePurgeableMemory(bool use) { m_shouldUsePurgeableMemory = use; }

#if ENABLE(DISK_IMAGE_CACHE)
    enum MemoryMappingState { QueuedForMapping, PreviouslyQueuedForMapping, SuccessAlreadyMapped, FailureCacheFull };

    // Calling this will cause this buffer to be memory mapped.
    MemoryMappingState allowToBeMemoryMapped();
    bool isAllowedToBeMemoryMapped() const;

    // This is called to indicate that the memory mapping failed.
    void failedMemoryMap();

    // This is called only once the buffer has been completely memory mapped.
    void markAsMemoryMapped();
    bool isMemoryMapped() const { return m_isMemoryMapped; }

    // This callback function will be called when either the buffer has been memory mapped or failed to be memory mapped.
    enum CompletionStatus { Failed, Succeeded };
    typedef void* MemoryMappedNotifyCallbackData;
    typedef void (*MemoryMappedNotifyCallback)(PassRefPtr<SharedBuffer>, CompletionStatus, MemoryMappedNotifyCallbackData);

    MemoryMappedNotifyCallbackData memoryMappedNotificationCallbackData() const;
    MemoryMappedNotifyCallback memoryMappedNotificationCallback() const;
    void setMemoryMappedNotificationCallback(MemoryMappedNotifyCallback, MemoryMappedNotifyCallbackData);
#endif

    void createPurgeableBuffer() const;

    void tryReplaceContentsWithPlatformBuffer(SharedBuffer*);

private:
    SharedBuffer();
    explicit SharedBuffer(unsigned);
    SharedBuffer(const char*, unsigned);
    SharedBuffer(const unsigned char*, unsigned);
    
    // Calling this function will force internal segmented buffers
    // to be merged into a flat buffer. Use getSomeData() whenever possible
    // for better performance.
    // As well, be aware that this method does *not* return any purgeable
    // memory, which can be a source of bugs.
    const Vector<char>& buffer() const;

    void clearPlatformData();
    void maybeTransferPlatformData();
    bool hasPlatformData() const;

    void copyBufferAndClear(char* destination, unsigned bytesToCopy) const;

    unsigned m_size;
    mutable Vector<char> m_buffer;
    bool m_shouldUsePurgeableMemory;
    mutable OwnPtr<PurgeableBuffer> m_purgeableBuffer;
#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    mutable Vector<RetainPtr<CFDataRef>> m_dataArray;
    unsigned copySomeDataFromDataArray(const char*& someData, unsigned position) const;
    const char *singleDataArrayBuffer() const;
#else
    mutable Vector<char*> m_segments;
#endif
#if ENABLE(DISK_IMAGE_CACHE)
    bool m_isMemoryMapped;
    unsigned m_diskImageCacheId; // DiskImageCacheId is unsigned.
    MemoryMappedNotifyCallback m_notifyMemoryMappedCallback;
    MemoryMappedNotifyCallbackData m_notifyMemoryMappedCallbackData;
#endif
#if USE(CF)
    explicit SharedBuffer(CFDataRef);
    RetainPtr<CFDataRef> m_cfData;
#endif

#if USE(SOUP)
    explicit SharedBuffer(SoupBuffer*);
    GUniquePtr<SoupBuffer> m_soupBuffer;
#endif
};

PassRefPtr<SharedBuffer> utf8Buffer(const String&);

} // namespace WebCore

#endif // SharedBuffer_h
