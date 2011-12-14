/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
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
#include <QIODevice>
#include <QImage>
#include <QPixmap>
#include <WebCore/FloatRect.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;
using namespace std;

namespace WebKit {

UpdateChunk::UpdateChunk()
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

size_t UpdateChunk::size() const
{
    int bpp;
    if (QPixmap::defaultDepth() == 16)
        bpp = 2;
    else
        bpp = 4;

    return ((m_rect.width() * bpp + 3) & ~0x3)
           * m_rect.height();
}

QImage UpdateChunk::createImage() const
{
    ASSERT(m_sharedMemory);
    if (!m_sharedMemory)
        return QImage();

    QImage::Format format;
    int bpp;
    if (QPixmap::defaultDepth() == 16) {
        format = QImage::Format_RGB16;
        bpp = 2;
    } else {
        format = QImage::Format_RGB32;
        bpp = 4;
    }

    return QImage(reinterpret_cast<unsigned char*>(m_sharedMemory->data()), m_rect.width(), m_rect.height(), (m_rect.width() * bpp + 3) & ~0x3, format);
}

} // namespace WebKit
