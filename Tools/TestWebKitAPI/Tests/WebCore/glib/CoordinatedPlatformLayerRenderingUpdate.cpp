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

#include "Helpers/Test.h"
#include <WebCore/Color.h>
#include <WebCore/CoordinatedImageBackingStore.h>
#include <WebCore/CoordinatedPlatformLayer.h>
#include <WebCore/CoordinatedTileManager.h>
#include <WebCore/Damage.h>
#include <WebCore/NativeImage.h>
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

// A parent layer covering the whole surface with a 40x40 child at the origin
// whose contents are a solid color. Main thread and compositor steps are
// driven explicitly:
//   - commit(): is what CoordinatedSceneState::flush() does at the end of a
//     rendering update.
//   - compose(): is what the compositor does.
class RenderingUpdateScene {
public:
    RenderingUpdateScene()
        : m_compositingQueue(WorkQueue::create("RenderingUpdateScene"_s))
        , m_parent(CoordinatedPlatformLayer::create())
        , m_child(CoordinatedPlatformLayer::create())
    {
        assertIsMainThread();
        {
            Locker locker { m_parent->lock() };
            m_parent->setSize({ 800, 600 });
            m_parent->setAnchorPoint({ 0, 0, 0 });
        }
        {
            Locker locker { m_child->lock() };
            m_child->setSize({ 40, 40 });
            m_child->setAnchorPoint({ 0, 0, 0 });
        }
        m_child->setContentsRect({ 0, 0, 40, 40 });
        m_child->setContentsColor(Color::green);
        addChild();
        commit();

        m_compositingQueue->dispatchSync([this] {
            m_surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
            m_root = SkiaCompositingLayer::create();
            m_root->setSize({ 800, 600 });
            m_root->setChildren({ Ref { m_parent->ensureTarget() } });
        });
        compose({ CompositionReason::RenderingUpdate });
    }

    ~RenderingUpdateScene()
    {
        m_compositingQueue->dispatchSync([this] {
            m_child->invalidateTarget();
            m_parent->invalidateTarget();
            m_root = nullptr;
            m_surface = nullptr;
        });
    }

    void setChildColor(const Color& color)
    {
        assertIsMainThread();
        m_child->setContentsColor(color);
    }

    void addChild()
    {
        assertIsMainThread();
        Vector<Ref<CoordinatedPlatformLayer>> children;
        children.append(m_child);
        Locker locker { m_parent->lock() };
        m_parent->setChildren(WTF::move(children));
    }

    void removeChild()
    {
        assertIsMainThread();
        Locker locker { m_parent->lock() };
        m_parent->setChildren({ });
    }

    bool commit()
    {
        assertIsMainThread();
        bool didCommit = m_parent->commitPendingRenderingUpdateProperties();
        didCommit |= m_child->commitPendingRenderingUpdateProperties();
        return didCommit;
    }

    void compose(OptionSet<CompositionReason> reasons)
    {
        m_compositingQueue->dispatchSync([&] {
            for (auto& layer : { m_parent.ptr(), m_child.ptr() }) {
                layer->flushCompositingState(reasons);
                layer->flushScrollingPositionChanges(reasons);
            }
        });
    }

    void invalidateChildTarget()
    {
        m_compositingQueue->dispatchSync([this] {
            m_child->invalidateTarget();
        });
    }

    SkColor childColor()
    {
        SkColor color = SK_ColorTRANSPARENT;
        m_compositingQueue->dispatchSync([&] {
            m_surface->getCanvas()->clear(SK_ColorTRANSPARENT);
            std::optional<Damage> frameDamage;
            m_root->paint(*m_surface->getCanvas(), frameDamage);
            SkPixmap pixmap;
            if (m_surface->peekPixels(&pixmap))
                color = pixmap.getColor(20, 20);
        });
        return color;
    }

private:
    Ref<WorkQueue> m_compositingQueue;
    Ref<CoordinatedPlatformLayer> m_parent;
    Ref<CoordinatedPlatformLayer> m_child;
    RefPtr<SkiaCompositingLayer> m_root;
    sk_sp<SkSurface> m_surface;
};

class CompositionTrackingClient final : public CoordinatedPlatformLayer::Client {
public:
    CompositionTrackingClient()
        : m_tileManager(CoordinatedTileManager::create(nullptr))
    {
    }

    unsigned compositionRequiredCount() const { return m_compositionRequiredCount; }
    void didFinishRenderingUpdate() { m_compositionRequired = false; }

private:
    void attachLayer(CoordinatedPlatformLayer&) final { }
    void detachLayer(CoordinatedPlatformLayer&) final { }
    CoordinatedTileManager& tileManager() const final { return *m_tileManager; }
    Ref<CoordinatedImageBackingStore> imageBackingStore(Ref<NativeImage>&&) final { RELEASE_ASSERT_NOT_REACHED(); }
    void notifyCompositionRequired() final
    {
        m_compositionRequired = true;
        ++m_compositionRequiredCount;
    }
    bool isCompositionRequiredOrOngoing() const final { return m_compositionRequired; }
    void requestComposition(CompositionReason) final { }
    void didChangeLayerPendingScrollingPosition(CoordinatedPlatformLayer&) final { }
    void didApplyLayerScrollingPositions() final { }
    RunLoop* compositingRunLoop() const final { return nullptr; }
    int maxTextureSize() const final { return 4096; }
    void willPaintTile() final { }
    void didPaintTile() final { }
    void didPaintPreRenderedTile(CoordinatedPlatformLayer&) final { }

