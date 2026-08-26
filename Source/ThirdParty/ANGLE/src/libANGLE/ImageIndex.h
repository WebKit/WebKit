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
// To avoid all confusions, some types specific to image siblings are created:
//
// * OwnLevel, OwnLayer and OwnImageIndex: these contain levels and layers from the
//   point of view of the current texture, be it the source or target texture.  They correspond to
//   the front-end's state, and are passed to the backend.
// * SourceLevel, SourceLayer and SourceImageIndex: these contain levels and layers corresponding to
//   the source texture.  For the source texture itself, these are equal to the Own* variants.  For
//   the target texture, they are offset by mEGLImageSourceAttributes.  This property holds for the
//   source texture too, since these offsets are zero.
//
// Before the backend accesses the backing image, it must use the |toSource*()| helpers in
// TextureState/RenderbufferState to convert |Own*| types to |Source*| types, where the EGL image
// level and layer offsets are applied, and finally |get()| the translated level/layer/image index
// to interface with the backing image.  The backend |TextureBackend| object should strive to
// internally pass around either Own* or Source* types, never gl::LevelIndex, uint32_t
// layer or gl::ImageIndex directly to ensure translation is always done and done only once.
// Ideally, the backend would _never_ use the gl::LevelIndex, uint32_t layer and gl::ImageIndex
// directly and would always use either the |Own*| or |Source*| type so there cannot be mistakes.
//
// For backends that don't need this translation, such as the GL backend where the translation is
// done by the driver, use |getUntranslated()| to get the untranslated front-end values for the
// current texture out of |Own*| types.
template <typename T>
class OwnIndex
{
  public:
    explicit OwnIndex(T index) : mIndex(index) {}

    T getUntranslated() const { return mIndex; }

  protected:
    T mIndex;
};

template <typename T>
class SourceIndex
{
  public:
    T get() const { return mIndex; }

  protected:
    friend struct egl::ImageSourceAttributes;
    SourceIndex(T index) : mIndex(index) {}
    T mIndex;
};

// Note: The front-end generally uses uint32_t for levels instead of LevelIndex.  The templated type
// uses LevelIndex so it's different from OwnLayer, but a uint32_t-taking constructor is added for
// convenience.
class OwnLevel : public OwnIndex<LevelIndex>
{
  public:
    explicit OwnLevel(uint32_t level) : OwnIndex(LevelIndex(level)) {}
};
using OwnLayer = OwnIndex<uint32_t>;
class OwnImageIndex : public OwnIndex<ImageIndex>
{
  public:
    explicit OwnImageIndex(const ImageIndex &index) : OwnIndex(index) {}

    // Convenience helpers that forward to ImageIndex and possibly wrap the results.
    TextureType getType() const { return mIndex.getType(); }
    OwnLevel getLevelIndex() const { return OwnLevel(mIndex.getLevelIndex()); }
    bool hasLayer() const { return mIndex.hasLayer(); }
    OwnLayer getLayerIndex() const
    {
        return OwnLayer(mIndex.hasLayer() ? mIndex.getLayerIndex() : 0);
    }
    OwnLayer cubeMapFaceIndex() const { return OwnLayer(mIndex.cubeMapFaceIndex()); }
    uint32_t getLayerCount() const { return mIndex.getLayerCount(); }
    bool usesTex3D() const { return mIndex.usesTex3D(); }
    TextureTarget getTarget() const { return mIndex.getTarget(); }
    TextureTarget getTargetOrFirstCubeFace() const { return mIndex.getTargetOrFirstCubeFace(); }
};

class SourceImageIndex;
class SourceLevel : public SourceIndex<LevelIndex>
{
  public:
    // Convenience helpers
    SourceLevel operator+(uint32_t offset) const { return SourceLevel(mIndex + offset); }

    // Helper while code is being transitioned to using SourceLevel consistently.  Remove once done.
    // TODO(http://anglebug.com/525079760)
    static SourceLevel VerifiedSourceLevel(LevelIndex level) { return SourceLevel(level); }

  protected:
    friend struct egl::ImageSourceAttributes;
    friend class SourceImageIndex;
    SourceLevel(LevelIndex level) : SourceIndex(level) {}
};
class SourceLayer : public SourceIndex<uint32_t>
{
  public:
    // Convenience helpers
    SourceLayer operator+(uint32_t offset) const { return SourceLayer(mIndex + offset); }

  protected:
    friend struct egl::ImageSourceAttributes;
    friend class SourceImageIndex;
    SourceLayer(uint32_t layer) : SourceIndex(layer) {}
};
class SourceImageIndex : public SourceIndex<ImageIndex>
{
  public:
    // Helper while code is being transitioned to using SourceLevel consistently.  Remove once done.
    // TODO(http://anglebug.com/525079760)
    static SourceImageIndex VerifiedSourceIndex(ImageIndex index)
    {
        return SourceImageIndex(index);
    }

    // Convenience helpers that forward to ImageIndex and possibly wrap the results.
    TextureType getType() const { return mIndex.getType(); }
    SourceLevel getLevelIndex() const { return SourceLevel(LevelIndex(mIndex.getLevelIndex())); }
    bool hasLayer() const { return mIndex.hasLayer(); }
    SourceLayer getLayerIndex() const
    {
        return SourceLayer(mIndex.hasLayer() ? mIndex.getLayerIndex() : 0);
    }
    SourceLayer cubeMapFaceIndex() const { return SourceLayer(mIndex.cubeMapFaceIndex()); }
    uint32_t getLayerCount() const { return mIndex.getLayerCount(); }
    bool usesTex3D() const { return mIndex.usesTex3D(); }
    TextureTarget getTarget() const { return mIndex.getTarget(); }
    TextureTarget getTargetOrFirstCubeFace() const { return mIndex.getTargetOrFirstCubeFace(); }

  protected:
    friend struct egl::ImageSourceAttributes;
    SourceImageIndex(const ImageIndex &index) : SourceIndex(index) {}
};

}  // namespace gl

#endif  // LIBANGLE_IMAGE_INDEX_H_
