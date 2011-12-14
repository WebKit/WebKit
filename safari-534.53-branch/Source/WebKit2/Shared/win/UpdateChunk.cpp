/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "UpdateChunk.h"

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "WebCoreArgumentCoders.h"

using namespace WebCore;

namespace WebKit {

UpdateChunk::UpdateChunk()
    : m_bitmapSharedMemory(0)
{
}

UpdateChunk::UpdateChunk(const IntRect& rect)
    : m_rect(rect)
{
    // Create our shared memory mapping.
    unsigned memorySize = rect.height() * rect.width() * 4;
    m_bitmapSharedMemory = ::CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, memorySize, 0);
}

UpdateChunk::UpdateChunk(const IntRect& rect, HANDLE bitmapSharedMemory)
    : m_rect(rect)
    , m_bitmapSharedMemory(bitmapSharedMemory)
{
}

void UpdateChunk::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_rect);
    encoder->encode(reinterpret_cast<uintptr_t>(m_bitmapSharedMemory));
}

bool UpdateChunk::decode(CoreIPC::ArgumentDecoder* decoder, UpdateChunk& updateChunk)
{
    IntRect rect;
    if (!decoder->decode(rect))
        return false;
    updateChunk.m_rect = rect;

    uintptr_t bitmapSharedMmemory;
    if (!decoder->decode(bitmapSharedMmemory))
        return false;

    updateChunk.m_bitmapSharedMemory = reinterpret_cast<HANDLE>(bitmapSharedMmemory);
    return true;
}

} // namespace WebKit
