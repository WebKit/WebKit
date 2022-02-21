/*
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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
#include "DisplayListResourceHeap.h"
#include "GraphicsContext.h"
#include "InMemoryDisplayList.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Replayer::Replayer(GraphicsContext& context, const DisplayList& displayList, const ResourceHeap* resourceHeap)
    : m_context(context)
    , m_displayList(displayList)
    , m_resourceHeap(resourceHeap ? *resourceHeap : m_displayList.resourceHeap())
{
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyImageBufferItem(GraphicsContext& context, const ResourceHeap& resourceHeap, ItemHandle item)
{
    auto& imageBufferItem = item.get<T>();
    auto resourceIdentifier = imageBufferItem.imageBufferIdentifier();
    if (auto* imageBuffer = resourceHeap.getImageBuffer(resourceIdentifier)) {
        imageBufferItem.apply(context, *imageBuffer);
        return std::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyNativeImageItem(GraphicsContext& context, const ResourceHeap& resourceHeap, ItemHandle item)
{
    auto& nativeImageItem = item.get<T>();
    auto resourceIdentifier = nativeImageItem.imageIdentifier();
    if (auto* image = resourceHeap.getNativeImage(resourceIdentifier)) {
        nativeImageItem.apply(context, *image);
        return std::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applySourceImageItem(GraphicsContext& context, const ResourceHeap& resourceHeap, ItemHandle item)
{
    auto& sourceImageItem = item.get<T>();
    auto resourceIdentifier = sourceImageItem.imageIdentifier();
    if (auto sourceImage = resourceHeap.getSourceImage(resourceIdentifier)) {
        sourceImageItem.apply(context, *sourceImage);
        return std::nullopt;
    }
    return resourceIdentifier;
}

inline static std::optional<RenderingResourceIdentifier> applySetStateItem(GraphicsContext& context, const ResourceHeap& resourceHeap, ItemHandle item)
{
    auto& setStateItem = item.get<SetState>();

    auto fixPatternTileImage = [&](Pattern* pattern) -> std::optional<RenderingResourceIdentifier> {
        if (!pattern)
            return std::nullopt;

        auto imageIdentifier = pattern->tileImage().imageIdentifier();
        auto sourceImage = resourceHeap.getSourceImage(imageIdentifier);
        if (!sourceImage)
            return imageIdentifier;

        pattern->setTileImage(WTFMove(*sourceImage));
        return std::nullopt;
    };

    if (auto imageIdentifier = fixPatternTileImage(setStateItem.stateChange().m_state.strokePattern.get()))
        return *imageIdentifier;

    if (auto imageIdentifier = fixPatternTileImage(setStateItem.stateChange().m_state.fillPattern.get()))
        return *imageIdentifier;

    setStateItem.apply(context);
    return std::nullopt;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyFontItem(GraphicsContext& context, const ResourceHeap& resourceHeap, ItemHandle item)
{
    auto& fontItem = item.get<T>();
    auto resourceIdentifier = fontItem.fontIdentifier();
    if (auto* font = resourceHeap.getFont(resourceIdentifier)) {
        fontItem.apply(context, *font);
        return std::nullopt;
    }
    return resourceIdentifier;
}

std::pair<std::optional<StopReplayReason>, std::optional<RenderingResourceIdentifier>> Replayer::applyItem(ItemHandle item)
{
    if (item.is<DrawImageBuffer>()) {
        if (auto missingCachedResourceIdentifier = applyImageBufferItem<DrawImageBuffer>(m_context, m_resourceHeap, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<ClipToImageBuffer>()) {
        if (auto missingCachedResourceIdentifier = applyImageBufferItem<ClipToImageBuffer>(m_context, m_resourceHeap, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<DrawNativeImage>()) {
        if (auto missingCachedResourceIdentifier = applyNativeImageItem<DrawNativeImage>(m_context, m_resourceHeap, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<DrawGlyphs>()) {
        if (auto missingCachedResourceIdentifier = applyFontItem<DrawGlyphs>(m_context, m_resourceHeap, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<DrawPattern>()) {
        if (auto missingCachedResourceIdentifier = applySourceImageItem<DrawPattern>(m_context, m_resourceHeap, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    if (item.is<SetState>()) {
        if (auto missingCachedResourceIdentifier = applySetStateItem(m_context, m_resourceHeap, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { std::nullopt, std::nullopt };
    }

    item.apply(m_context);
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
