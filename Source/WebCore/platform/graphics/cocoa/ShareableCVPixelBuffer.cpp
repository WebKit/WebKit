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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShareableCVPixelBuffer.h"

#if PLATFORM(COCOA)

#include "CVUtilities.h"
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/text/StringBuilder.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

ShareableCVPixelBufferConfiguration::ShareableCVPixelBufferConfiguration(const RetainPtr<CVPixelBufferRef>& pixelBuffer)
    : m_width(CVPixelBufferGetWidth(pixelBuffer.get()))
    , m_height(CVPixelBufferGetHeight(pixelBuffer.get()))
    , m_bytesPerRow(CVPixelBufferGetBytesPerRow(pixelBuffer.get()))
    , m_pixelFormat(fromCVPixelFormat(CVPixelBufferGetPixelFormatType(pixelBuffer.get())))
{
}

ShareableCVPixelBufferConfiguration::ShareableCVPixelBufferConfiguration(unsigned width, unsigned height, unsigned bytesPerRow, ShareableCVPixelFormat pixelFormat)
    : m_width(width)
    , m_height(height)
    , m_bytesPerRow(bytesPerRow)
    , m_pixelFormat(pixelFormat)
{
}

// MARK: - ShareableCVPixelBufferBytesConfiguration.

ShareableCVPixelBufferBytesConfiguration::ShareableCVPixelBufferBytesConfiguration(const RetainPtr<CVPixelBufferRef>& pixelBuffer, unsigned planeIndex)
    : m_width(CVPixelBufferGetWidthOfPlane(pixelBuffer.get(), planeIndex))
    , m_height(CVPixelBufferGetHeightOfPlane(pixelBuffer.get(), planeIndex))
    , m_bytesPerRow(CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer.get(), planeIndex))
{
}

ShareableCVPixelBufferBytesConfiguration::ShareableCVPixelBufferBytesConfiguration(const ShareableCVPixelBufferConfiguration& configuration)
    : m_width(configuration.width())
    , m_height(configuration.height())
    , m_bytesPerRow(configuration.bytesPerRow())
{
}

ShareableCVPixelBufferBytesConfiguration::ShareableCVPixelBufferBytesConfiguration(unsigned width, unsigned height, unsigned bytesPerRow)
    : m_width(width)
    , m_height(height)
    , m_bytesPerRow(bytesPerRow)
{
}

// MARK: - ShareableCVPixelBufferBytesHandle.

ShareableCVPixelBufferBytesHandle::ShareableCVPixelBufferBytesHandle(SharedMemory::Handle&& handle, const ShareableCVPixelBufferBytesConfiguration& configuration)
    : m_handle(WTF::move(handle))
    , m_configuration(configuration)
{
}

// MARK: - ShareableCVPixelBufferBytes.

Ref<ShareableCVPixelBufferBytes> ShareableCVPixelBufferBytes::create(const ShareableCVPixelBufferBytesConfiguration& configuration, Ref<SharedMemory>&& sharedMemory)
{
    return adoptRef(*new ShareableCVPixelBufferBytes(configuration, WTF::move(sharedMemory)));
}

RefPtr<ShareableCVPixelBufferBytes> ShareableCVPixelBufferBytes::create(const ShareableCVPixelBufferBytesConfiguration& configuration, std::span<const uint8_t> sourceSpan)
{
    auto sizeInBytes = configuration.sizeInBytes();

    ASSERT(sizeInBytes);
    ASSERT(!sourceSpan.empty());

    RefPtr sharedMemory = SharedMemory::allocate(sizeInBytes);
    if (!sharedMemory)
        return nullptr;

    auto destinationSpan = sharedMemory->mutableSpan();
    if (destinationSpan.empty())
        return nullptr;

    memcpySpan(destinationSpan, sourceSpan);

    return create(configuration, sharedMemory.releaseNonNull());
}

RefPtr<ShareableCVPixelBufferBytes> ShareableCVPixelBufferBytes::create(Handle&& handle, SharedMemory::Protection protection)
{
    auto sharedMemory = SharedMemory::map(WTF::move(handle.m_handle), protection, SharedMemory::CopyOnWrite::No);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_configuration, sharedMemory.releaseNonNull());
}

