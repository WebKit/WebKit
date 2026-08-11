/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#include <WebCore/CVPixelBufferUtilities.h>
#include <WebCore/ShareableCVPixelFormat.h>
#include <WebCore/SharedMemory.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class ShareableCVPixelBufferConfiguration {
public:
    ShareableCVPixelBufferConfiguration(const ShareableCVPixelBufferConfiguration&) = default;
    ShareableCVPixelBufferConfiguration(ShareableCVPixelBufferConfiguration&&) = default;
    ShareableCVPixelBufferConfiguration(const RetainPtr<CVPixelBufferRef>&);
    WEBCORE_EXPORT ShareableCVPixelBufferConfiguration(unsigned width, unsigned height, unsigned bytesPerRow, ShareableCVPixelFormat);

    ShareableCVPixelBufferConfiguration& operator=(ShareableCVPixelBufferConfiguration&&) = default;

    unsigned width() const { return m_width; }
    unsigned height() const { ASSERT(!m_height.hasOverflowed()); return m_height; }
    unsigned bytesPerRow() const { ASSERT(!m_bytesPerRow.hasOverflowed()); return m_bytesPerRow; }
    ShareableCVPixelFormat pixelFormat() const { return m_pixelFormat; }

private:
    friend struct IPC::ArgumentCoder<ShareableCVPixelBufferConfiguration>;

    CheckedUint32 m_width;
    CheckedUint32 m_height;
    CheckedUint32 m_bytesPerRow;
    ShareableCVPixelFormat m_pixelFormat;
};

class ShareableCVPixelBufferBytesConfiguration {
public:
    ShareableCVPixelBufferBytesConfiguration(const ShareableCVPixelBufferBytesConfiguration&) = default;
    WEBCORE_EXPORT ShareableCVPixelBufferBytesConfiguration(const RetainPtr<CVPixelBufferRef>&, unsigned planeIndex);
    WEBCORE_EXPORT ShareableCVPixelBufferBytesConfiguration(const ShareableCVPixelBufferConfiguration&);
    WEBCORE_EXPORT ShareableCVPixelBufferBytesConfiguration(unsigned width, unsigned height, unsigned bytesPerRow);

    unsigned width() const { return m_width; }
    unsigned height() const { return m_height; }
    unsigned bytesPerRow() const { return m_bytesPerRow; }

    CheckedUint32 sizeInBytes() const { return m_bytesPerRow * m_height; }

private:
    friend struct IPC::ArgumentCoder<ShareableCVPixelBufferBytesConfiguration>;

    CheckedUint32 m_width;
    CheckedUint32 m_height;
    CheckedUint32 m_bytesPerRow;
};

class ShareableCVPixelBufferBytesHandle  {
public:
    ShareableCVPixelBufferBytesHandle(ShareableCVPixelBufferBytesHandle&&) = default;
    explicit ShareableCVPixelBufferBytesHandle(const ShareableCVPixelBufferBytesHandle&) = default;
    WEBCORE_EXPORT ShareableCVPixelBufferBytesHandle(SharedMemory::Handle&&, const ShareableCVPixelBufferBytesConfiguration&);

    ShareableCVPixelBufferBytesHandle& operator=(ShareableCVPixelBufferBytesHandle&&) = default;

    SharedMemory::Handle& handle() LIFETIME_BOUND { return m_handle; }

private:
    friend struct IPC::ArgumentCoder<ShareableCVPixelBufferBytesHandle>;
    friend class ShareableCVPixelBufferBytes;

    SharedMemory::Handle m_handle;
    ShareableCVPixelBufferBytesConfiguration m_configuration;
};

class ShareableCVPixelBufferBytes : public ThreadSafeRefCounted<ShareableCVPixelBufferBytes> {
public:
    using Handle = ShareableCVPixelBufferBytesHandle;

    WEBCORE_EXPORT static Ref<ShareableCVPixelBufferBytes> create(const ShareableCVPixelBufferBytesConfiguration&, Ref<SharedMemory>&&);
    WEBCORE_EXPORT static RefPtr<ShareableCVPixelBufferBytes> create(const ShareableCVPixelBufferBytesConfiguration&, std::span<const uint8_t> sourceSpan);

