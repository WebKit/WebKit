/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "DisplayList.h"
#include "DisplayListItems.h"
#include "DisplayListResourceHeap.h"
#include "GraphicsContext.h"
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
inline static std::optional<RenderingResourceIdentifier> applyImageBufferItem(GraphicsContext& context, const ResourceHeap& resourceHeap, const T& item)
{
    auto resourceIdentifier = item.imageBufferIdentifier();
    if (auto* imageBuffer = resourceHeap.getImageBuffer(resourceIdentifier)) {
        item.apply(context, *imageBuffer);
        return std::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applyNativeImageItem(GraphicsContext& context, const ResourceHeap& resourceHeap, const T& item)
{
    auto resourceIdentifier = item.imageIdentifier();
    if (auto* image = resourceHeap.getNativeImage(resourceIdentifier)) {
        item.apply(context, *image);
        return std::nullopt;
    }
    return resourceIdentifier;
}

template<class T>
inline static std::optional<RenderingResourceIdentifier> applySourceImageItem(GraphicsContext& context, const ResourceHeap& resourceHeap, const T& item)
{
    auto resourceIdentifier = item.imageIdentifier();
    if (auto sourceImage = resourceHeap.getSourceImage(resourceIdentifier)) {
        item.apply(context, *sourceImage);
        return std::nullopt;
    }
    return resourceIdentifier;
}

inline static std::optional<RenderingResourceIdentifier> applySetStateItem(GraphicsContext& context, const ResourceHeap& resourceHeap, const SetState& item)
{
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

    if (auto imageIdentifier = fixPatternTileImage(item.state().strokeBrush().pattern()))
        return *imageIdentifier;

    if (auto imageIdentifier = fixPatternTileImage(item.state().fillBrush().pattern()))
        return *imageIdentifier;

    item.apply(context);
    return std::nullopt;
}

inline static std::optional<RenderingResourceIdentifier> applyDrawGlyphs(GraphicsContext& context, const ResourceHeap& resourceHeap, const DrawGlyphs& item)
{
    auto resourceIdentifier = item.fontIdentifier();
    if (auto* font = resourceHeap.getFont(resourceIdentifier)) {
        item.apply(context, *font);
        return std::nullopt;
    }
    return resourceIdentifier;
}

inline static std::optional<RenderingResourceIdentifier> applyDrawDecomposedGlyphs(GraphicsContext& context, const ResourceHeap& resourceHeap, const DrawDecomposedGlyphs& item)
{
    auto fontIdentifier = item.fontIdentifier();
    auto* font = resourceHeap.getFont(fontIdentifier);
    if (!font)
        return fontIdentifier;

    auto drawGlyphsIdentifier = item.decomposedGlyphsIdentifier();
    auto* decomposedGlyphs = resourceHeap.getDecomposedGlyphs(drawGlyphsIdentifier);
    if (!decomposedGlyphs)
        return drawGlyphsIdentifier;

    item.apply(context, *font, *decomposedGlyphs);
    return std::nullopt;
}

auto Replayer::applyItem(const DisplayListItem& item) -> ApplyItemResult
{
    if (!isValid(item))
        return { StopReplayReason::InvalidItemOrExtent, std::nullopt };

    return WTF::switchOn(item,
        [&](const ClipToImageBuffer& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applyImageBufferItem(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const DrawGlyphs& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applyDrawGlyphs(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const DrawDecomposedGlyphs& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applyDrawDecomposedGlyphs(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const DrawImageBuffer& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applyImageBufferItem(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const DrawNativeImage& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applyNativeImageItem<DrawNativeImage>(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const DrawPattern& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applySourceImageItem<DrawPattern>(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const SetState& item) -> ApplyItemResult {
            if (auto missingCachedResourceIdentifier = applySetStateItem(m_context, m_resourceHeap, item))
                return { StopReplayReason::MissingCachedResource, WTFMove(missingCachedResourceIdentifier) };
            return { };
        }, [&](const auto& item) -> ApplyItemResult {
            item.apply(m_context);
            return { };
        }
    );
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
    for (auto& item : m_displayList.items()) {
        LOG_WITH_STREAM(DisplayLists, stream << "applying " << i++ << " " << item);

        auto applyResult = applyItem(item);
        if (applyResult.stopReason) {
            result.reasonForStopping = *applyResult.stopReason;
            result.missingCachedResourceIdentifier = WTFMove(applyResult.resourceIdentifier);
            break;
        }

        if (UNLIKELY(trackReplayList))
            replayList->items().append(item);
    }

    result.trackedDisplayList = WTFMove(replayList);
    return result;
}

} // namespace DisplayList
} // namespace WebCore
