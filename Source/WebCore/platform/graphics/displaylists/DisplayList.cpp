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
#include "DisplayList.h"

#include "DecomposedGlyphs.h"
#include "DisplayListResourceHeap.h"
#include "Filter.h"
#include "Font.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

Ref<DisplayList> DisplayList::create(RenderingResourceIdentifier renderingResourceIdentifier)
{
    return adoptRef(*new DisplayList(renderingResourceIdentifier));
}

Ref<DisplayList> DisplayList::create(Vector<Item>&& items, ResourceHeap&& resourceHeap, RenderingResourceIdentifier renderingResourceIdentifier)
{
    return adoptRef(*new DisplayList(WTFMove(items), WTFMove(resourceHeap), renderingResourceIdentifier));
}

DisplayList::DisplayList(RenderingResourceIdentifier renderingResourceIdentifier)
    : RenderingResource(renderingResourceIdentifier)
{
}

DisplayList::DisplayList(Vector<Item>&& items, ResourceHeap&& resourceHeap, RenderingResourceIdentifier renderingResourceIdentifier)
    : RenderingResource(renderingResourceIdentifier)
    , m_items(WTFMove(items))
    , m_resourceHeap(WTFMove(resourceHeap))
{
}

void DisplayList::append(Item&& item)
{
    m_items.append(WTFMove(item));
}

void DisplayList::shrinkToFit()
{
    m_items.shrinkToFit();
}

void DisplayList::clear()
{
    m_items.clear();
    m_resourceHeap.clearAllResources();
}

bool DisplayList::isEmpty() const
{
    return m_items.isEmpty();
}

Vector<RenderingResourceIdentifier> DisplayList::resourceIdentifiers() const
{
    Vector<RenderingResourceIdentifier> resourceIdentifiers;
    auto resourceIdentifiersRange = m_resourceHeap.resourceIdentifiers();
    resourceIdentifiers.appendRange(resourceIdentifiersRange.begin(), resourceIdentifiersRange.end());
    return resourceIdentifiers;
}

void DisplayList::cacheImageBuffer(ImageBuffer& imageBuffer)
{
    m_resourceHeap.add(Ref { imageBuffer });
}

void DisplayList::cacheNativeImage(NativeImage& image)
{
    m_resourceHeap.add(Ref<RenderingResource> { image });
}

void DisplayList::cacheFont(Font& font)
{
    m_resourceHeap.add(Ref { font });
}

void DisplayList::cacheDecomposedGlyphs(DecomposedGlyphs& decomposedGlyphs)
{
    m_resourceHeap.add(Ref<RenderingResource> { decomposedGlyphs });
}

void DisplayList::cacheGradient(Gradient& gradient)
{
    m_resourceHeap.add(Ref<RenderingResource> { gradient });
}

void DisplayList::cacheFilter(Filter& filter)
{
    m_resourceHeap.add(Ref<RenderingResource> { filter });
}

void DisplayList::cacheDisplayList(DisplayList& displayList)
{
    RELEASE_ASSERT(&displayList != this);
    m_resourceHeap.add(Ref<RenderingResource> { displayList });
}

ReplayResult DisplayList::replay(GraphicsContext& context, const FloatRect& initialClip, bool trackReplayList) const
{
    LOG_WITH_STREAM(DisplayLists, stream << "\nReplaying with clip " << initialClip);
    UNUSED_PARAM(initialClip);

    RefPtr<DisplayList> replayList;
    if (UNLIKELY(trackReplayList))
        replayList = DisplayList::create();

#if !LOG_DISABLED
    size_t i = 0;
#endif
    ReplayResult result;
    for (auto& item : m_items) {
        LOG_WITH_STREAM(DisplayLists, stream << "applying " << i++ << " " << item);

        auto applyResult = applyItem(context, m_resourceHeap, item);
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

String DisplayList::asText(OptionSet<AsTextFlag> flags) const
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    for (const auto& item : m_items) {
        if (!shouldDumpItem(item, flags))
            continue;

        TextStream::GroupScope group(stream);
        dumpItem(stream, item, flags);
    }
    return stream.release();
}

void DisplayList::dump(TextStream& ts) const
{
    TextStream::GroupScope group(ts);
    ts << "display list";

    for (const auto& item : m_items) {
        TextStream::GroupScope group(ts);
        dumpItem(ts, item, { AsTextFlag::IncludePlatformOperations, AsTextFlag::IncludeResourceIdentifiers });
    }
}

TextStream& operator<<(TextStream& ts, const DisplayList& displayList)
{
    displayList.dump(ts);
    return ts;
}

} // namespace DisplayList
} // namespace WebCore
