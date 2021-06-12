/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#include "GraphicsTypes.h"
#include "Timer.h"
#include <wtf/HashMap.h>

namespace WebCore {

class GraphicsContext;
class Image;
class LayoutSize;
class RenderBoxModelObject;
class RenderView;
class RenderStyle;

class ImageQualityController {
    WTF_MAKE_NONCOPYABLE(ImageQualityController); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ImageQualityController(const RenderView&);

    static std::optional<InterpolationQuality> interpolationQualityFromStyle(const RenderStyle&);
    InterpolationQuality chooseInterpolationQuality(GraphicsContext&, RenderBoxModelObject*, Image&, const void* layer, const LayoutSize&);

    void rendererWillBeDestroyed(RenderBoxModelObject& renderer) { removeObject(&renderer); }

private:
    typedef HashMap<const void*, LayoutSize> LayerSizeMap;
    typedef HashMap<RenderBoxModelObject*, LayerSizeMap> ObjectLayerSizeMap;

    void removeLayer(RenderBoxModelObject*, LayerSizeMap* innerMap, const void* layer);
    void set(RenderBoxModelObject*, LayerSizeMap* innerMap, const void* layer, const LayoutSize&);
    void highQualityRepaintTimerFired();
    void restartTimer();
    void removeObject(RenderBoxModelObject*);

    const RenderView& m_renderView;
    ObjectLayerSizeMap m_objectLayerSizeMap;
    Timer m_timer;
    bool m_animatedResizeIsActive { false };
    bool m_liveResizeOptimizationIsActive { false };
};

} // namespace WebCore
