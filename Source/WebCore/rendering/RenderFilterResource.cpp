/*
 * Copyright (C) 2025 Apple Inc.  All rights reserved.
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
#include "RenderFilterResource.h"

#include "Filter.h"
#include "FilterResults.h"
#include "GraphicsContextSwitcher.h"
#include "Logging.h"

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(FilterClient);

SwitcherState RenderFilterResource::beginDrawSourceImage(RenderElement& renderer, const FloatRect& targetBoundingBox, std::optional<FloatRect> clipRect, GraphicsContext*& context)
{
    ASSERT(context);

    LOG(Filters, "RenderFilterResource %p apply renderer %p", this, &renderer);

    if (auto* filterClient = m_rendererFilterClientCache.get(renderer)) {
        auto& targetSwitcher = filterClient->targetSwitcher;
        ASSERT(targetSwitcher);

        if (targetSwitcher->state() == SwitcherState::PaintingSource) {
            targetSwitcher->setState(SwitcherState::CycleDetected);
            return SwitcherState::CycleDetected;
        }

        auto state = targetSwitcher->beginDrawSourceImage(*context, clipRect);
        if (state != SwitcherState::PaintingSource)
            return state;

        filterClient->savedContext = context;
        context = targetSwitcher->drawingContext(*context);
        return state;
    }

    auto createResult = createFilter(renderer, targetBoundingBox, *context);
    if (!createResult)
        return SwitcherState::None;

    auto& filter = std::get<Ref<Filter>>(*createResult);
    auto& sourceImageRect = std::get<FloatRect>(*createResult);

    auto& filterResults = filter->ensureResults([&]() {
        return makeUnique<FilterResults>();
    });

    auto colorSpace = operatingColorSpace();

    auto targetSwitcher = GraphicsContextSwitcher::create(*context, colorSpace, sourceImageRect, Ref { filter }, &filterResults);
    if (!targetSwitcher)
        return SwitcherState::None;

    auto savedContext = context;

    auto state = targetSwitcher->beginDrawSourceImage(*context, clipRect);
    if (state != SwitcherState::PaintingSource)
        return state;

    context = targetSwitcher->drawingContext(*context);

    auto filterClient = makeUnique<FilterClient>(FilterClient { WTFMove(filter), sourceImageRect, WTFMove(targetSwitcher), savedContext, false });
    auto addResult = m_rendererFilterClientCache.set(renderer, WTFMove(filterClient));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);

    return SwitcherState::PaintingSource;
}

SwitcherState RenderFilterResource::endDrawSourceImage(RenderElement& renderer, GraphicsContext*& context)
{
    ASSERT(context);

    auto findResult = m_rendererFilterClientCache.find(renderer);
    if (findResult == m_rendererFilterClientCache.end())
        return SwitcherState::None;

    auto& filterClient = *findResult->value;
    auto& targetSwitcher = filterClient.targetSwitcher;
    ASSERT(targetSwitcher);

    switch (targetSwitcher->state()) {
    case SwitcherState::None:
        RELEASE_ASSERT_NOT_REACHED();
        return SwitcherState::None;

    case SwitcherState::PaintingSource:
        if (!filterClient.savedContext) {
            removeClient(renderer);
            return SwitcherState::None;
        }

        context = filterClient.savedContext;
        filterClient.savedContext = nullptr;
        targetSwitcher->endDrawSourceImage(*context);
        break;

    case SwitcherState::SourcePainted:
        break;

    case SwitcherState::CycleDetected:
        // We have a cycle if we are already applying the data.
        // This can occur due to FeImage referencing a source that makes use of the FEImage itself.
        // This is the first place we've hit the cycle, so set the state back to PaintingSource so the return stack
        // will continue correctly.
        targetSwitcher->setState(SwitcherState::PaintingSource);
        break;
    }

    if (filterClient.markedForRemoval) {
        m_rendererFilterClientCache.remove(findResult);
        return SwitcherState::None;
    }

    return targetSwitcher->state();
}

void RenderFilterResource::applyFilter(RenderElement& renderer, GraphicsContext& context)
{
    if (auto* targetSwitcher = this->targetSwitcher(renderer))
        targetSwitcher->drawOutputImage(context, destinationColorSpace());

    LOG_WITH_STREAM(Filters, stream << "FilterResource " << this << " postApply done\n");
}

bool RenderFilterResource::hasSourceImage(RenderElement& renderer) const
{
    if (auto* targetSwitcher = this->targetSwitcher(renderer))
        return targetSwitcher->hasSourceImage();
    return false;
}

GraphicsContextSwitcher* RenderFilterResource::targetSwitcher(RenderElement& renderer) const
{
    if (auto* filterClient = m_rendererFilterClientCache.get(renderer))
        return filterClient->targetSwitcher.get();
    return nullptr;
}

FloatRect RenderFilterResource::sourceImageRect(RenderElement& renderer) const
{
    if (auto* filterClient = m_rendererFilterClientCache.get(renderer))
        return filterClient->sourceImageRect;
    return { };
}

FloatRect RenderFilterResource::filterRegion(RenderElement& renderer) const
{
    if (auto* filterClient = m_rendererFilterClientCache.get(renderer))
        return filterClient->filter->filterRegion();
    return { };
}

Vector<CheckedRef<RenderElement>> RenderFilterResource::allClients() const
{
    return WTF::map(m_rendererFilterClientCache, [](auto& pair) -> CheckedRef<RenderElement> {
        return { pair.key };
    });
}

void RenderFilterResource::clearEffectResult(FilterEffect& effect)
{
    LOG(Filters, "RenderFilterResource %p didChangeEffect effect %p", this, &effect);

    for (const auto& pair : m_rendererFilterClientCache) {
        const auto& filterClient = *pair.value;

        auto& targetSwitcher = filterClient.targetSwitcher;
        ASSERT(targetSwitcher);

        if (targetSwitcher->state() != SwitcherState::SourcePainted)
            continue;

        // Rebuild the effect result and repaint the image on the screen.
        filterClient.filter->clearEffectResult(effect);
    }
}

void RenderFilterResource::removeClient(RenderElement& client)
{
    LOG(Filters, "RenderFilterResource %p removing client %p", this, &client);

    auto findResult = m_rendererFilterClientCache.find(client);
    if (findResult == m_rendererFilterClientCache.end())
        return;

    auto& filterClient = *findResult->value;
    if (filterClient.savedContext)
        filterClient.markedForRemoval = true;
    else
        m_rendererFilterClientCache.remove(findResult);
}

void RenderFilterResource::removeAllClients()
{
    LOG(Filters, "RenderFilterResource %p removeAllClientsFromCache", this);
    m_rendererFilterClientCache.clear();
}

} // namespace WebCore
