/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "AcceleratedTimeline.h"

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include "Document.h"
#include "LocalDOMWindow.h"
#include "Page.h"
#include "Performance.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderStyleConstants.h"
#include "Styleable.h"
#include <wtf/MonotonicTime.h>

namespace WebCore {
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AcceleratedTimeline);

AcceleratedTimeline::AcceleratedTimeline(Document& document)
{
    auto now = MonotonicTime::now();
    m_timeOrigin = now.secondsSinceEpoch();
    if (auto* domWindow = document.domWindow())
        m_timeOrigin -= Seconds::fromMilliseconds(domWindow->performance().relativeTimeFromTimeOriginInReducedResolution(now));
}

void AcceleratedTimeline::updateEffectStacks()
{
    auto targetsPendingUpdate = std::exchange(m_targetsPendingUpdate, { });
    for (auto hashedStyleable : targetsPendingUpdate) {
        auto* element = hashedStyleable.first;
        if (!element)
            continue;

        auto pseudoId = static_cast<PseudoId>(hashedStyleable.second);
        Styleable target { *element, pseudoId };

        auto* renderer = dynamicDowncast<RenderLayerModelObject>(target.renderer());
        if (!renderer || !renderer->isComposited())
            continue;

        auto* renderLayer = renderer->layer();
        ASSERT(renderLayer && renderLayer->backing());
        renderLayer->backing()->updateAcceleratedEffectsAndBaseValues();
    }
}

void AcceleratedTimeline::updateEffectStackForTarget(const Styleable& target)
{
    m_targetsPendingUpdate.add({ &target.element, enumToUnderlyingType(target.pseudoId) });
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
