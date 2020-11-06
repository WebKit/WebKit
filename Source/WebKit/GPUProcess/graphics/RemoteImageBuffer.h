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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "Decoder.h"
#include "GPUConnectionToWebProcess.h"
#include <WebCore/ConcreteImageBuffer.h>
#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/DisplayListReplayer.h>

namespace WebKit {

template<typename BackendType>
class RemoteImageBuffer : public WebCore::ConcreteImageBuffer<BackendType>, public WebCore::DisplayList::Replayer::Delegate, public WebCore::DisplayList::ItemBufferReadingClient {
    using BaseConcreteImageBuffer = WebCore::ConcreteImageBuffer<BackendType>;
    using BaseConcreteImageBuffer::context;
    using BaseConcreteImageBuffer::m_backend;
    using BaseConcreteImageBuffer::putImageData;

public:
    static auto create(const WebCore::FloatSize& size, float resolutionScale, WebCore::ColorSpace colorSpace, RemoteRenderingBackend& remoteRenderingBackend, WebCore::RenderingResourceIdentifier renderingResourceIdentifier)
    {
        return BaseConcreteImageBuffer::template create<RemoteImageBuffer>(size, resolutionScale, colorSpace, nullptr, remoteRenderingBackend, renderingResourceIdentifier);
    }

    RemoteImageBuffer(std::unique_ptr<BackendType>&& backend, RemoteRenderingBackend& remoteRenderingBackend, WebCore::RenderingResourceIdentifier renderingResourceIdentifier)
        : BaseConcreteImageBuffer(WTFMove(backend), renderingResourceIdentifier)
        , m_remoteRenderingBackend(remoteRenderingBackend)
    {
        m_remoteRenderingBackend.imageBufferBackendWasCreated(m_backend->logicalSize(), m_backend->backendSize(), m_backend->resolutionScale(), m_backend->colorSpace(), m_backend->createImageBufferBackendHandle(), renderingResourceIdentifier);
    }

    ~RemoteImageBuffer()
    {
        // Unwind the context's state stack before destruction, since calls to restore may not have
        // been flushed yet, or the web process may have terminated.
        while (context().stackSize())
            context().restore();
    }

private:
    void flushDisplayList(const WebCore::DisplayList::DisplayList& displayList) override
    {
        if (!displayList.isEmpty()) {
            const auto& imageBuffers = m_remoteRenderingBackend.remoteResourceCache().imageBuffers();
            WebCore::DisplayList::Replayer replayer { BaseConcreteImageBuffer::context(), displayList, &imageBuffers, this };
            replayer.replay();
        }
    }

    bool apply(WebCore::DisplayList::ItemHandle item, WebCore::GraphicsContext& context) override
    {
        if (item.is<WebCore::DisplayList::PutImageData>()) {
            auto& putImageDataItem = item.get<WebCore::DisplayList::PutImageData>();
            putImageData(putImageDataItem.inputFormat(), putImageDataItem.imageData(), putImageDataItem.srcRect(), putImageDataItem.destPoint(), putImageDataItem.destFormat());
            return true;
        }

        return m_remoteRenderingBackend.applyMediaItem(item, context);
    }

