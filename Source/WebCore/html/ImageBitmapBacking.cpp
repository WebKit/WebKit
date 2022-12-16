/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "ImageBitmapBacking.h"

#include "Chrome.h"
#include "Page.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

ImageBitmapBacking::ImageBitmapBacking(RefPtr<ImageBuffer>&& bitmapData, OptionSet<SerializationState> serializationState)
    : m_bitmapData(WTFMove(bitmapData))
    , m_serializationState(serializationState)
{
    ASSERT(m_bitmapData);
}

ImageBuffer* ImageBitmapBacking::buffer() const
{
    return m_bitmapData.get();
}

RefPtr<ImageBuffer> ImageBitmapBacking::takeImageBuffer()
{
    return WTFMove(m_bitmapData);
}

RefPtr<ImageBuffer> ImageBitmapBacking::takeImageBufferForDifferentThread()
{
    return ImageBuffer::sinkIntoBufferForDifferentThread(WTFMove(m_bitmapData));
}

unsigned ImageBitmapBacking::width() const
{
    // FIXME: Is this the right width?
    return m_bitmapData ? m_bitmapData->truncatedLogicalSize().width() : 0;
}

unsigned ImageBitmapBacking::height() const
{
    // FIXME: Is this the right height?
    return m_bitmapData ? m_bitmapData->truncatedLogicalSize().height() : 0;
}

void ImageBitmapBacking::disconnect()
{
    // FIXME: Rather than storing both the ImageBuffer and the
    // SerializedImageBuffer here, with only one valid at a time,
    // we should have a separate object for the serialized state
    if (m_bitmapData)
        m_serializedBitmap = ImageBuffer::sinkIntoSerializedImageBuffer(WTFMove(m_bitmapData));
    ASSERT(!m_bitmapData);
}

void ImageBitmapBacking::connect(ScriptExecutionContext& context)
{
    ASSERT(!m_bitmapData);
    if (!m_serializedBitmap)
        return;

    if (is<WorkerGlobalScope>(context) && downcast<WorkerGlobalScope>(context).workerClient()) {
        auto* client = downcast<WorkerGlobalScope>(context).workerClient();
        m_bitmapData = client->sinkIntoImageBuffer(WTFMove(m_serializedBitmap));
    } else if (is<Document>(context)) {
        ASSERT(downcast<Document>(context).page());
        m_bitmapData = downcast<Document>(context).page()->chrome().sinkIntoImageBuffer(WTFMove(m_serializedBitmap));
    } else
        m_bitmapData = SerializedImageBuffer::sinkIntoImageBuffer(WTFMove(m_serializedBitmap));
}

} // namespace WebCore
