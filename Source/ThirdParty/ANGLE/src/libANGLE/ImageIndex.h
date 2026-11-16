//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ImageIndex.h: A helper struct for indexing into an Image array

#ifndef LIBANGLE_IMAGE_INDEX_H_
#define LIBANGLE_IMAGE_INDEX_H_

#include "common/PackedEnums.h"
#include "common/mathutil.h"
#include "libANGLE/angletypes.h"

#include "angle_gl.h"

namespace egl
{
struct ImageSourceAttributes;
}

namespace gl
{

class ImageIndexIterator;

class ImageIndex
{
  public:
    ImageIndex();
    ImageIndex(const ImageIndex &other);
    ImageIndex &operator=(const ImageIndex &other);

    TextureType getType() const { return mType; }
    GLint getLevelIndex() const { return mLevelIndex; }
    GLint getLayerIndex() const { return mLayerIndex; }
    GLint getLayerCount() const { return mLayerCount; }

    bool hasLayer() const;
    bool has3DLayer() const;
    bool usesTex3D() const;
    GLint cubeMapFaceIndex() const;
    bool valid() const;
    // Note that you cannot use this function when the ImageIndex represents an entire level of cube
    // map.
    TextureTarget getTarget() const;
    TextureTarget getTargetOrFirstCubeFace() const;

    bool isLayered() const;
    bool isEntireLevelCubeMap() const;

    static ImageIndex MakeBuffer();
    static ImageIndex Make2D(GLint levelIndex);
    static ImageIndex MakeRectangle(GLint levelIndex);
    static ImageIndex MakeCubeMapFace(TextureTarget target, GLint levelIndex);
    static ImageIndex Make2DArray(GLint levelIndex, GLint layerIndex = kEntireLevel);
    static ImageIndex Make2DArrayRange(GLint levelIndex, GLint layerIndex, GLint layerCount);
    static ImageIndex Make3D(GLint levelIndex, GLint layerIndex = kEntireLevel);
    static ImageIndex MakeFromTarget(TextureTarget target, GLint levelIndex, GLint depth = 0);
    static ImageIndex MakeFromType(TextureType type,
                                   GLint levelIndex,
                                   GLint layerIndex = kEntireLevel,
                                   GLint layerCount = 1);
    static ImageIndex Make2DMultisample();
    static ImageIndex Make2DMultisampleArray(GLint layerIndex = kEntireLevel);
    static ImageIndex Make2DMultisampleArrayRange(GLint layerIndex, GLint layerCount);

    static constexpr GLint kEntireLevel = static_cast<GLint>(-1);

    bool operator<(const ImageIndex &b) const;
    bool operator==(const ImageIndex &b) const;
    bool operator!=(const ImageIndex &b) const;

    // Only valid for 3D/Cube textures with layers.
    ImageIndexIterator getLayerIterator(GLint layerCount) const;

  private:
    friend class ImageIndexIterator;

    ImageIndex(TextureType type, GLint leveIndex, GLint layerIndex, GLint layerCount);

    TextureType mType;
    GLint mLevelIndex;
    GLint mLayerIndex;
    GLint mLayerCount;
};

// To be used like this:
//
// ImageIndexIterator it = ...;
// while (it.hasNext())
// {
//     ImageIndex current = it.next();
// }
class ImageIndexIterator
{
  public:
    ImageIndexIterator(const ImageIndexIterator &other);

    static ImageIndexIterator MakeBuffer();
    static ImageIndexIterator Make2D(GLint minMip, GLint maxMip);
    static ImageIndexIterator MakeRectangle(GLint minMip, GLint maxMip);
    static ImageIndexIterator MakeCube(GLint minMip, GLint maxMip);
    static ImageIndexIterator Make3D(GLint minMip, GLint maxMip, GLint minLayer, GLint maxLayer);
    static ImageIndexIterator Make2DArray(GLint minMip, GLint maxMip, const GLsizei *layerCounts);
    static ImageIndexIterator Make2DMultisample();
    static ImageIndexIterator Make2DMultisampleArray(const GLsizei *layerCounts);
    static ImageIndexIterator MakeGeneric(TextureType type,
                                          GLint minMip,
                                          GLint maxMip,
                                          GLint minLayer,
                                          GLint maxLayer);

    ImageIndex next();
    ImageIndex current() const;
    bool hasNext() const;

  private:
    ImageIndexIterator(TextureType type,
                       const Range<GLint> &mipRange,
                       const Range<GLint> &layerRange,
                       const GLsizei *layerCounts);

