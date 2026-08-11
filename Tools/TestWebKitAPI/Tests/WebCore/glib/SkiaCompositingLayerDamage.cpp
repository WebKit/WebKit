/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(DAMAGE_TRACKING) && USE(SKIA)
#include "Helpers/Test.h"
#include <WebCore/Color.h>
#include <WebCore/Damage.h>
#include <WebCore/SkiaCompositingLayer.h>
#include <WebCore/TransformationMatrix.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkColor.h>
#include <skia/core/SkPixmap.h>
#include <skia/core/SkSurface.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

namespace TestWebKitAPI {
using namespace WebCore;

// The damage is collected in device coordinates, while a layer's size is in layer coordinates, which the
// root transform scales to the device by the device pixel ratio. A layer that the scale puts beyond the
// root's own size must still be collected.
TEST(SkiaCompositingLayerDamage, CollectsDamageBeyondTheRootLayerSize)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto root = SkiaCompositingLayer::create();
    root->setDamagePropagationEnabled(true);
    root->setSize({ 400, 300 });

    TransformationMatrix deviceScale;
    deviceScale.scale(2);
    root->setTransform(deviceScale);

    auto child = SkiaCompositingLayer::create();
    child->setDamagePropagationEnabled(true);
    child->setSize({ 64, 64 });
    child->setPosition({ 250, 200 });
    child->setContentsSolidColor(Color::green);

    Vector<Ref<SkiaCompositingLayer>> children;
    children.append(child.copyRef());
    root->setChildren(WTF::move(children));

    std::optional<Damage> frameDamage = Damage(IntSize { 800, 600 });
    root->paint(*surface->getCanvas(), frameDamage);

    // The whole of the child, rather than the part of it that fits within the root's own size, which is
    // what clipping the collecting walk against that size would leave.
    ASSERT_TRUE(frameDamage);
    EXPECT_FALSE(frameDamage->isEmpty());
    EXPECT_EQ(frameDamage->bounds(), IntRect(300, 250, 128, 128));
}

// Builds a root with one masked layer under it. The masked layer paints a solid colour, and its child,
// when one is asked for, paints beyond the masked layer's own bounds.
struct MaskedTree {
    Ref<SkiaCompositingLayer> root;
    Ref<SkiaCompositingLayer> masked;
    Ref<SkiaCompositingLayer> mask;
    RefPtr<SkiaCompositingLayer> child;
};

static MaskedTree createMaskedTree(bool withOverflowingChild)
{
    auto root = SkiaCompositingLayer::create();
    root->setDamagePropagationEnabled(true);
    root->setSize({ 800, 600 });

    auto masked = SkiaCompositingLayer::create();
    masked->setDamagePropagationEnabled(true);
    masked->setSize({ 50, 50 });
    masked->setPosition({ 100, 100 });
    masked->setAnchorPoint({ 0, 0, 0 });
    masked->setContentsSolidColor(Color::green);
    masked->setContentsRect({ 0, 0, 50, 50 });

    auto mask = SkiaCompositingLayer::create();
    mask->setDamagePropagationEnabled(true);
    mask->setSize({ 50, 50 });
    mask->setAnchorPoint({ 0, 0, 0 });
    masked->setMask(mask.copyRef());

    RefPtr<SkiaCompositingLayer> child;
    if (withOverflowingChild) {
        child = SkiaCompositingLayer::create();
        child->setDamagePropagationEnabled(true);
        child->setSize({ 150, 150 });
        child->setAnchorPoint({ 0, 0, 0 });
        child->setContentsSolidColor(Color::blue);
        child->setContentsRect({ 0, 0, 150, 150 });

        Vector<Ref<SkiaCompositingLayer>> grandChildren;
        grandChildren.append(*child);
        masked->setChildren(WTF::move(grandChildren));
    }

    Vector<Ref<SkiaCompositingLayer>> children;
    children.append(masked.copyRef());
    root->setChildren(WTF::move(children));

    return { WTF::move(root), WTF::move(masked), WTF::move(mask), WTF::move(child) };
}

