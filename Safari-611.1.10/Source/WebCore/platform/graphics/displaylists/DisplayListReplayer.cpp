/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "GraphicsContext.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Replayer::Replayer(GraphicsContext& context, const DisplayList& displayList, const ImageBufferHashMap* imageBuffers, const NativeImageHashMap* nativeImages, const FontRenderingResourceMap* fonts, Delegate* delegate)
    : m_context(context)
    , m_displayList(displayList)
    , m_imageBuffers(imageBuffers ? *imageBuffers : m_displayList.imageBuffers())
    , m_nativeImages(nativeImages ? *nativeImages : m_displayList.nativeImages())
    , m_fonts(fonts ? *fonts : m_displayList.fonts())
    , m_delegate(delegate)
{
}

Replayer::~Replayer() = default;

template<class T>
inline static Optional<RenderingResourceIdentifier> applyImageBufferItem(GraphicsContext& context, const ImageBufferHashMap& imageBuffers, ItemHandle item)
{
    auto& imageBufferItem = item.get<T>();
    auto resourceIdentifier = imageBufferItem.imageBufferIdentifier();
    if (auto* imageBuffer = imageBuffers.get(resourceIdentifier)) {
        imageBufferItem.apply(context, *imageBuffer);
        return WTF::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static Optional<RenderingResourceIdentifier> applyNativeImageItem(GraphicsContext& context, const NativeImageHashMap& nativeImages, ItemHandle item)
{
    auto& nativeImageItem = item.get<T>();
    auto resourceIdentifier = nativeImageItem.imageIdentifier();
    if (auto* image = nativeImages.get(resourceIdentifier)) {
        nativeImageItem.apply(context, *image);
        return WTF::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static Optional<RenderingResourceIdentifier> applyFontItem(GraphicsContext& context, const FontRenderingResourceMap& fonts, ItemHandle item)
{
    auto& fontItem = item.get<T>();
    auto resourceIdentifier = fontItem.fontIdentifier();
    if (auto* font = fonts.get(resourceIdentifier)) {
        fontItem.apply(context, *font);
        return WTF::nullopt;
    }
    return resourceIdentifier;
}

std::pair<Optional<StopReplayReason>, Optional<RenderingResourceIdentifier>> Replayer::applyItem(ItemHandle item)
{
    if (m_delegate && m_delegate->apply(item, m_context))
        return { WTF::nullopt, WTF::nullopt };

    if (item.is<DrawImageBuffer>()) {
        if (auto missingCachedResourceIdentifier = applyImageBufferItem<DrawImageBuffer>(m_context, m_imageBuffers, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { WTF::nullopt, WTF::nullopt };
    }

    if (item.is<ClipToImageBuffer>()) {
        if (auto missingCachedResourceIdentifier = applyImageBufferItem<ClipToImageBuffer>(m_context, m_imageBuffers, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { WTF::nullopt, WTF::nullopt };
    }

    if (item.is<DrawNativeImage>()) {
        if (auto missingCachedResourceIdentifier = applyNativeImageItem<DrawNativeImage>(m_context, m_nativeImages, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { WTF::nullopt, WTF::nullopt };
    }

    if (item.is<DrawGlyphs>()) {
        if (auto missingCachedResourceIdentifier = applyFontItem<DrawGlyphs>(m_context, m_fonts, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { WTF::nullopt, WTF::nullopt };
    }

    if (item.is<DrawPattern>()) {
        if (auto missingCachedResourceIdentifier = applyNativeImageItem<DrawPattern>(m_context, m_nativeImages, item))
            return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
        return { WTF::nullopt, WTF::nullopt };
    }

    item.apply(m_context);
    return { WTF::nullopt, WTF::nullopt };
}

ReplayResult Replayer::replay(const FloatRect& initialClip, bool trackReplayList)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nReplaying with clip " << initialClip);
    UNUSED_PARAM(initialClip);

    std::unique_ptr<DisplayList> replayList;
    if (UNLIKELY(trackReplayList))
        replayList = makeUnique<DisplayList>();

#if !LOG_DISABLED
    size_t i = 0;
#endif
    ReplayResult result;
    for (auto [item, extent, itemSizeInBuffer] : m_displayList) {
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