    GLint maxLayer() const;

    const Range<GLint> mMipRange;
    const Range<GLint> mLayerRange;
    const GLsizei *const mLayerCounts;

    ImageIndex mCurrentIndex;
};

TextureTarget TextureTypeToTarget(TextureType type, GLint layerIndex);

// A |Texture| can have a number of levels and layers.  An EGL image may be created out of the
// texture, viewing a specific level and layer specified by EGL_GL_TEXTURE_LEVEL_KHR and
// EGL_GL_TEXTURE_ZOFFSET_KHR.  The EGL image can then be imported into another texture whose own
// state does not include these offsets (e.g. the texture is accessed at level 0).  It may similarly
// be imported into a renderbuffer that doesn't even have a level and layer state.  It's easy to
// confuse which gl::ImageIndex translates to the (backend's) backing image's level/layer.
//
// For example, consider the source texture as defined as having 4 levels, with base level 2.  The
// EGL image is created with EGL_GL_TEXTURE_LEVEL_KHR = 3, and imported into the target texture
// Then:
//
// * The backing image (such as VkImage) can be created with 2 levels, corresponding to the source
//   texture's levels 2 and 3.
// * The target texture's base level (always 0) maps to the backing image's level 1, which is 3 (the
//   EGL image level) minus 2 (base level).
//
// To avoid all confusions, two sets of types exist:
//
// * LevelIndex, LayerIndex, and ImageIndex: these contain levels and layers from the point of view
//   of the current texture, be it the source or target texture.  They correspond to the front-end's
//   state, and are passed to the backend.
// * OwnerLevel, OwnerLayer, and OwnerImageIndex: these contain levels and layers corresponding to
//   the source texture (owner of the backing storage).  For the source texture itself, these are
//   equal to the other variants.  For the target texture, they are offset by
//   mEGLImageSourceAttributes.  This property holds for the source texture too, since these offsets
//   are zero.
//
// Before the backend accesses the backing image, it must use the |toOwner*()| helpers in
// TextureState/RenderbufferState to convert from the first set of types to |Owner*| types, where
// the EGL image level and layer offsets are applied, and finally |get()| the translated
// level/layer/image index to interface with the native API.  The backend |TextureBackend| object
// should strive to internally use |Owner*| types before translation to the native API, never
// gl::LevelIndex, gl::LayerIndex, or gl::ImageIndex directly to ensure translation is always done
// and done only once.

// Similar class to LevelIndex.  Notable difference is the default constructor which initializes to
// an invalid value; the fact that this level corresponds to the owner of the backing storage must
// always be explicit.
class OwnerLevel final
{
  public:
    constexpr OwnerLevel() : mLevel(0xFFFFFFFF) {}
    constexpr explicit OwnerLevel(uint32_t ownerLevel) : mLevel(ownerLevel) {}

    // Convenience helpers
    constexpr OwnerLevel operator+(uint32_t offset) const { return OwnerLevel(mLevel + offset); }
    constexpr uint32_t operator-(OwnerLevel other) const
    {
        ASSERT(mLevel >= other.mLevel);
        return mLevel - other.mLevel;
    }
    OwnerLevel &operator++()
    {
        ++mLevel;
        return *this;
    }
    constexpr bool operator<(const OwnerLevel &other) const { return mLevel < other.mLevel; }
    constexpr bool operator<=(const OwnerLevel &other) const { return mLevel <= other.mLevel; }
    constexpr bool operator>(const OwnerLevel &other) const { return mLevel > other.mLevel; }
    constexpr bool operator>=(const OwnerLevel &other) const { return mLevel >= other.mLevel; }
    constexpr bool operator==(const OwnerLevel &other) const { return mLevel == other.mLevel; }
    constexpr bool operator!=(const OwnerLevel &other) const { return mLevel != other.mLevel; }

    constexpr uint32_t get() const { return mLevel; }

  protected:
    uint32_t mLevel;
};
// Similar class to LayerIndex.  Notable difference is the default constructor which initializes to
// an invalid value; the fact that this layer corresponds to the owner of the backing storage must
// always be explicit.
class OwnerLayer final
{
  public:
    constexpr OwnerLayer() : mLayer(0xFFFFFFFF) {}
    constexpr explicit OwnerLayer(uint32_t ownerLayer) : mLayer(ownerLayer) {}

    // Convenience helpers
    constexpr OwnerLayer operator+(uint32_t offset) const { return OwnerLayer(mLayer + offset); }
    OwnerLayer &operator++()
    {
        ++mLayer;
        return *this;
    }

