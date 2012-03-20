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

#ifndef CCOverdrawMetrics_h
#define CCOverdrawMetrics_h

#include <wtf/PassOwnPtr.h>

namespace WebCore {
class IntRect;
class TransformationMatrix;
class CCLayerTreeHost;
class CCLayerTreeHostImpl;

// FIXME: compute overdraw metrics only occasionally, not every frame.
class CCOverdrawMetrics {
public:
    static PassOwnPtr<CCOverdrawMetrics> create(bool recordMetricsForFrame) { return adoptPtr(new CCOverdrawMetrics(recordMetricsForFrame)); }

    void didCull(const TransformationMatrix& transformToTarget, const IntRect& beforeCullRect, const IntRect& afterCullRect);
    void didDraw(const TransformationMatrix& transformToTarget, const IntRect& afterCullRect, const IntRect& opaqueRect);

    void recordMetrics(const CCLayerTreeHost*) const;
    void recordMetrics(const CCLayerTreeHostImpl*) const;

    // Accessors for tests.
    float pixelsDrawnOpaque() const { return m_pixelsDrawnOpaque; }
    float pixelsDrawnTranslucent() const { return m_pixelsDrawnTranslucent; }
    float pixelsCulled() const { return m_pixelsCulled; }

private:
    explicit CCOverdrawMetrics(bool recordMetricsForFrame);

    enum MetricsType {
        DRAWING,
        UPLOADING
    };

    template<typename LayerTreeHostType>
    void recordMetricsInternal(MetricsType, const LayerTreeHostType*) const;

    // When false this class is a giant no-op.
    bool m_recordMetricsForFrame;
    // Count of pixels that are opaque (and thus occlude). Ideally this is no more
    // than wiewport width x height.
    float m_pixelsDrawnOpaque;
    // Count of pixels that are possibly translucent, and cannot occlude.
    float m_pixelsDrawnTranslucent;
    // Count of pixels not drawn as they are occluded by somthing opaque.
    float m_pixelsCulled;
};

} // namespace WebCore

#endif
