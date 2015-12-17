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

#include "FileSystem.h"
#include <runtime/ArrayBuffer.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(CF)
#include "VNodeTracker.h"
#include <wtf/RetainPtr.h>
#endif

#if USE(SOUP)
#include "GUniquePtrSoup.h"
#endif

#if USE(FOUNDATION)
OBJC_CLASS NSData;
#endif

namespace WebCore {
    
class SharedBuffer : public RefCounted<SharedBuffer> {
public:
    static PassRefPtr<SharedBuffer> create() { return adoptRef(new SharedBuffer); }
    static PassRefPtr<SharedBuffer> create(unsigned size) { return adoptRef(new SharedBuffer(size)); }
    static PassRefPtr<SharedBuffer> create(const char* c, unsigned i) { return adoptRef(new SharedBuffer(c, i)); }
    static PassRefPtr<SharedBuffer> create(const unsigned char* c, unsigned i) { return adoptRef(new SharedBuffer(c, i)); }

    WEBCORE_EXPORT static RefPtr<SharedBuffer> createWithContentsOfFile(const String& filePath);

    WEBCORE_EXPORT static PassRefPtr<SharedBuffer> adoptVector(Vector<char>& vector);
    
    WEBCORE_EXPORT ~SharedBuffer();
    
#if USE(FOUNDATION)
    WEBCORE_EXPORT RetainPtr<NSData> createNSData();
    WEBCORE_EXPORT static PassRefPtr<SharedBuffer> wrapNSData(NSData *data);
#endif
#if USE(CF)
    WEBCORE_EXPORT RetainPtr<CFDataRef> createCFData();
    WEBCORE_EXPORT CFDataRef existingCFData();
    WEBCORE_EXPORT static PassRefPtr<SharedBuffer> wrapCFData(CFDataRef);
#endif

#if USE(SOUP)
    static PassRefPtr<SharedBuffer> wrapSoupBuffer(SoupBuffer*);
#endif

    // Calling this function will force internal segmented buffers
    // to be merged into a flat buffer. Use getSomeData() whenever possible
    // for better performance.
    WEBCORE_EXPORT const char* data() const;
    // Creates an ArrayBuffer and copies this SharedBuffer's contents to that
    // ArrayBuffer without merging segmented buffers into a flat buffer.
    PassRefPtr<ArrayBuffer> createArrayBuffer() const;

    WEBCORE_EXPORT unsigned size() const;


    bool isEmpty() const { return !size(); }

    WEBCORE_EXPORT void append(SharedBuffer*);
    WEBCORE_EXPORT void append(const char*, unsigned);
    void append(const Vector<char>&);

    WEBCORE_EXPORT void clear();
    const char* platformData() const;
    unsigned platformDataSize() const;

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    static PassRefPtr<SharedBuffer> wrapCFDataArray(CFArrayRef);
    void append(CFDataRef);
#endif

    WEBCORE_EXPORT Ref<SharedBuffer> copy() const;
    
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
    WEBCORE_EXPORT unsigned getSomeData(const char*& data, unsigned position = 0) const;

    bool tryReplaceContentsWithPlatformBuffer(SharedBuffer&);
    WEBCORE_EXPORT bool hasPlatformData() const;

    struct DataBuffer : public ThreadSafeRefCounted<DataBuffer> {
        Vector<char> data;
    };

    void hintMemoryNotNeededSoon();

private:
    WEBCORE_EXPORT SharedBuffer();
    explicit SharedBuffer(unsigned);
    WEBCORE_EXPORT SharedBuffer(const char*, unsigned);
    WEBCORE_EXPORT SharedBuffer(const unsigned char*, unsigned);
    explicit SharedBuffer(MappedFileData&&);

    static RefPtr<SharedBuffer> createFromReadingFile(const String& filePath);

    // Calling this function will force internal segmented buffers
    // to be merged into a flat buffer. Use getSomeData() whenever possible
    // for better performance.
    const Vector<char>& buffer() const;

    void clearPlatformData();
    void maybeTransferPlatformData();
    bool maybeAppendPlatformData(SharedBuffer*);

    void maybeTransferMappedFileData();

    void copyBufferAndClear(char* destination, unsigned bytesToCopy) const;

    void appendToDataBuffer(const char *, unsigned) const;
    void duplicateDataBufferIfNecessary() const;
    void clearDataBuffer();

    unsigned m_size { 0 };
    mutable RefPtr<DataBuffer> m_buffer;

#if USE(NETWORK_CFDATA_ARRAY_CALLBACK)
    explicit SharedBuffer(CFArrayRef);
    mutable Vector<RetainPtr<CFDataRef>> m_dataArray;
    unsigned copySomeDataFromDataArray(const char*& someData, unsigned position) const;
    const char *singleDataArrayBuffer() const;
    bool maybeAppendDataArray(SharedBuffer*);
#else
    mutable Vector<char*> m_segments;
#endif

#if USE(CF)
    explicit SharedBuffer(CFDataRef);
    RetainPtr<CFDataRef> m_cfData;
    VNodeTracker::Token m_vnodeToken;
#endif

#if USE(SOUP)
    explicit SharedBuffer(SoupBuffer*);
    GUniquePtr<SoupBuffer> m_soupBuffer;
#endif

    MappedFileData m_fileData;
};

PassRefPtr<SharedBuffer> utf8Buffer(const String&);

} // namespace WebCore

#endif // SharedBuffer_h