    WEBCORE_EXPORT static RefPtr<ShareableCVPixelBufferBytes> create(Handle&&, SharedMemory::Protection = SharedMemory::Protection::ReadWrite);
    WEBCORE_EXPORT static std::optional<Ref<ShareableCVPixelBufferBytes>> createReadOnly(std::optional<Handle>&&);

    WEBCORE_EXPORT std::optional<Handle> createReadOnlyHandle() const;

    void copyPixels(std::span<uint8_t> destinationSpan, unsigned destinationHeight, unsigned destinationBytesPerRow);

private:
    ShareableCVPixelBufferBytes(const ShareableCVPixelBufferBytesConfiguration&, Ref<SharedMemory>&&);

    ShareableCVPixelBufferBytesConfiguration m_configuration;
    const Ref<SharedMemory> m_sharedMemory;
};

class ShareableCVPixelBuffer : public ThreadSafeRefCounted<ShareableCVPixelBuffer> {
public:
    WEBCORE_EXPORT static RefPtr<ShareableCVPixelBuffer> create(const RetainPtr<CVPixelBufferRef>&);

    virtual ~ShareableCVPixelBuffer() = default;

    virtual bool isPlanar() const { return false; }
    const ShareableCVPixelBufferConfiguration& configuration() const { return m_configuration; }

    virtual RetainPtr<CVPixelBufferRef> createMetalCompatibleCVPixelBuffer() const = 0;

protected:
    ShareableCVPixelBuffer(ShareableCVPixelBufferConfiguration&&);

    ShareableCVPixelBufferConfiguration m_configuration;
};

class ShareableCVPixelBufferWithBytes : public ShareableCVPixelBuffer {
public:
    WEBCORE_EXPORT static Ref<ShareableCVPixelBufferWithBytes> create(ShareableCVPixelBufferConfiguration&&, Ref<ShareableCVPixelBufferBytes>&&);

    WEBCORE_EXPORT static RefPtr<ShareableCVPixelBufferWithBytes> create(const RetainPtr<CVPixelBufferRef>&);

private:
    friend struct IPC::ArgumentCoder<ShareableCVPixelBufferWithBytes>;

    ShareableCVPixelBufferWithBytes(ShareableCVPixelBufferConfiguration&&, Ref<ShareableCVPixelBufferBytes>&&);

    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> createMetalCompatibleCVPixelBuffer() const final;

    Ref<ShareableCVPixelBufferBytes> m_bytes;
};

class ShareableCVPixelBufferWithPlanarBytes : public ShareableCVPixelBuffer {
public:
    WEBCORE_EXPORT static Ref<ShareableCVPixelBufferWithPlanarBytes> create(ShareableCVPixelBufferConfiguration&&, Vector<Ref<ShareableCVPixelBufferBytes>>&&);

    WEBCORE_EXPORT static RefPtr<ShareableCVPixelBufferWithPlanarBytes> create(const RetainPtr<CVPixelBufferRef>&);

private:
    friend struct IPC::ArgumentCoder<ShareableCVPixelBufferWithPlanarBytes>;

    ShareableCVPixelBufferWithPlanarBytes(ShareableCVPixelBufferConfiguration&&, Vector<Ref<ShareableCVPixelBufferBytes>>&&);

    bool isPlanar() const final { return true; }
    WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> createMetalCompatibleCVPixelBuffer() const final;

    Vector<Ref<ShareableCVPixelBufferBytes>> m_planes;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShareableCVPixelBufferWithBytes) \
    static bool isType(const WebCore::ShareableCVPixelBuffer& pixelBuffer) { return !pixelBuffer.isPlanar(); } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ShareableCVPixelBufferWithPlanarBytes) \
    static bool isType(const WebCore::ShareableCVPixelBuffer& pixelBuffer) { return pixelBuffer.isPlanar(); } \
SPECIALIZE_TYPE_TRAITS_END()

#endif
