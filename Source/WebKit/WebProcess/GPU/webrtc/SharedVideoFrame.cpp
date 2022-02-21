/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "SharedVideoFrame.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA) && ENABLE(VIDEO)

#include "Logging.h"
#include <WebCore/SharedVideoFrameInfo.h>
#include <wtf/Scope.h>

namespace WebKit {
using namespace WebCore;

SharedVideoFrameWriter::SharedVideoFrameWriter()
    : m_semaphore(makeUniqueRef<IPC::Semaphore>())
{
}

bool SharedVideoFrameWriter::wait(const Function<void(IPC::Semaphore&)>& newSemaphoreCallback)
{
    if (!m_isSemaphoreInUse) {
        m_isSemaphoreInUse = true;
        newSemaphoreCallback(m_semaphore.get());
        return true;
    }
    return !m_isDisabled && m_semaphore->wait();
}

bool SharedVideoFrameWriter::allocateStorage(size_t size, const Function<void(const SharedMemory::IPCHandle&)>& newMemoryCallback)
{
    m_storage = SharedMemory::allocate(size);
    if (!m_storage)
        return false;

    SharedMemory::Handle handle;
    m_storage->createHandle(handle, SharedMemory::Protection::ReadOnly);

    auto dataSize = handle.size();
    newMemoryCallback(SharedMemory::IPCHandle { WTFMove(handle), dataSize });
    return true;
}

bool SharedVideoFrameWriter::prepareWriting(const SharedVideoFrameInfo& info, const Function<void(IPC::Semaphore&)>& newSemaphoreCallback, const Function<void(const SharedMemory::IPCHandle&)>& newMemoryCallback)
{
    if (!info.isReadWriteSupported())
        return false;

    if (!wait(newSemaphoreCallback))
        return false;

    size_t size = info.storageSize();
    if (!m_storage || m_storage->size() < size) {
        if (!allocateStorage(size, newMemoryCallback))
            return false;
    }
    return true;
}

bool SharedVideoFrameWriter::write(CVPixelBufferRef pixelBuffer, const Function<void(IPC::Semaphore&)>& newSemaphoreCallback, const Function<void(const SharedMemory::IPCHandle&)>& newMemoryCallback)
{
    auto info = SharedVideoFrameInfo::fromCVPixelBuffer(pixelBuffer);
    if (!prepareWriting(info, newSemaphoreCallback, newMemoryCallback))
        return false;

    return info.writePixelBuffer(pixelBuffer, static_cast<uint8_t*>(m_storage->data()));
}

#if USE(LIBWEBRTC)
bool SharedVideoFrameWriter::write(const webrtc::VideoFrame& frame, const Function<void(IPC::Semaphore&)>& newSemaphoreCallback, const Function<void(const SharedMemory::IPCHandle&)>& newMemoryCallback)
{
    auto info = SharedVideoFrameInfo::fromVideoFrame(frame);
    if (!prepareWriting(info, newSemaphoreCallback, newMemoryCallback))
        return false;

    return info.writeVideoFrame(frame, static_cast<uint8_t*>(m_storage->data()));
}
#endif

void SharedVideoFrameWriter::disable()
{
    m_isDisabled = true;
    m_semaphore->signal();
}

RetainPtr<CVPixelBufferRef> SharedVideoFrameReader::read()
{
    if (!m_storage)
        return { };

    auto scope = makeScopeExit([&] {
        m_semaphore.signal();
    });

    auto* data = static_cast<const uint8_t*>(m_storage->data());
    auto info = SharedVideoFrameInfo::decode({ data, m_storage->size() });
    if (!info)
        return { };

    if (!info->isReadWriteSupported())
        return { };

    if (m_storage->size() < info->storageSize())
        return { };

    return info->createPixelBufferFromMemory(data + SharedVideoFrameInfoEncodingLength, pixelBufferPool(*info));
}

CVPixelBufferPoolRef SharedVideoFrameReader::pixelBufferPool(const SharedVideoFrameInfo& info)
{
    if (m_useIOSurfaceBufferPool == UseIOSurfaceBufferPool::No)
        return nullptr;

    if (!m_bufferPool || m_bufferPoolType != info.bufferType() || m_bufferPoolWidth != info.width() || m_bufferPoolHeight != info.height()) {
        m_bufferPoolType = info.bufferType();
        m_bufferPoolWidth = info.width();
        m_bufferPoolHeight = info.height();
        m_bufferPool = info.createCompatibleBufferPool();
    }

    return m_bufferPool.get();
}

bool SharedVideoFrameReader::setSharedMemory(const SharedMemory::IPCHandle& ipcHandle)
{
    m_storage = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
    return !!m_storage;
}

}

#endif
