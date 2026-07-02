/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "RemoteImageBufferGraphicsContext.h"

#if ENABLE(GPU_PROCESS)

#include "Logging.h"
#include "RemoteGraphicsContextMessages.h"

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_renderingBackend->streamConnection());

namespace WebKit {
using namespace WebCore;

Ref<RemoteImageBufferGraphicsContext> RemoteImageBufferGraphicsContext::create(ImageBuffer& imageBuffer, RemoteGraphicsContextIdentifier identifier, RemoteRenderingBackend& renderingBackend)
{
    Ref instance = adoptRef(*new RemoteImageBufferGraphicsContext(imageBuffer, identifier, renderingBackend));
    instance->startListeningForIPC();
    return instance;
}

RemoteImageBufferGraphicsContext::RemoteImageBufferGraphicsContext(ImageBuffer& imageBuffer, RemoteGraphicsContextIdentifier identifier, RemoteRenderingBackend& renderingBackend)
    : RemoteGraphicsContext(imageBuffer.context(), renderingBackend)
    , m_imageBuffer(imageBuffer)
    , m_identifier(identifier)
{
}

RemoteImageBufferGraphicsContext::~RemoteImageBufferGraphicsContext() = default;

void RemoteImageBufferGraphicsContext::startListeningForIPC()
{
    m_renderingBackend->streamConnection().startReceivingMessages(*this, Messages::RemoteGraphicsContext::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteImageBufferGraphicsContext::stopListeningForIPC()
{
    m_renderingBackend->streamConnection().stopReceivingMessages(Messages::RemoteGraphicsContext::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteImageBufferGraphicsContext::drawImageBuffer(RenderingResourceIdentifier imageBufferIdentifier, const FloatRect& destinationRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr sourceImage = imageBuffer(imageBufferIdentifier);
    MESSAGE_CHECK(sourceImage);
    bool selfCopy = false;
    if (sourceImage == m_imageBuffer.ptr() && sourceImage->renderingMode() == RenderingMode::Accelerated) {
        sourceImage = sourceImage->clone();
        sourceImage->flushDrawingContext();
        selfCopy = true;
    }
    context().drawImageBuffer(*sourceImage, destinationRect, srcRect, options);
    if (selfCopy)
        m_imageBuffer->flushDrawingContext();
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