static Damage paintAndCollect(SkiaCompositingLayer& root, SkCanvas& canvas)
{
    std::optional<Damage> frameDamage = Damage(IntSize { 800, 600 });
    root.paint(canvas, frameDamage);
    return WTF::move(*frameDamage);
}

// A mask is never reached by the walk, so a mask that only moves is only noticed because computing the
// transforms reaches it and damages it.
TEST(SkiaCompositingLayerDamage, MovedMaskDamagesTheMaskedLayer)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto tree = createMaskedTree(false);
    paintAndCollect(tree.root, *surface->getCanvas());

    tree.mask->setPosition({ 10, 10 });

    auto damage = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_FALSE(damage.isEmpty());
    EXPECT_EQ(damage.bounds(), IntRect(100, 100, 50, 50));
}

// The mask applies to the whole subtree, so a child painting beyond the masked layer's bounds is masked
// too, and a changed mask has to damage it as well.
TEST(SkiaCompositingLayerDamage, ChangedMaskDamagesChildrenOutsideTheLayerBounds)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto tree = createMaskedTree(true);
    paintAndCollect(tree.root, *surface->getCanvas());

    tree.mask->addDamage(Damage { FloatSize { 50, 50 }, Damage::Mode::Full });

    auto damage = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_FALSE(damage.isEmpty());
    EXPECT_EQ(damage.bounds(), IntRect(100, 100, 150, 150));
}

// A mask that is taken away has no damage of its own to find, so the removal itself has to damage what
// the mask used to apply to, subtree included.
TEST(SkiaCompositingLayerDamage, RemovedMaskDamagesChildrenOutsideTheLayerBounds)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto tree = createMaskedTree(true);
    paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_TRUE(paintAndCollect(tree.root, *surface->getCanvas()).isEmpty());

    tree.masked->setMask(nullptr);

    auto damage = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_FALSE(damage.isEmpty());
    EXPECT_EQ(damage.bounds(), IntRect(100, 100, 150, 150));
}

// Builds a root with one solid colour layer under it, painted twice, so the tree has settled and the
// root has the layer's rect under the id it gave it.
struct SettledTree {
    Ref<SkiaCompositingLayer> root;
    Ref<SkiaCompositingLayer> child;
};

static Ref<SkiaCompositingLayer> createSolidColorLayer(const FloatSize& size, const FloatPoint& position)
{
    auto layer = SkiaCompositingLayer::create();
    layer->setDamagePropagationEnabled(true);
    layer->setSize(size);
    layer->setPosition(position);
    layer->setAnchorPoint({ 0, 0, 0 });
    layer->setContentsSolidColor(Color::green);
    layer->setContentsRect({ 0, 0, size.width(), size.height() });
    return layer;
}