std::optional<Ref<ShareableCVPixelBufferBytes>> ShareableCVPixelBufferBytes::createReadOnly(std::optional<Handle>&& handle)
{
    if (!handle)
        return std::nullopt;

    auto sharedMemory = SharedMemory::map(WTF::move(handle->m_handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemory)
        return std::nullopt;

    if (RefPtr bytes = create(WTF::move(*handle), SharedMemory::Protection::ReadOnly))
        return { bytes.releaseNonNull() };

    return std::nullopt;
}

ShareableCVPixelBufferBytes::ShareableCVPixelBufferBytes(const ShareableCVPixelBufferBytesConfiguration& configuration, Ref<SharedMemory>&& sharedMemory)
    : m_configuration(configuration)
    , m_sharedMemory(WTF::move(sharedMemory))
{
}

std::optional<ShareableCVPixelBufferBytesHandle> ShareableCVPixelBufferBytes::createReadOnlyHandle() const
{
    auto memoryHandle = m_sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
    if (!memoryHandle)
        return std::nullopt;
    return { Handle(WTF::move(*memoryHandle), m_configuration) };
}

void ShareableCVPixelBufferBytes::copyPixels(std::span<uint8_t> destinationSpan, unsigned destinationHeight, unsigned destinationBytesPerRow)
{
    ASSERT(destinationSpan.size() >= static_cast<size_t>(destinationHeight) * destinationBytesPerRow);

    auto sourceHeight = m_configuration.height();
    auto sourceBytesPerRow = m_configuration.bytesPerRow();
    auto sourceSpan = m_sharedMemory->span();

    auto clampedSourceHeight = std::min(sourceHeight, destinationHeight);
    auto clampedSourceBytesPerRow = std::min(sourceBytesPerRow, destinationBytesPerRow);

    for (unsigned row = 0; row < clampedSourceHeight; ++row) {
        auto destinationSubSpan = destinationSpan.subspan(row * destinationBytesPerRow, destinationBytesPerRow);
        auto sourceSubSpan = sourceSpan.subspan(row * sourceBytesPerRow, clampedSourceBytesPerRow);
        memcpySpan(destinationSubSpan, sourceSubSpan);
    }
}

// MARK: - ShareableCVPixelBuffer.

RefPtr<ShareableCVPixelBuffer> ShareableCVPixelBuffer::create(const RetainPtr<CVPixelBufferRef>& pixelBuffer)
{
    if (CVPixelBufferIsPlanar(pixelBuffer.get()))
        return ShareableCVPixelBufferWithPlanarBytes::create(pixelBuffer);
    return ShareableCVPixelBufferWithBytes::create(pixelBuffer);
}

ShareableCVPixelBuffer::ShareableCVPixelBuffer(ShareableCVPixelBufferConfiguration&& configuration)
    : m_configuration(WTF::move(configuration))
{
}

// MARK: - ShareableCVPixelBufferWithBytes.

Ref<ShareableCVPixelBufferWithBytes> ShareableCVPixelBufferWithBytes::create(ShareableCVPixelBufferConfiguration&& configuration, Ref<ShareableCVPixelBufferBytes>&& bytes)
{
    return adoptRef(*new ShareableCVPixelBufferWithBytes(WTF::move(configuration), WTF::move(bytes)));
}

RefPtr<ShareableCVPixelBufferWithBytes> ShareableCVPixelBufferWithBytes::create(const RetainPtr<CVPixelBufferRef>& pixelBuffer)
{
    ASSERT(!CVPixelBufferIsPlanar(pixelBuffer.get()));

    auto configuration = ShareableCVPixelBufferConfiguration(pixelBuffer.get());

    RefPtr<ShareableCVPixelBufferBytes> bytes;

    CVPixelBufferLockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    {
        auto bytesConfiguration = ShareableCVPixelBufferBytesConfiguration(configuration);
        auto baseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer.get()));

        if (auto sizeInBytes = bytesConfiguration.sizeInBytes()) {
            auto sourceSpan = unsafeMakeSpan(baseAddress, sizeInBytes);
            bytes = ShareableCVPixelBufferBytes::create(bytesConfiguration, sourceSpan);
        }
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);

    if (!bytes)
        return nullptr;

    return create(WTF::move(configuration), bytes.releaseNonNull());
}

ShareableCVPixelBufferWithBytes::ShareableCVPixelBufferWithBytes(ShareableCVPixelBufferConfiguration&& configuration, Ref<ShareableCVPixelBufferBytes>&& bytes)
    : ShareableCVPixelBuffer(WTF::move(configuration))
    , m_bytes(WTF::move(bytes))
{
}

