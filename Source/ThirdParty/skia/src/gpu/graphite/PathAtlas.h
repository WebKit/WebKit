/*
 * Copyright 2023 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_PathAtlas_DEFINED
#define skgpu_graphite_PathAtlas_DEFINED

#include "include/core/SkStrokeRec.h"
#include "src/core/SkAutoPixmapStorage.h"
#include "src/gpu/RectanizerSkyline.h"
#include "src/gpu/graphite/geom/CoverageMaskShape.h"

#ifdef SK_ENABLE_VELLO_SHADERS
#include "src/gpu/graphite/compute/VelloRenderer.h"
#endif

#include <memory>

namespace skgpu::graphite {

class Caps;
class DrawContext;
class Recorder;
class Rect;
class Renderer;
class Shape;
class TextureProxy;
class Transform;

/**
 * PathAtlas manages one or more atlas textures that store coverage masks for path rendering.
 *
 * The contents of a PathAtlas are intended to be transient: atlas regions are considered valid only
 * for the scope of the render passes that sample them. Unlike DrawAtlas, PathAtlas does not
 * necessarily support partial eviction and reuse of subregions. In most subclasses, once an atlas
 * texture is filled up all of its sub-allocations must be invalidated before it can be reused.
 *
 * PathAtlas does not prescribe how atlas contents get uploaded to the GPU. The specific task
 * mechanism is defined by subclasses.
 */
class PathAtlas {
public:
    /**
     * The PathAtlas will use textures of the requested size or the system's maximum texture size,
     * whichever is smaller.
     */
    PathAtlas(Recorder* recorder, uint32_t requestedWidth, uint32_t requestedHeight);
    virtual ~PathAtlas();

    using MaskAndOrigin = std::pair<CoverageMaskShape, SkIPoint>;

    /**
     * Searches the atlas for a slot that can fit a coverage mask for a clipped shape with the given
     * bounds in device coordinates and submits the mask to be drawn into the found atlas region.
     * For atlases that cache coverage masks, will first search the cache before adding.
     *
     * Returns an empty result if a the shape cannot fit in the atlas. Otherwise, returns the
     * CoverageMaskShape (including the texture proxy) for sampling the eventually-rendered coverage
     * mask and the device-space origin the mask should be drawn at (e.g. its recorded draw should
     * be an integer translation matrix), and the Renderer that should be used to draw that shape.
     * The Renderer should have single-channel coverage, require AA bounds outsetting, and have a
     * single renderStep.
     *
     * The bounds of the atlas entry is laid out with a 1 pixel outset from the given dimensions.
     * The returned shape's UV origin accounts for the padding, and its mask size does not include
     * the padding. This allows the mask to be sampled safely with linear filtering without worrying
     * about HW filtering accessing pixels from other entries.
     *
     * `shape` will be drawn after applying the linear components (scale, rotation, skew) of the
     * provided `localToDevice` transform. This is done by  translating the shape by the inverse of
     * the rounded out `transformedShapeBounds` offset. For an unclipped shape this amounts to
     * translating it back to its origin while preserving any sub-pixel translation. For a clipped
     * shape, this ensures that the visible portions of the mask are centered in the atlas slot
     * while invisible portions that would lie outside the atlas slot get clipped out.
     *
     * `addShape()` schedules the shape to be drawn but when and how the rendering happens is
     * specified by the subclass implementation.
     *
     * The stroke-and-fill style is drawn as a single combined coverage mask containing the stroke
     * and the fill.
     */
    std::pair<const Renderer*, std::optional<MaskAndOrigin>> addShape(
            const Rect& transformedShapeBounds,
            const Shape& shape,
            const Transform& localToDevice,
            const SkStrokeRec& style);

    /**
     * Returns true if a path coverage mask with the given device-space bounds is sufficiently
     * small to benefit from atlasing without causing too many atlas renders.
     */
    virtual bool isSuitableForAtlasing(const Rect& transformedShapeBounds) const { return true; }

    uint32_t width() const { return fWidth; }
    uint32_t height() const { return fHeight; }

protected:
    // Subclasses should ensure that the recorded masks have this much padding around each entry.
    // PathAtlas passes in un-padded sizes to onAddShape and assumes that padding has been included
    // in the outPos value.
    static constexpr int kEntryPadding = 1;

    // The 'transform' has been adjusted to draw the Shape into a logical image from (0,0) to
    // 'maskSize'. The actual rendering into the returned TextureProxy will need to be further
    // translated by the value written to 'outPos', which is the responsibility of subclasses.
    virtual const TextureProxy* onAddShape(const Shape&,
                                           const Transform& transform,
                                           const SkStrokeRec&,
                                           skvx::half2 maskSize,
                                           skvx::half2* outPos) = 0;

    // The Recorder that created and owns this Atlas.
    Recorder* fRecorder;

    uint32_t fWidth = 0;
    uint32_t fHeight = 0;
};

class DispatchGroup;

/**
 * Base class for PathAtlas implementations that rasterize coverage masks on the GPU using compute
 * shaders.
 *
 * When a new shape gets added, it gets tracked as input to a series of GPU compute passes. This
 * data is recorded by `recordDispatches()` into a DispatchGroup which can be added to a
 * ComputeTask.
 *
 * After a successful call to `recordDispatches()`, the client is free to call `reset()` and start
 * adding new shapes for a future atlas render.
 */
class ComputePathAtlas : public PathAtlas {
public:
    // Returns the currently preferred ComputePathAtlas implementation.
    static std::unique_ptr<ComputePathAtlas> CreateDefault(Recorder*);

    virtual std::unique_ptr<DispatchGroup> recordDispatches(Recorder*) const = 0;

    // Clear all scheduled atlas draws and free up atlas allocations, if necessary. After this call
    // the atlas can be considered cleared and available for new shape insertions. However this
    // method does not have any bearing on the contents of any atlas textures themselves, which may
    // be in use by GPU commands that are in-flight or yet to be submitted.
    void reset();

protected:
    explicit ComputePathAtlas(Recorder*);

    const TextureProxy* texture() const { return fTexture.get(); }
    const TextureProxy* addRect(skvx::half2 maskSize,
                                SkIPoint16* outPos);
    bool isSuitableForAtlasing(const Rect& transformedShapeBounds) const override;

    virtual void onReset() = 0;

private:
    bool initializeTextureIfNeeded();

    skgpu::RectanizerSkyline fRectanizer;

    // ComputePathAtlas lazily requests a texture from the AtlasProvider when the first shape gets
    // added to it and references the same texture for the duration of its lifetime. A reference to
    // this texture is stored here, which is used by AtlasShapeRenderStep when encoding the render
    // pass.
    sk_sp<TextureProxy> fTexture;
};

}  // namespace skgpu::graphite

#endif  // skgpu_graphite_PathAtlas_DEFINED
