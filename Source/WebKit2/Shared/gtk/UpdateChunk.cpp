/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L
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

#include "WebCoreArgumentCoders.h"
#include <gdk/gdkx.h>

using namespace WebCore;

static cairo_format_t imageFormat = CAIRO_FORMAT_ARGB32;

namespace WebKit {

UpdateChunk::UpdateChunk()
    : m_sharedMemory(0)
{
}

UpdateChunk::UpdateChunk(const IntRect& rect)
    : m_rect(rect)
    , m_sharedMemory(SharedMemory::create(size()))
{
}

UpdateChunk::~UpdateChunk()
{
}

size_t UpdateChunk::size() const
{
    return cairo_format_stride_for_width(imageFormat, m_rect.width()) * m_rect.height();
}

void UpdateChunk::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_rect);
    if (!m_sharedMemory) {
        encoder->encode(false);
        return;
    }

    SharedMemory::Handle handle;
    if (m_sharedMemory->createHandle(handle, SharedMemory::ReadOnly)) {
        encoder->encode(true);
        encoder->encode(handle);
    } else
        encoder->encode(false);

    m_sharedMemory = 0;
}

bool UpdateChunk::decode(CoreIPC::ArgumentDecoder* decoder, UpdateChunk& chunk)
{
    ASSERT_ARG(chunk, chunk.isEmpty());

    IntRect rect;
    if (!decoder->decode(rect))
        return false;

    chunk.m_rect = rect;

    bool hasSharedMemory;
    if (!decoder->decode(hasSharedMemory))
        return false;

    if (!hasSharedMemory) {
        chunk.m_sharedMemory = 0;
        return true;
    }

    SharedMemory::Handle handle;
    if (!decoder->decode(handle))
        return false;

    chunk.m_sharedMemory = SharedMemory::create(handle, SharedMemory::ReadOnly);
    return true;
}

cairo_surface_t* UpdateChunk::createImage() const
{
    ASSERT(m_sharedMemory);
    if (!m_sharedMemory)
        return 0;

    int stride = cairo_format_stride_for_width(imageFormat, m_rect.width());
    return cairo_image_surface_create_for_data(static_cast<unsigned char*>(m_sharedMemory->data()),
                                               imageFormat, m_rect.width(), m_rect.height(), stride);
}

} // namespace WebKit