// The target's repaint region is in device coordinates, so the surface size paint() tests it against for
// whole-surface coverage must be device sized too. A layer's own size is in layer coordinates, which the
// root transform scales to the device by the device pixel ratio. Passing the layer size here would treat a
// region that merely spans the layer-sized rect as covering the whole target, dropping the restriction and
// repainting everything. Under a 2x scale the device target is 800x600 while the root's own size is
// 400x300, so a region covering exactly the 400x300 rect is a partial repaint, not a full one.
TEST(SkiaCompositingLayerDamage, PartialRepaintRegionUnderHiDPIRestrictsToTheDamageRect)
{
    constexpr SkColor untouched = SkColorSetARGB(0xFF, 0xFF, 0x00, 0x00);

    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);
    surface->getCanvas()->clear(untouched);

    auto root = SkiaCompositingLayer::create();
    root->setDamagePropagationEnabled(true);
    root->setSize({ 400, 300 });

    TransformationMatrix deviceScale;
    deviceScale.scale(2);
    root->setTransform(deviceScale);

    // Painted from the top-left and scaled up, the child reaches past the damage rect, so a full repaint
    // colours pixels outside the damage while a restricted one leaves them alone.
    auto child = createSolidColorLayer({ 400, 300 }, { 0, 0 });
    Vector<Ref<SkiaCompositingLayer>> children;
    children.append(child.copyRef());
    root->setChildren(WTF::move(children));

    // The damage rect is exactly the root's own 400x300 size, in device coordinates. It does not cover the
    // 800x600 device target, so a draw landing outside it - but still within the surface - must be left out.
    Damage priorTargetDamage(IntSize { 800, 600 });
    priorTargetDamage.add(IntRect { 0, 0, 400, 300 });

    std::optional<Damage> noFrameDamage;
    root->paint(*surface->getCanvas(), noFrameDamage, priorTargetDamage);

    SkPixmap pixmap;
    ASSERT_TRUE(surface->peekPixels(&pixmap));

    // Inside the damage rect the child paints, so the pixel changes. At (500, 350) the child still covers
    // the pixel but the damage does not, so a correct restriction leaves it as it was. Treating the region
    // as whole-surface coverage instead repaints everything and colours it too, which this catches.
    EXPECT_NE(pixmap.getColor(200, 150), untouched);
    EXPECT_EQ(pixmap.getColor(500, 350), untouched);
}

static SettledTree createSettledTree(SkCanvas& canvas, const FloatPoint& position)
{
    auto root = SkiaCompositingLayer::create();
    root->setDamagePropagationEnabled(true);
    root->setSize({ 800, 600 });

    auto child = createSolidColorLayer({ 40, 40 }, position);
    Vector<Ref<SkiaCompositingLayer>> children;
    children.append(child.copyRef());
    root->setChildren(WTF::move(children));

    // The first frame damages the child, since it has never painted. The second leaves it settled.
    paintAndCollect(root, canvas);
    EXPECT_TRUE(paintAndCollect(root, canvas).isEmpty());

    return { WTF::move(root), WTF::move(child) };
}

// A layer that leaves the tree is repainted where it was, once. The root drops its rect at the same
// time, so the frames after it are damaged by nothing at all.
TEST(SkiaCompositingLayerDamage, RemovedLayerIsDamagedOnce)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto tree = createSettledTree(*surface->getCanvas(), { 100, 100 });
    tree.root->setChildren({ });

    auto damage = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_FALSE(damage.isEmpty());
    EXPECT_EQ(damage.bounds(), IntRect(100, 100, 40, 40));

    EXPECT_TRUE(paintAndCollect(tree.root, *surface->getCanvas()).isEmpty());
    EXPECT_TRUE(paintAndCollect(tree.root, *surface->getCanvas()).isEmpty());
}

// The root keeps the rect, so a layer that is gone by the time anyone looks is still repainted where it
// used to be.
TEST(SkiaCompositingLayerDamage, DestroyedLayerIsDamagedWhereItPainted)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto root = SkiaCompositingLayer::create();
    root->setDamagePropagationEnabled(true);
    root->setSize({ 800, 600 });

    {
        auto child = createSolidColorLayer({ 40, 40 }, { 200, 200 });
        Vector<Ref<SkiaCompositingLayer>> children;
        children.append(child.copyRef());
        root->setChildren(WTF::move(children));
        paintAndCollect(root, *surface->getCanvas());
        EXPECT_TRUE(paintAndCollect(root, *surface->getCanvas()).isEmpty());

        // Takes the last reference with it, so nothing is left to be asked where the layer was.
        root->setChildren({ });
    }

    auto damage = paintAndCollect(root, *surface->getCanvas());
    EXPECT_FALSE(damage.isEmpty());
    EXPECT_EQ(damage.bounds(), IntRect(200, 200, 40, 40));

    EXPECT_TRUE(paintAndCollect(root, *surface->getCanvas()).isEmpty());
}

