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
#include "AcceleratedEffectStackUpdater.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectStack.h"
#include "AcceleratedTimeline.h"
#include "AcceleratedTimelinesUpdater.h"
#include "DocumentPage.h"
#include "KeyframeEffectStack.h"
#include "NodeDocument.h"
#include "RenderElement.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerModelObject.h"
#include "RenderStyleConstants.h"
#include "ScrollTimeline.h"
#include "Styleable.h"

namespace WebCore {

void AcceleratedEffectStackUpdater::update()
{
    if (!hasTargetsPendingUpdate())
        return;

    RefPtr<Page> page;
    HashSet<Ref<AcceleratedTimeline>> timelinesInUpdate;
    // We keep a list of all the accelerated effect stacks that will change
    // during this update so that the AcceleratedTimeline that were referenced
    // from effects contained in those stacks are kept alive for the duration
    // of this function. Once this function is done, timelines not in use by
    // any remaining effect on any accelerated stack will be released and this
    // will be picked up in `AcceleratedTimelinesUpdater::takeTimelinesUpdate()`
    // to work out a list of destroyed accelerated timelines.
    Vector<RefPtr<const AcceleratedEffectStack>> previousEffectStacks;

    auto targetsPendingUpdate = std::exchange(m_targetsPendingUpdate, { });
    for (auto weakTarget : targetsPendingUpdate) {
        auto target = weakTarget.styleable();
        if (!target)
            continue;

        if (!page)
            page = protect(target->element.document())->page();

        CheckedPtr renderer = dynamicDowncast<RenderLayerModelObject>(target->renderer());
        if (!renderer || !renderer->isComposited())
            continue;

        CheckedPtr renderLayer = renderer->layer();
        ASSERT(renderLayer && renderLayer->backing());
        auto* backing = renderLayer->backing();
        previousEffectStacks.append(protect(backing->acceleratedEffectStack()));
        backing->updateAcceleratedEffectsAndBaseValues(timelinesInUpdate);
    }

    if (page && !timelinesInUpdate.isEmpty())
        page->ensureAcceleratedTimelinesUpdater().processTimelinesSeenDuringEffectStacksUpdate(WTF::move(timelinesInUpdate));
}

void AcceleratedEffectStackUpdater::scheduleUpdateForTarget(const Styleable& target)
{
    m_targetsPendingUpdate.add({ target });
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
