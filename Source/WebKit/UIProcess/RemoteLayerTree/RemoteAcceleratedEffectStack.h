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

#pragma once

#if ENABLE(THREADED_ANIMATION_RESOLUTION)

#include <WebCore/AcceleratedEffect.h>
#include <WebCore/AcceleratedEffectStack.h>
#include <WebCore/AcceleratedEffectValues.h>
#include <WebCore/PlatformCAFilters.h>
#include <WebCore/PlatformLayer.h>
#include <wtf/OptionSet.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS CAPresentationModifierGroup;
OBJC_CLASS CAPresentationModifier;

namespace WebKit {

class RemoteAcceleratedEffectStack final : public WebCore::AcceleratedEffectStack {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(RemoteAcceleratedEffectStack);
public:
    static Ref<RemoteAcceleratedEffectStack> create(WebCore::FloatRect, Seconds);

    void setEffects(WebCore::AcceleratedEffects&&) final;

#if PLATFORM(MAC)
    void initEffectsFromMainThread(PlatformLayer*, MonotonicTime now);
    void applyEffectsFromScrollingThread(MonotonicTime now) const;
#endif

    void applyEffectsFromMainThread(PlatformLayer*, MonotonicTime now, bool backdropRootIsOpaque) const;

    void clear(PlatformLayer*);

private:
    explicit RemoteAcceleratedEffectStack(WebCore::FloatRect, Seconds);

    WebCore::AcceleratedEffectValues computeValues(MonotonicTime now) const;

#if PLATFORM(MAC)
    const WebCore::FilterOperations* longestFilterList() const;
#endif

    enum class LayerProperty : uint8_t {
        Opacity = 1 << 1,
        Transform = 1 << 2,
        Filter = 1 << 3
    };

    OptionSet<LayerProperty> m_affectedLayerProperties;

    WebCore::FloatRect m_bounds;
    Seconds m_acceleratedTimelineTimeOrigin;

#if PLATFORM(MAC)
    RetainPtr<CAPresentationModifierGroup> m_presentationModifierGroup;
    RetainPtr<CAPresentationModifier> m_opacityPresentationModifier;
    RetainPtr<CAPresentationModifier> m_transformPresentationModifier;
    Vector<WebCore::TypedFilterPresentationModifier> m_filterPresentationModifiers;
#endif
};

} // namespace WebKit

#endif // ENABLE(THREADED_ANIMATION_RESOLUTION)
