/*
 * Copyright (C) 2026 Igalia S.L.
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

#if ENABLE(DAMAGE_TRACKING) && USE(SKIA)
#include "Helpers/Test.h"
#include <WebCore/Color.h>
#include <WebCore/CoordinatedPlatformLayer.h>
#include <WebCore/Damage.h>
#include <WebCore/SkiaCompositingLayer.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColor.h>
#include <skia/core/SkPixmap.h>
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/MainThread.h>
#include <wtf/WorkQueue.h>

namespace TestWebKitAPI {
using namespace WebCore;

class ScrollingPositionScene {
public:
    ScrollingPositionScene()
        : m_compositingQueue(WorkQueue::create("ScrollingPositionScene"_s))
        , m_layer(CoordinatedPlatformLayer::create())
    {
        {
            Locker locker { m_layer->lock() };
            m_layer->setSize({ 40, 40 });
            m_layer->setAnchorPoint({ 0, 0, 0 });
            m_layer->setContentsColor(Color::green);
            m_layer->setContentsRect({ 0, 0, 40, 40 });
        }
        m_compositingQueue->dispatchSync([this] {
            m_surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
            m_root = SkiaCompositingLayer::create();
            m_root->setSize({ 800, 600 });
            m_layer->flushCompositingState({ CompositionReason::RenderingUpdate });
            m_root->setChildren({ Ref { m_layer->ensureTarget() } });
        });
    }

    ~ScrollingPositionScene()
    {
        m_compositingQueue->dispatchSync([this] {
            m_layer->invalidateTarget();
            m_root = nullptr;
            m_surface = nullptr;
        });
    }

    void setMainPosition(const FloatPoint& position)
    {
        Locker locker { m_layer->lock() };
        m_layer->setPosition(position);
    }

    void commitScrollingPosition(const FloatPoint& position)
    {
        m_layer->setPositionForScrolling(position);
        m_layer->commitPendingScrollingPosition();
    }

    void compose(OptionSet<CompositionReason> reasons)
    {
        m_compositingQueue->dispatchSync([&] {
            m_layer->flushCompositingState(reasons);
            m_layer->flushScrollingPositionChanges(reasons);
        });
    }

    bool isLayerDrawnAt(int y)
    {
        bool drawn = false;
        m_compositingQueue->dispatchSync([&] {
            m_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            std::optional<Damage> frameDamage;
            m_root->paint(*m_surface->getCanvas(), frameDamage);
            SkPixmap pixmap;
            if (m_surface->peekPixels(&pixmap))
                drawn = pixmap.getColor(20, y + 20) == SK_ColorGREEN;
        });
        return drawn;
    }

private:
    Ref<WorkQueue> m_compositingQueue;
    Ref<CoordinatedPlatformLayer> m_layer;
    RefPtr<SkiaCompositingLayer> m_root;
    sk_sp<SkSurface> m_surface;
};

TEST(CoordinatedPlatformLayerScrollingPosition, ScrollingPositionCommittedAfterTheMainPositionWins)
{
    WTF::initializeMainThread();

    ScrollingPositionScene scene;
    scene.setMainPosition({ 0, 100 });
    scene.commitScrollingPosition({ 0, 400 });
    scene.compose({ CompositionReason::RenderingUpdate });

    EXPECT_TRUE(scene.isLayerDrawnAt(400));
    EXPECT_FALSE(scene.isLayerDrawnAt(100));
}

TEST(CoordinatedPlatformLayerScrollingPosition, ScrollingPositionAlreadyComposedWinsOverAnOlderMainPosition)
{
    WTF::initializeMainThread();

    ScrollingPositionScene scene;
    scene.setMainPosition({ 0, 100 });
    scene.commitScrollingPosition({ 0, 400 });
    scene.compose({ CompositionReason::AsyncScrolling });
    EXPECT_TRUE(scene.isLayerDrawnAt(400));

    scene.commitScrollingPosition({ 0, 400 });
    scene.compose({ CompositionReason::RenderingUpdate });

    EXPECT_TRUE(scene.isLayerDrawnAt(400));
    EXPECT_FALSE(scene.isLayerDrawnAt(100));
}

TEST(CoordinatedPlatformLayerScrollingPosition, MainPositionSetAfterTheScrollingPositionWins)
{
    WTF::initializeMainThread();

    ScrollingPositionScene scene;
    scene.commitScrollingPosition({ 0, 400 });
    scene.compose({ CompositionReason::AsyncScrolling });
    EXPECT_TRUE(scene.isLayerDrawnAt(400));

    scene.setMainPosition({ 0, 100 });
    scene.compose({ CompositionReason::RenderingUpdate });

    EXPECT_TRUE(scene.isLayerDrawnAt(100));
    EXPECT_FALSE(scene.isLayerDrawnAt(400));
}

} // namespace TestWebKitAPI

#endif // ENABLE(DAMAGE_TRACKING) && USE(SKIA)
