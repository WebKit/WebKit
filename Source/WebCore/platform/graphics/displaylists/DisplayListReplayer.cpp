/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "DisplayListReplayer.h"

#include "DisplayListItems.h"
#include "DisplayListIterator.h"
#include "GraphicsContext.h"
#include "InMemoryDisplayList.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Replayer::Replayer(GraphicsContext& context, const DisplayList& displayList, const ImageBufferHashMap* imageBuffers, const NativeImageHashMap* nativeImages, const FontRenderingResourceMap* fonts, ImageBuffer* maskImageBuffer, Delegate* delegate)
    : m_context(context)
    , m_maskImageBuffer(maskImageBuffer)
    , m_displayList(displayList)
    , m_imageBuffers(imageBuffers ? *imageBuffers : m_displayList.imageBuffers())
    , m_nativeImages(nativeImages ? *nativeImages : m_displayList.nativeImages())
    , m_fonts(fonts ? *fonts : m_displayList.fonts())
    , m_delegate(delegate)
{
}

Replayer::~Replayer() = default;

GraphicsContext& Replayer::context() const
{
    return m_maskImageBuffer ? m_maskImageBuffer->context() : m_context;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyImageBufferItem(GraphicsContext& context, const ImageBufferHashMap& imageBuffers, ItemHandle item, Replayer::Delegate* delegate)
{
    auto& imageBufferItem = item.get<T>();
    auto resourceIdentifier = imageBufferItem.imageBufferIdentifier();
    if (auto* imageBuffer = imageBuffers.get(resourceIdentifier)) {
        imageBufferItem.apply(context, *imageBuffer);
        if (delegate)
            delegate->recordResourceUse(resourceIdentifier);
        return std::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyNativeImageItem(GraphicsContext& context, const NativeImageHashMap& nativeImages, ItemHandle item, Replayer::Delegate* delegate)
{
    auto& nativeImageItem = item.get<T>();
    auto resourceIdentifier = nativeImageItem.imageIdentifier();
    if (auto* image = nativeImages.get(resourceIdentifier)) {
        nativeImageItem.apply(context, *image);
        if (delegate)
            delegate->recordResourceUse(resourceIdentifier);
        return std::nullopt;
    }
    return resourceIdentifier;
}

inline static std::optional<RenderingResourceIdentifier> applySetStateItem(GraphicsContext& context, const NativeImageHashMap& nativeImages, ItemHandle item, Replayer::Delegate* delegate)
{
    auto& setStateItem = item.get<SetState>();

    RenderingResourceIdentifier strokePatternRenderingResourceIdentifier;
    NativeImage* strokePatternImage = nullptr;
    RenderingResourceIdentifier fillPatternRenderingResourceIdentifier;
    NativeImage* fillPatternImage = nullptr;

    if ((strokePatternRenderingResourceIdentifier = setStateItem.strokePatternImageIdentifier())) {
        strokePatternImage = nativeImages.get(strokePatternRenderingResourceIdentifier);
        if (!strokePatternImage)
            return strokePatternRenderingResourceIdentifier;
    }

    if ((fillPatternRenderingResourceIdentifier = setStateItem.fillPatternImageIdentifier())) {
        fillPatternImage = nativeImages.get(fillPatternRenderingResourceIdentifier);
        if (!fillPatternImage)
            return fillPatternRenderingResourceIdentifier;
    }

    setStateItem.apply(context, strokePatternImage, fillPatternImage);

    if (!delegate)
        return std::nullopt;

    if (strokePatternRenderingResourceIdentifier)
        delegate->recordResourceUse(strokePatternRenderingResourceIdentifier);
    if (fillPatternRenderingResourceIdentifier)
        delegate->recordResourceUse(fillPatternRenderingResourceIdentifier);
    return std::nullopt;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyFontItem(GraphicsContext& context, const FontRenderingResourceMap& fonts, ItemHandle item, Replayer::Delegate* delegate)
{
    auto& fontItem = item.get<T>();
    auto resourceIdentifier = fontItem.fontIdentifier();
    if (auto* font = fonts.get(resourceIdentifier)) {
        fontItem.apply(context, *font);
        if (delegate)
            delegate->recordResourceUse(resourceIdentifier);
        return std::nullopt;
    }
    return resourceIdentifier;
}

std::pair<std::optional<StopReplayReason>, std::optional<RenderingResourceIdentifier>> Replayer::applyItem(ItemHandle item)
{
    if (m_delegate && m_delegate->apply(item, context()))
        return { std::nullopt, std::nullopt };

    if (item.is<DrawImageBuffer>()) {
        if (auto missingCachedResourceIdentifier = applyImageBufferItem<DrawImageBuffer>(context(), m_imageBuffers, item, m_delegate))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<ClipToImageBuffer>()) {
        if (auto missingCachedResourceIdentifier = applyImageBufferItem<ClipToImageBuffer>(context(), m_imageBuffers, item, m_delegate))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<DrawNativeImage>()) {
        if (auto missingCachedResourceIdentifier = applyNativeImageItem<DrawNativeImage>(context(), m_nativeImages, item, m_delegate))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<DrawGlyphs>()) {
        if (auto missingCachedResourceIdentifier = applyFontItem<DrawGlyphs>(context(), m_fonts, item, m_delegate))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<DrawPattern>()) {
        if (auto missingCachedResourceIdentifier = applyNativeImageItem<DrawPattern>(context(), m_nativeImages, item, m_delegate))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<SetState>()) {
        if (auto missingCachedResourceIdentifier = applySetStateItem(context(), m_nativeImages, item, m_delegate))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<BeginClipToDrawingCommands>()) {
        if (m_maskImageBuffer)
            return { StopReplayReason::InvalidItemOrExtent, std::nullopt };
        auto& clipItem = item.get<BeginClipToDrawingCommands>();
        m_maskImageBuffer = ImageBuffer::createCompatibleBuffer(clipItem.destination().size(), clipItem.colorSpace(), m_context);
        if (!m_maskImageBuffer)
            return { StopReplayReason::OutOfMemory, std::nullopt };
        if (m_delegate)
            m_delegate->didCreateMaskImageBuffer(*m_maskImageBuffer);
        return { std::nullopt, std::nullopt };
    }

    if (item.is<EndClipToDrawingCommands>()) {
        if (!m_maskImageBuffer)
            return { StopReplayReason::InvalidItemOrExtent, std::nullopt };
        auto& clipItem = item.get<EndClipToDrawingCommands>();
        m_context.clipToImageBuffer(*m_maskImageBuffer, clipItem.destination());
        m_maskImageBuffer = nullptr;
        if (m_delegate)
            m_delegate->didResetMaskImageBuffer();
        return { std::nullopt, std::nullopt };
    }

    item.apply(context());
    return { std::nullopt, std::nullopt };
}

ReplayResult Replayer::replay(const FloatRect& initialClip, bool trackReplayList)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nReplaying with clip " << initialClip);
    UNUSED_PARAM(initialClip);

    std::unique_ptr<InMemoryDisplayList> replayList;
    if (UNLIKELY(trackReplayList))
        replayList = makeUnique<InMemoryDisplayList>();

#if !LOG_DISABLED
    size_t i = 0;
#endif
    ReplayResult result;
    for (auto displayListItem : m_displayList) {
        if (!displayListItem) {
            result.reasonForStopping = StopReplayReason::InvalidItemOrExtent;
            break;
        }

        auto [item, extent, itemSizeInBuffer] = displayListItem.value();

        if (!initialClip.isZero() && extent && !extent->intersects(initialClip)) {
            LOG_WITH_STREAM(DisplayLists, stream << "skipping " << i++ << " " << item);
            result.numberOfBytesRead += itemSizeInBuffer;
            continue;
        }

        LOG_WITH_STREAM(DisplayLists, stream << "applying " << i++ << " " << item);

        if (item.is<MetaCommandChangeDestinationImageBuffer>()) {
            result.numberOfBytesRead += itemSizeInBuffer;
            result.reasonForStopping = StopReplayReason::ChangeDestinationImageBuffer;
            result.nextDestinationImageBuffer = item.get<MetaCommandChangeDestinationImageBuffer>().identifier();
            break;
        }

        if (auto [reasonForStopping, missingCachedResourceIdentifier] = applyItem(item); reasonForStopping) {
            result.reasonForStopping = *reasonForStopping;
            result.missingCachedResourceIdentifier = WTFMove(missingCachedResourceIdentifier);
            break;
        }

        result.numberOfBytesRead += itemSizeInBuffer;

        if (UNLIKELY(trackReplayList)) {
            replayList->append(item);
            if (item.isDrawingItem())
                replayList->addDrawingItemExtent(WTFMove(extent));
        }
    }

    result.trackedDisplayList = WTFMove(replayList);
    return result;
}

} // namespace DisplayList
} // namespace WebCore
