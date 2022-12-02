/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(DATA_DETECTION) && PLATFORM(MAC)

#import "GraphicsLayerClient.h"
#import "SimpleRange.h"
#import "Timer.h"
#import <wtf/RefCounted.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>

typedef struct __DDHighlight *DDHighlightRef;

namespace WebCore {

class DataDetectorHighlight;
class FloatRect;
class GraphicsContext;
class GraphicsLayer;
class Page;

class DataDetectorHighlightClient : public CanMakeWeakPtr<DataDetectorHighlightClient> {
public:
    virtual ~DataDetectorHighlightClient() = default;

    virtual DataDetectorHighlight* activeHighlight() const = 0;
};

class DataDetectorHighlight : public RefCounted<DataDetectorHighlight>, private GraphicsLayerClient, public CanMakeWeakPtr<DataDetectorHighlight> {
    WTF_MAKE_NONCOPYABLE(DataDetectorHighlight);
public:
    static Ref<DataDetectorHighlight> createForSelection(Page&, DataDetectorHighlightClient&, RetainPtr<DDHighlightRef>&&, SimpleRange&&);
    static Ref<DataDetectorHighlight> createForTelephoneNumber(Page&, DataDetectorHighlightClient&, RetainPtr<DDHighlightRef>&&, SimpleRange&&);
    static Ref<DataDetectorHighlight> createForImageOverlay(Page&, DataDetectorHighlightClient&, RetainPtr<DDHighlightRef>&&, SimpleRange&&);

    ~DataDetectorHighlight() = default;

    void invalidate();

    DDHighlightRef highlight() const { return m_highlight.get(); }
    const SimpleRange& range() const { return m_range; }
    GraphicsLayer& layer() const { return m_graphicsLayer.get(); }

    enum class Type : uint8_t {
        None = 0,
        TelephoneNumber = 1 << 0,
        Selection = 1 << 1,
        ImageOverlay = 1 << 2,
    };

    Type type() const { return m_type; }

    void fadeIn();
    void fadeOut();

    void setHighlight(DDHighlightRef);

private:
    DataDetectorHighlight(Page&, DataDetectorHighlightClient&, Type, RetainPtr<DDHighlightRef>&&, SimpleRange&&);

    // GraphicsLayerClient
    void notifyFlushRequired(const GraphicsLayer*) override;
    void paintContents(const GraphicsLayer*, GraphicsContext&, const FloatRect& inClip, GraphicsLayerPaintBehavior) override;
    float deviceScaleFactor() const override;

    void fadeAnimationTimerFired();
    void startFadeAnimation();
    void didFinishFadeOutAnimation();

    WeakPtr<DataDetectorHighlightClient> m_client;
    WeakPtr<Page> m_page;
    RetainPtr<DDHighlightRef> m_highlight;
    SimpleRange m_range;
    Ref<GraphicsLayer> m_graphicsLayer;
    Type m_type { Type::None };

    Timer m_fadeAnimationTimer;
    WallTime m_fadeAnimationStartTime;

    enum class FadeAnimationState : uint8_t { NotAnimating, FadingIn, FadingOut };
    FadeAnimationState m_fadeAnimationState { FadeAnimationState::NotAnimating };
};

bool areEquivalent(const DataDetectorHighlight*, const DataDetectorHighlight*);

} // namespace WebCore

#endif // ENABLE(DATA_DETECTION) && PLATFORM(MAC)
