/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCOcclusionTrackerTestCommon_h
#define CCOcclusionTrackerTestCommon_h

#include "IntRect.h"
#include "Region.h"
#include "RenderSurfaceChromium.h"
#include "cc/CCOcclusionTracker.h"
#include "cc/CCRenderSurface.h"

namespace WebKitTests {

// A subclass to expose the total current occlusion.
template<typename LayerType, typename RenderSurfaceType>
class TestCCOcclusionTrackerBase : public WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType> {
public:
    TestCCOcclusionTrackerBase(WebCore::IntRect screenScissorRect, bool recordMetricsForFrame = false)
        : WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>(screenScissorRect, recordMetricsForFrame)
    {
    }

    WebCore::Region occlusionInScreenSpace() const { return WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen; }
    WebCore::Region occlusionInTargetSurface() const { return WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget; }

    void setOcclusionInScreenSpace(const WebCore::Region& region) { WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInScreen = region; }
    void setOcclusionInTargetSurface(const WebCore::Region& region) { WebCore::CCOcclusionTrackerBase<LayerType, RenderSurfaceType>::m_stack.last().occlusionInTarget = region; }
};

typedef TestCCOcclusionTrackerBase<WebCore::LayerChromium, WebCore::RenderSurfaceChromium> TestCCOcclusionTracker;
typedef TestCCOcclusionTrackerBase<WebCore::CCLayerImpl, WebCore::CCRenderSurface> TestCCOcclusionTrackerImpl;

}

#endif // CCOcclusionTrackerTestCommon_h