RetainPtr<CVPixelBufferRef> ShareableCVPixelBufferWithBytes::createMetalCompatibleCVPixelBuffer() const
{
    RetainPtr pixelBuffer = createScratchMetalCompatibleCVPixelBuffer(*this);
    if (!pixelBuffer)
        return nullptr;

    CVPixelBufferLockBaseAddress(pixelBuffer.get(), 0);
    {
        unsigned destinationHeight = CVPixelBufferGetHeight(pixelBuffer.get());
        unsigned destinationBytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer.get());
        auto* destinationBaseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer.get()));
        auto destinationSpan = unsafeMakeSpan(destinationBaseAddress, destinationBytesPerRow * destinationHeight);
        protect(m_bytes)->copyPixels(destinationSpan, destinationHeight, destinationBytesPerRow);
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), 0);

    return WTF::move(pixelBuffer);
}

// MARK: - ShareableCVPixelBufferWithPlanarBytes.

Ref<ShareableCVPixelBufferWithPlanarBytes> ShareableCVPixelBufferWithPlanarBytes::create(ShareableCVPixelBufferConfiguration&& configuration, Vector<Ref<ShareableCVPixelBufferBytes>>&& planes)
{
    return adoptRef(*new ShareableCVPixelBufferWithPlanarBytes(WTF::move(configuration), WTF::move(planes)));
}

RefPtr<ShareableCVPixelBufferWithPlanarBytes> ShareableCVPixelBufferWithPlanarBytes::create(const RetainPtr<CVPixelBufferRef>& pixelBuffer)
{
    ASSERT(CVPixelBufferIsPlanar(pixelBuffer.get()));

    auto planesCount = static_cast<uint32_t>(CVPixelBufferGetPlaneCount(pixelBuffer.get()));
    if (!planesCount)
        return nullptr;

    Vector<Ref<ShareableCVPixelBufferBytes>> planes;

    CVPixelBufferLockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);
    {
        for (unsigned i = 0; i < planesCount; ++i) {
            auto bytesConfiguration = ShareableCVPixelBufferBytesConfiguration(pixelBuffer, i);
            auto baseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer.get(), i));

            if (auto sizeInBytes = bytesConfiguration.sizeInBytes()) {
                auto sourceSpan = unsafeMakeSpan(baseAddress, sizeInBytes);
                if (RefPtr bytes = ShareableCVPixelBufferBytes::create(bytesConfiguration, sourceSpan))
                    planes.append(bytes.releaseNonNull());
            }
        }
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), kCVPixelBufferLock_ReadOnly);

    if (planes.size() != planesCount)
        return nullptr;

    return create(ShareableCVPixelBufferConfiguration(pixelBuffer.get()), WTF::move(planes));
}

ShareableCVPixelBufferWithPlanarBytes::ShareableCVPixelBufferWithPlanarBytes(ShareableCVPixelBufferConfiguration&& configuration, Vector<Ref<ShareableCVPixelBufferBytes>>&& planes)
    : ShareableCVPixelBuffer(WTF::move(configuration))
    , m_planes(WTF::move(planes))
{
}

RetainPtr<CVPixelBufferRef> ShareableCVPixelBufferWithPlanarBytes::createMetalCompatibleCVPixelBuffer() const
{
    RetainPtr pixelBuffer = createScratchMetalCompatibleCVPixelBuffer(*this);
    if (!pixelBuffer)
        return nullptr;

    CVPixelBufferLockBaseAddress(pixelBuffer.get(), 0);
    {
        for (unsigned i = 0; i < m_planes.size(); ++i) {
            unsigned destinationHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer.get(), i);
            unsigned destinationBytesPerRow = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer.get(), i);
            auto* destinationBaseAddress = static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(pixelBuffer.get(), i));
            auto destinationSpan = unsafeMakeSpan(destinationBaseAddress, destinationBytesPerRow * destinationHeight);
            protect(m_planes[i])->copyPixels(destinationSpan, destinationHeight, destinationBytesPerRow);
        }
    }
    CVPixelBufferUnlockBaseAddress(pixelBuffer.get(), 0);

    return WTF::move(pixelBuffer);
}

} // namespace WebCore

#endif