    Optional<WebCore::DisplayList::ItemHandle> WARN_UNUSED_RETURN decodeItem(const uint8_t* data, size_t length, WebCore::DisplayList::ItemType type, uint8_t* handleLocation) override
    {
        switch (type) {
        case WebCore::DisplayList::ItemType::ClipOutToPath:
            return decodeAndCreate<WebCore::DisplayList::ClipOutToPath>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::ClipPath:
            return decodeAndCreate<WebCore::DisplayList::ClipPath>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::ClipToDrawingCommands:
            return decodeAndCreate<WebCore::DisplayList::ClipToDrawingCommands>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawFocusRingPath:
            return decodeAndCreate<WebCore::DisplayList::DrawFocusRingPath>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawFocusRingRects:
            return decodeAndCreate<WebCore::DisplayList::DrawFocusRingRects>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawGlyphs:
            return decodeAndCreate<WebCore::DisplayList::DrawGlyphs>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawImage:
            return decodeAndCreate<WebCore::DisplayList::DrawImage>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawLinesForText:
            return decodeAndCreate<WebCore::DisplayList::DrawLinesForText>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawNativeImage:
            return decodeAndCreate<WebCore::DisplayList::DrawNativeImage>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawPath:
            return decodeAndCreate<WebCore::DisplayList::DrawPath>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawPattern:
            return decodeAndCreate<WebCore::DisplayList::DrawPattern>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawTiledImage:
            return decodeAndCreate<WebCore::DisplayList::DrawTiledImage>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::DrawTiledScaledImage:
            return decodeAndCreate<WebCore::DisplayList::DrawTiledScaledImage>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::FillCompositedRect:
            return decodeAndCreate<WebCore::DisplayList::FillCompositedRect>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::FillPath:
            return decodeAndCreate<WebCore::DisplayList::FillPath>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::FillRectWithColor:
            return decodeAndCreate<WebCore::DisplayList::FillRectWithColor>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::FillRectWithGradient:
            return decodeAndCreate<WebCore::DisplayList::FillRectWithGradient>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::FillRectWithRoundedHole:
            return decodeAndCreate<WebCore::DisplayList::FillRectWithRoundedHole>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::FillRoundedRect:
            return decodeAndCreate<WebCore::DisplayList::FillRoundedRect>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::PutImageData:
            return decodeAndCreate<WebCore::DisplayList::PutImageData>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::SetLineDash:
            return decodeAndCreate<WebCore::DisplayList::SetLineDash>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::SetState:
            return decodeAndCreate<WebCore::DisplayList::SetState>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::StrokePath:
            return decodeAndCreate<WebCore::DisplayList::StrokePath>(data, length, handleLocation);
        case WebCore::DisplayList::ItemType::ApplyDeviceScaleFactor:
#if USE(CG)
        case WebCore::DisplayList::ItemType::ApplyFillPattern:
        case WebCore::DisplayList::ItemType::ApplyStrokePattern:
#endif
        case WebCore::DisplayList::ItemType::BeginTransparencyLayer:
        case WebCore::DisplayList::ItemType::ClearRect:
        case WebCore::DisplayList::ItemType::ClearShadow:
        case WebCore::DisplayList::ItemType::Clip:
        case WebCore::DisplayList::ItemType::ClipOut:
        case WebCore::DisplayList::ItemType::ConcatenateCTM:
        case WebCore::DisplayList::ItemType::DrawDotsForDocumentMarker:
        case WebCore::DisplayList::ItemType::DrawEllipse:
        case WebCore::DisplayList::ItemType::DrawImageBuffer:
        case WebCore::DisplayList::ItemType::DrawLine:
        case WebCore::DisplayList::ItemType::DrawRect:
        case WebCore::DisplayList::ItemType::EndTransparencyLayer:
        case WebCore::DisplayList::ItemType::FillEllipse:
#if ENABLE(INLINE_PATH_DATA)
        case WebCore::DisplayList::ItemType::FillInlinePath:
#endif
        case WebCore::DisplayList::ItemType::FillRect:
        case WebCore::DisplayList::ItemType::PaintFrameForMedia:
        case WebCore::DisplayList::ItemType::Restore:
        case WebCore::DisplayList::ItemType::Rotate:
        case WebCore::DisplayList::ItemType::Save:
        case WebCore::DisplayList::ItemType::Scale:
        case WebCore::DisplayList::ItemType::SetCTM:
        case WebCore::DisplayList::ItemType::SetInlineFillColor:
        case WebCore::DisplayList::ItemType::SetInlineFillGradient:
        case WebCore::DisplayList::ItemType::SetInlineStrokeColor:
        case WebCore::DisplayList::ItemType::SetLineCap:
        case WebCore::DisplayList::ItemType::SetLineJoin:
        case WebCore::DisplayList::ItemType::SetMiterLimit:
        case WebCore::DisplayList::ItemType::SetStrokeThickness:
        case WebCore::DisplayList::ItemType::StrokeEllipse:
#if ENABLE(INLINE_PATH_DATA)
        case WebCore::DisplayList::ItemType::StrokeInlinePath:
#endif
        case WebCore::DisplayList::ItemType::StrokeRect:
        case WebCore::DisplayList::ItemType::Translate: {
            ASSERT_NOT_REACHED();
            break;
        }
        }
        ASSERT_NOT_REACHED();
        return WTF::nullopt;
    }

    template<typename T>
    Optional<WebCore::DisplayList::ItemHandle> WARN_UNUSED_RETURN decodeAndCreate(const uint8_t* data, size_t length, uint8_t* handleLocation)
    {
        if (auto item = IPC::Decoder::decodeSingleObject<T>(data, length)) {
            new (handleLocation + sizeof(WebCore::DisplayList::ItemType)) T(WTFMove(*item));
            return {{ handleLocation }};
        }
        return WTF::nullopt;
    }

    RemoteRenderingBackend& m_remoteRenderingBackend;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
