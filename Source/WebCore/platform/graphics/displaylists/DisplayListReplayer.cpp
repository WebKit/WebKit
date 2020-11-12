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

Replayer::Replayer(GraphicsContext& context, const DisplayList& displayList, const ImageBufferHashMap* imageBuffers, const NativeImageHashMap* nativeImages, Delegate* delegate)
    : m_context(context)
    , m_displayList(displayList)
    , m_imageBuffers(imageBuffers ? *imageBuffers : m_displayList.imageBuffers())
    , m_nativeImages(nativeImages ? *nativeImages : m_displayList.nativeImages())
    , m_delegate(delegate)
{
}

Replayer::~Replayer() = default;

template<class T>
inline static bool applyImageBufferItem(GraphicsContext& context, const ImageBufferHashMap& imageBuffers, ItemHandle item)
{
    if (!item.is<T>())
        return false;
    auto& imageBufferItem = item.get<T>();
    if (auto* imageBuffer = imageBuffers.get(imageBufferItem.imageBufferIdentifier()))
        imageBufferItem.apply(context, *imageBuffer);
    return true;
}

template<class T>
inline static bool applyNativeImageItem(GraphicsContext& context, const NativeImageHashMap& nativeImages, ItemHandle item)
{
    if (!item.is<T>())
        return false;
    auto& nativeImageItem = item.get<T>();
    if (auto* image = nativeImages.get(nativeImageItem.imageIdentifier()))
        nativeImageItem.apply(context, *image);
    return true;
}

void Replayer::applyItem(ItemHandle item)
{
    if (m_delegate && m_delegate->apply(item, m_context))
        return;

    if (applyImageBufferItem<DrawImageBuffer>(m_context, m_imageBuffers, item))
        return;
    if (applyImageBufferItem<ClipToImageBuffer>(m_context, m_imageBuffers, item))
        return;

    if (applyNativeImageItem<DrawNativeImage>(m_context, m_nativeImages, item))
        return;
    if (applyNativeImageItem<DrawPattern>(m_context, m_nativeImages, item))
        return;

    item.apply(m_context);
}

std::unique_ptr<DisplayList> Replayer::replay(const FloatRect& initialClip, bool trackReplayList)
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nReplaying with clip " << initialClip);
    UNUSED_PARAM(initialClip);

    std::unique_ptr<DisplayList> replayList;
    if (UNLIKELY(trackReplayList))
        replayList = makeUnique<DisplayList>();

#if !LOG_DISABLED
    size_t i = 0;
#endif
    for (auto [item, extent] : m_displayList) {
        if (!initialClip.isZero() && extent && !extent->intersects(initialClip)) {
            LOG_WITH_STREAM(DisplayLists, stream << "skipping " << i++ << " " << item);
            continue;
        }

        LOG_WITH_STREAM(DisplayLists, stream << "applying " << i++ << " " << item);
        applyItem(item);

        if (UNLIKELY(trackReplayList)) {
            replayList->append(item);
            if (item.isDrawingItem())
                replayList->addDrawingItemExtent(WTFMove(extent));
        }
    }

    return replayList;
}

} // namespace DisplayList
} // namespace WebCore