    constexpr bool operator<(const OwnerLayer &other) const { return mLayer < other.mLayer; }
    constexpr bool operator<=(const OwnerLayer &other) const { return mLayer <= other.mLayer; }
    constexpr bool operator>(const OwnerLayer &other) const { return mLayer > other.mLayer; }
    constexpr bool operator>=(const OwnerLayer &other) const { return mLayer >= other.mLayer; }
    constexpr bool operator==(const OwnerLayer &other) const { return mLayer == other.mLayer; }
    constexpr bool operator!=(const OwnerLayer &other) const { return mLayer != other.mLayer; }
    // Helper for loops up to "layer count"
    constexpr bool operator<(uint32_t limit) const { return mLayer < limit; }

    constexpr uint32_t get() const { return mLayer; }

  protected:
    uint32_t mLayer;
};
// Wrapper around |ImageIndex| to reuse its functionality.  Notable difference is that
// |getLayerIndex()| returns 0 if there are no layers, instead of -1 in |ImageIndex|, obviating the
// need for |index.hasLayer() ? index.getLayerIndex() : 0| in many places.
class OwnerImageIndex final
{
  public:
    OwnerImageIndex() = default;

    // Convenience helpers that forward to ImageIndex and possibly wrap the results.
    TextureType getType() const { return mIndex.getType(); }
    OwnerLevel getLevelIndex() const { return OwnerLevel(mIndex.getLevelIndex()); }
    bool hasLayer() const { return mIndex.hasLayer(); }
    bool has3DLayer() const { return mIndex.has3DLayer(); }
    OwnerLayer getLayerIndex() const
    {
        return OwnerLayer(mIndex.hasLayer() ? mIndex.getLayerIndex() : 0);
    }
    OwnerLayer cubeMapFaceIndex() const { return OwnerLayer(mIndex.cubeMapFaceIndex()); }
    uint32_t getLayerCount() const { return mIndex.getLayerCount(); }
    bool usesTex3D() const { return mIndex.usesTex3D(); }
    bool isLayered() const { return mIndex.isLayered(); }
    TextureTarget getTarget() const { return mIndex.getTarget(); }
    TextureTarget getTargetOrFirstCubeFace() const { return mIndex.getTargetOrFirstCubeFace(); }

    static OwnerImageIndex MakeInvalid() { return OwnerImageIndex(ImageIndex{}); }
    static OwnerImageIndex Make2D(OwnerLevel level)
    {
        return OwnerImageIndex(ImageIndex::Make2D(level.get()));
    }
    static OwnerImageIndex MakeCubeMapFace(TextureTarget target, OwnerLevel level)
    {
        return OwnerImageIndex(ImageIndex::MakeCubeMapFace(target, level.get()));
    }
    static OwnerImageIndex Make2DArrayRange(OwnerLevel level, OwnerLayer layer, GLint layerCount)
    {
        return OwnerImageIndex(ImageIndex::Make2DArrayRange(level.get(), layer.get(), layerCount));
    }
    static OwnerImageIndex Make3D(OwnerLevel level, OwnerLayer layer = kEntireLayer)
    {
        return OwnerImageIndex(ImageIndex::Make3D(level.get(), layer.get()));
    }
    static OwnerImageIndex MakeFromTarget(TextureTarget target, OwnerLevel level, GLint depth = 0)
    {
        return OwnerImageIndex(ImageIndex::MakeFromTarget(target, level.get(), depth));
    }
    static OwnerImageIndex MakeFromType(TextureType type,
                                        OwnerLevel level,
                                        OwnerLayer layer = kEntireLayer,
                                        GLint layerCount = 1)
    {
        return OwnerImageIndex(
            ImageIndex::MakeFromType(type, level.get(), layer.get(), layerCount));
    }

    bool operator<(const OwnerImageIndex &b) const { return mIndex < b.mIndex; }
    bool operator==(const OwnerImageIndex &b) const { return mIndex == b.mIndex; }
    bool operator!=(const OwnerImageIndex &b) const { return mIndex != b.mIndex; }

    const ImageIndex &get() const { return mIndex; }

    static constexpr OwnerLayer kEntireLayer = OwnerLayer(ImageIndex::kEntireLevel);

  protected:
    ImageIndex mIndex;
    friend struct egl::ImageSourceAttributes;
    OwnerImageIndex(const ImageIndex &index) : mIndex(index) {}
};

}  // namespace gl

#endif  // LIBANGLE_IMAGE_INDEX_H_