// A hidden layer records no visit, so it is repainted and its rect is dropped. It keeps its id, and the
// rect it gets when it comes back is the one it paints then, not the one it had before it was hidden.
TEST(SkiaCompositingLayerDamage, HiddenLayerComingBackIsDamagedWhereItPaintsNow)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto tree = createSettledTree(*surface->getCanvas(), { 300, 300 });

    tree.child->setContentsVisible(false);
    auto hidden = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_EQ(hidden.bounds(), IntRect(300, 300, 40, 40));
    EXPECT_TRUE(paintAndCollect(tree.root, *surface->getCanvas()).isEmpty());

    tree.child->setPosition({ 500, 400 });
    tree.child->setContentsVisible(true);

    auto returned = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_EQ(returned.bounds(), IntRect(500, 400, 40, 40));
    EXPECT_TRUE(paintAndCollect(tree.root, *surface->getCanvas()).isEmpty());
}

// A layer that moves is repainted where it was and where it now is. The rect kept is the one from the
// last frame alone, so moving again damages the last hop rather than everywhere the layer has been.
TEST(SkiaCompositingLayerDamage, MovedLayerIsDamagedInBothPlaces)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto tree = createSettledTree(*surface->getCanvas(), { 100, 100 });

    tree.child->setPosition({ 300, 100 });
    auto moved = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_EQ(moved.bounds(), IntRect(100, 100, 240, 40));
    EXPECT_TRUE(paintAndCollect(tree.root, *surface->getCanvas()).isEmpty());

    tree.child->setPosition({ 500, 100 });
    paintAndCollect(tree.root, *surface->getCanvas());
    tree.child->setPosition({ 700, 100 });

    auto movedAgain = paintAndCollect(tree.root, *surface->getCanvas());
    EXPECT_EQ(movedAgain.bounds(), IntRect(500, 100, 240, 40));
}

// A replicated layer is walked once per replica, so what it writes down has to be united. Overwriting
// would leave the replica out of the rect the root keeps, and the place it moved away from unpainted.
TEST(SkiaCompositingLayerDamage, ReplicatedLayerIsDamagedWithItsReplica)
{
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(800, 600));
    ASSERT_TRUE(surface);

    auto root = SkiaCompositingLayer::create();
    root->setDamagePropagationEnabled(true);
    root->setSize({ 800, 600 });

    auto replicated = createSolidColorLayer({ 40, 40 }, { 100, 100 });

    auto replica = SkiaCompositingLayer::create();
    replica->setDamagePropagationEnabled(true);
    replica->setSize({ 40, 40 });
    replica->setAnchorPoint({ 0, 0, 0 });
    TransformationMatrix replicaTransform;
    replicaTransform.translate(0, 200);
    replica->setTransform(replicaTransform);
    replicated->setReplica(replica.copyRef());

    Vector<Ref<SkiaCompositingLayer>> children;
    children.append(replicated.copyRef());
    root->setChildren(WTF::move(children));

    // The layer and its replica, 200 apart, rather than either one on its own.
    auto first = paintAndCollect(root, *surface->getCanvas());
    EXPECT_EQ(first.bounds(), IntRect(100, 100, 40, 240));
    EXPECT_TRUE(paintAndCollect(root, *surface->getCanvas()).isEmpty());

    replicated->setPosition({ 300, 100 });
    auto moved = paintAndCollect(root, *surface->getCanvas());
    EXPECT_EQ(moved.bounds(), IntRect(100, 100, 240, 240));
    EXPECT_TRUE(paintAndCollect(root, *surface->getCanvas()).isEmpty());
}

} // namespace TestWebKitAPI

#endif // (PLATFORM(GTK) || PLATFORM(WPE)) && USE(COORDINATED_GRAPHICS) && USE(SKIA) && ENABLE(DAMAGE_TRACKING)