    std::unique_ptr<CoordinatedTileManager> m_tileManager;
    bool m_compositionRequired { false };
    unsigned m_compositionRequiredCount { 0 };
};

TEST(CoordinatedPlatformLayerRenderingUpdate, ChangesAreNotAppliedUntilCommitted)
{
    WTF::initializeMainThread();

    RenderingUpdateScene scene;
    EXPECT_EQ(scene.childColor(), SK_ColorGREEN);

    scene.setChildColor(Color::red);
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorGREEN);

    EXPECT_TRUE(scene.commit());
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorRED);
}

TEST(CoordinatedPlatformLayerRenderingUpdate, ChangesAfterCommitDoNotAffectTheCommittedUpdate)
{
    WTF::initializeMainThread();

    RenderingUpdateScene scene;
    scene.setChildColor(Color::red);
    EXPECT_TRUE(scene.commit());

    // The main thread keeps going with the next rendering update before the compositor runs.
    scene.setChildColor(Color::blue);
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorRED);

    EXPECT_TRUE(scene.commit());
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorBLUE);
}

TEST(CoordinatedPlatformLayerRenderingUpdate, CommittedChangesAreOnlyAppliedByRenderingUpdateCompositions)
{
    WTF::initializeMainThread();

    RenderingUpdateScene scene;
    scene.setChildColor(Color::red);
    EXPECT_TRUE(scene.commit());

    scene.compose({ CompositionReason::AsyncScrolling });
    EXPECT_EQ(scene.childColor(), SK_ColorGREEN);
    scene.compose({ CompositionReason::Animation });
    EXPECT_EQ(scene.childColor(), SK_ColorGREEN);

    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorRED);
}

TEST(CoordinatedPlatformLayerRenderingUpdate, ChildrenAreSnapshottedAtCommit)
{
    WTF::initializeMainThread();

    RenderingUpdateScene scene;
    scene.removeChild();
    EXPECT_TRUE(scene.commit());

    // Re-attached after the commit: not part of the committed update.
    scene.addChild();
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorTRANSPARENT);

    EXPECT_TRUE(scene.commit());
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_EQ(scene.childColor(), SK_ColorGREEN);
}

TEST(CoordinatedPlatformLayerRenderingUpdate, CommitWithoutChangesDoesNothing)
{
    WTF::initializeMainThread();

    RenderingUpdateScene scene;
    EXPECT_FALSE(scene.commit());

    // Setting the current value is not a change.
    scene.setChildColor(Color::green);
    EXPECT_FALSE(scene.commit());

    scene.setChildColor(Color::red);
    EXPECT_TRUE(scene.commit());
    scene.compose({ CompositionReason::RenderingUpdate });
    EXPECT_FALSE(scene.commit());
}

// ScrollingTreeCoordinated::didCompleteRenderingUpdate() asks whether a composition is required
// before CoordinatedSceneState::flush() commits the layer properties, so the setters must report it.
TEST(CoordinatedPlatformLayerRenderingUpdate, ChangesRequireCompositionBeforeCommit)
{
    WTF::initializeMainThread();
    assertIsMainThread();

    CompositionTrackingClient client;
    Ref layer = CoordinatedPlatformLayer::create(client);
    EXPECT_FALSE(layer->isCompositionRequiredOrOngoing());

    // Property also read from the scrolling thread, guarded by the layer lock.
    {
        Locker locker { layer->lock() };
        layer->setPosition({ 0, 100 });
    }
    EXPECT_TRUE(layer->isCompositionRequiredOrOngoing());
    EXPECT_TRUE(layer->commitPendingRenderingUpdateProperties());
    client.didFinishRenderingUpdate();

    // Main thread only property.
    layer->setOpacity(0.5);
    EXPECT_TRUE(layer->isCompositionRequiredOrOngoing());
    EXPECT_TRUE(layer->commitPendingRenderingUpdateProperties());
    client.didFinishRenderingUpdate();

    // Shape clip path, which used to not notify at all.
    layer->setClipPath({ }, WindRule::NonZero);
    EXPECT_TRUE(layer->isCompositionRequiredOrOngoing());
    EXPECT_TRUE(layer->commitPendingRenderingUpdateProperties());
    client.didFinishRenderingUpdate();

    EXPECT_FALSE(layer->commitPendingRenderingUpdateProperties());
    EXPECT_FALSE(layer->isCompositionRequiredOrOngoing());
}

TEST(CoordinatedPlatformLayerRenderingUpdate, ChangesNotifyOncePerRenderingUpdate)
{
    WTF::initializeMainThread();
    assertIsMainThread();

    CompositionTrackingClient client;
    Ref layer = CoordinatedPlatformLayer::create(client);

    layer->setOpacity(0.5);
    layer->setBackgroundColor(Color::red);
    EXPECT_EQ(client.compositionRequiredCount(), 1u);

    EXPECT_TRUE(layer->commitPendingRenderingUpdateProperties());
    client.didFinishRenderingUpdate();

    layer->setOpacity(1);
    EXPECT_EQ(client.compositionRequiredCount(), 2u);
}

} // namespace TestWebKitAPI
