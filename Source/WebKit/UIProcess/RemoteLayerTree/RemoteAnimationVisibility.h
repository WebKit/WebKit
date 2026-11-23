/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(THREADED_ANIMATIONS)

#include <WebCore/PlatformLayerIdentifier.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS CALayer;

namespace WebKit {

class RemoteAnimationStack;
class RemoteLayerTreeNode;

// Determines whether the content of animated layers may be visible within the web view.
class RemoteAnimationVisibility {
public:
    using AnimationStackForNode = Function<RefPtr<const RemoteAnimationStack>(const RemoteLayerTreeNode&)>;

    struct AnimatedLayer {
        WebCore::PlatformLayerIdentifier layerID;
        RefPtr<RemoteLayerTreeNode> node;
        Ref<const RemoteAnimationStack> animationStack;
    };

    void setNeedsUpdate() { m_needsUpdate = true; }
    bool needsUpdate() const { return m_needsUpdate; }

    // Determines which of the provided animated layers may be visible and returns those targeted
    // by monotonic animations that may be newly visible.
    Vector<AnimatedLayer> update(CALayer *webViewLayer, const Vector<AnimatedLayer>&, NOESCAPE const AnimationStackForNode&);

    bool mayBeVisible(WebCore::PlatformLayerIdentifier layerID) const { return m_layerIDsThatMayBeVisible.contains(layerID); }
    bool hasVisibleMonotonicAnimations() const { return m_hasVisibleMonotonicAnimations; }
    bool hasHighImpactMonotonicAnimationsThatMayBeVisible() const { return m_hasHighImpactMonotonicAnimationsThatMayBeVisible; }
    void remove(WebCore::PlatformLayerIdentifier layerID) { m_layerIDsThatMayBeVisible.remove(layerID); }

private:
    using VisibilityCache = HashMap<WebCore::PlatformLayerIdentifier, bool>;
    static bool animatedContentMayBeVisible(const RemoteLayerTreeNode&, CALayer *webViewLayer, VisibilityCache&, NOESCAPE const AnimationStackForNode&);
    static bool computeAnimatedContentMayBeVisible(const RemoteLayerTreeNode&, CALayer *webViewLayer, VisibilityCache&, NOESCAPE const AnimationStackForNode&);

    HashSet<WebCore::PlatformLayerIdentifier> m_layerIDsThatMayBeVisible;
    bool m_hasVisibleMonotonicAnimations { true };
    bool m_hasHighImpactMonotonicAnimationsThatMayBeVisible { true };
    bool m_needsUpdate { false };
};

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATIONS)
