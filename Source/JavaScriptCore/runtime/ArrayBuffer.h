/*
 * Copyright (C) 2009-2018 Apple Inc. All rights reserved.
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

#include "ArrayBufferSharingMode.h"
#include "GCIncomingRefCounted.h"
#include "Watchpoint.h"
#include "Weak.h"
#include <wtf/CagedPtr.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/PackedRefPtr.h>
#include <wtf/SharedTask.h>
#include <wtf/StdLibExtras.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {

#define MAX_ARRAY_BUFFER_SIZE 0x7fffffffu

class VM;
class ArrayBuffer;
class ArrayBufferView;
class JSArrayBuffer;

using ArrayBufferDestructorFunction = RefPtr<SharedTask<void(void*)>>;
using PackedArrayBufferDestructorFunction = PackedRefPtr<SharedTask<void(void*)>>;

class SharedArrayBufferContents : public ThreadSafeRefCounted<SharedArrayBufferContents> {
public:
    SharedArrayBufferContents(void* data, unsigned size, ArrayBufferDestructorFunction&&);
    ~SharedArrayBufferContents();
    
    void* data() const { return m_data.getMayBeNull(m_sizeInBytes); }
    
private:
    using DataType = CagedPtr<Gigacage::Primitive, void, tagCagedPtr>;
    DataType m_data;
    PackedArrayBufferDestructorFunction m_destructor;
    unsigned m_sizeInBytes;
};

class ArrayBufferContents {
    WTF_MAKE_NONCOPYABLE(ArrayBufferContents);
public:
    JS_EXPORT_PRIVATE ArrayBufferContents();
    JS_EXPORT_PRIVATE ArrayBufferContents(void* data, unsigned sizeInBytes, ArrayBufferDestructorFunction&&);
    
    JS_EXPORT_PRIVATE ArrayBufferContents(ArrayBufferContents&&);
    JS_EXPORT_PRIVATE ArrayBufferContents& operator=(ArrayBufferContents&&);

    JS_EXPORT_PRIVATE ~ArrayBufferContents();
    
    JS_EXPORT_PRIVATE void clear();
    
    explicit operator bool() { return !!m_data; }
    
    void* data() const { return m_data.getMayBeNull(sizeInBytes()); }
    unsigned sizeInBytes() const { return m_sizeInBytes; }
    
    bool isShared() const { return m_shared; }
    
private:
    void destroy();
    void reset();

    friend class ArrayBuffer;

    enum InitializationPolicy {
        ZeroInitialize,
        DontInitialize
    };

    void tryAllocate(unsigned numElements, unsigned elementByteSize, InitializationPolicy);
    
    void makeShared();
    void transferTo(ArrayBufferContents&);
    void copyTo(ArrayBufferContents&);
    void shareWith(ArrayBufferContents&);

    using DataType = CagedPtr<Gigacage::Primitive, void, tagCagedPtr>;
    DataType m_data;
    PackedArrayBufferDestructorFunction m_destructor;
    PackedRefPtr<SharedArrayBufferContents> m_shared;
    unsigned m_sizeInBytes;
};

class ArrayBuffer : public GCIncomingRefCounted<ArrayBuffer> {
public:
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> create(unsigned numElements, unsigned elementByteSize);
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> create(ArrayBuffer&);
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> create(const void* source, unsigned byteLength);
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> create(ArrayBufferContents&&);
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> createAdopted(const void* data, unsigned byteLength);
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> createFromBytes(const void* data, unsigned byteLength, ArrayBufferDestructorFunction&&);
    JS_EXPORT_PRIVATE static RefPtr<ArrayBuffer> tryCreate(unsigned numElements, unsigned elementByteSize);
    JS_EXPORT_PRIVATE static RefPtr<ArrayBuffer> tryCreate(ArrayBuffer&);
    JS_EXPORT_PRIVATE static RefPtr<ArrayBuffer> tryCreate(const void* source, unsigned byteLength);

    // Only for use by Uint8ClampedArray::tryCreateUninitialized and SharedBuffer::tryCreateArrayBuffer.
    JS_EXPORT_PRIVATE static Ref<ArrayBuffer> createUninitialized(unsigned numElements, unsigned elementByteSize);
    JS_EXPORT_PRIVATE static RefPtr<ArrayBuffer> tryCreateUninitialized(unsigned numElements, unsigned elementByteSize);

    inline void* data();
    inline const void* data() const;
    inline unsigned byteLength() const;
    
    void makeShared();
    void setSharingMode(ArrayBufferSharingMode);
    inline bool isShared() const;
    inline ArrayBufferSharingMode sharingMode() const { return isShared() ? ArrayBufferSharingMode::Shared : ArrayBufferSharingMode::Default; }

    inline size_t gcSizeEstimateInBytes() const;

    JS_EXPORT_PRIVATE RefPtr<ArrayBuffer> slice(double begin, double end) const;
    JS_EXPORT_PRIVATE RefPtr<ArrayBuffer> slice(double begin) const;
    
    inline void pin();
    inline void unpin();
    inline void pinAndLock();
    inline bool isLocked();

    void makeWasmMemory();
    inline bool isWasmMemory();

    JS_EXPORT_PRIVATE bool transferTo(VM&, ArrayBufferContents&);
    JS_EXPORT_PRIVATE bool shareWith(ArrayBufferContents&);

    void neuter(VM&);
    bool isNeutered() { return !m_contents.m_data; }
    InlineWatchpointSet& neuteringWatchpointSet() { return m_neuteringWatchpointSet; }

    static ptrdiff_t offsetOfData() { return OBJECT_OFFSETOF(ArrayBuffer, m_contents) + OBJECT_OFFSETOF(ArrayBufferContents, m_data); }

    ~ArrayBuffer() { }

    JS_EXPORT_PRIVATE static Ref<SharedTask<void(void*)>> primitiveGigacageDestructor();

private:
    static Ref<ArrayBuffer> create(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy);
    static Ref<ArrayBuffer> createInternal(ArrayBufferContents&&, const void*, unsigned);
    static RefPtr<ArrayBuffer> tryCreate(unsigned numElements, unsigned elementByteSize, ArrayBufferContents::InitializationPolicy);
    ArrayBuffer(ArrayBufferContents&&);
    RefPtr<ArrayBuffer> sliceImpl(unsigned begin, unsigned end) const;
    inline unsigned clampIndex(double index) const;
    static inline unsigned clampValue(double x, unsigned left, unsigned right);

    void notifyNeutering(VM&);

    ArrayBufferContents m_contents;
    InlineWatchpointSet m_neuteringWatchpointSet { IsWatched };
public:
    Weak<JSArrayBuffer> m_wrapper;
private:
    Checked<unsigned> m_pinCount;
    bool m_isWasmMemory;
    // m_locked == true means that some API user fetched m_contents directly from a TypedArray object,
    // the buffer is backed by a WebAssembly.Memory, or is a SharedArrayBuffer.
    bool m_locked;
};

void* ArrayBuffer::data()
{
    return m_contents.data();
}

const void* ArrayBuffer::data() const
{
    return m_contents.data();
}

unsigned ArrayBuffer::byteLength() const
{
    return m_contents.sizeInBytes();
}

bool ArrayBuffer::isShared() const
{
    return m_contents.isShared();
}

size_t ArrayBuffer::gcSizeEstimateInBytes() const
{
    // FIXME: We probably want to scale this by the shared ref count or something.
    return sizeof(ArrayBuffer) + static_cast<size_t>(byteLength());
}

void ArrayBuffer::pin()
{
    m_pinCount++;
}

void ArrayBuffer::unpin()
{
    m_pinCount--;
}

void ArrayBuffer::pinAndLock()
{
    m_locked = true;
}

bool ArrayBuffer::isLocked()
{
    return m_locked;
}

bool ArrayBuffer::isWasmMemory()
{
    return m_isWasmMemory;
}

JS_EXPORT_PRIVATE ASCIILiteral errorMesasgeForTransfer(ArrayBuffer*);

} // namespace JSC

using JSC::ArrayBuffer;
