//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image.h: Defines the egl::Image class representing the EGLimage object.

#ifndef LIBANGLE_IMAGE_H_
#define LIBANGLE_IMAGE_H_

#include "common/angleutils.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/formatutils.h"

#include <set>

namespace rx
{
class EGLImplFactory;
class ImageImpl;
}

namespace egl
{
class Image;

// Only currently Renderbuffers and Textures can be bound with images. This makes the relationship
// explicit, and also ensures that an image sibling can determine if it's been initialized or not,
// which is important for the robust resource init extension with Textures and EGLImages.
class ImageSibling : public gl::RefCountObject, public gl::FramebufferAttachmentObject
{
  public:
    ImageSibling(GLuint id);
    ~ImageSibling() override;

    bool isEGLImageTarget() const;
    gl::InitState sourceEGLImageInitState() const;
    void setSourceEGLImageInitState(gl::InitState initState) const;

  protected:
    // Set the image target of this sibling
    void setTargetImage(const gl::Context *context, egl::Image *imageTarget);

    // Orphan all EGL image sources and targets
    gl::Error orphanImages(const gl::Context *context);

  private:
    friend class Image;

    // Called from Image only to add a new source image
    void addImageSource(egl::Image *imageSource);

    // Called from Image only to remove a source image when the Image is being deleted
    void removeImageSource(egl::Image *imageSource);

    std::set<Image *> mSourcesOf;
    gl::BindingPointer<Image> mTargetOf;
};

struct ImageState : private angle::NonCopyable
{
    ImageState(EGLenum target, ImageSibling *buffer, const AttributeMap &attribs);
    ~ImageState();

    gl::ImageIndex imageIndex;
    gl::BindingPointer<ImageSibling> source;
    std::set<ImageSibling *> targets;
};

class Image final : public gl::RefCountObject
{
  public:
    Image(rx::EGLImplFactory *factory,
          EGLenum target,
          ImageSibling *buffer,
          const AttributeMap &attribs);

    gl::Error onDestroy(const gl::Context *context) override;
    ~Image() override;

    const gl::Format &getFormat() const;
    size_t getWidth() const;
    size_t getHeight() const;
    size_t getSamples() const;

    Error initialize();

    rx::ImageImpl *getImplementation() const;

    bool orphaned() const;
    gl::InitState sourceInitState() const;
    void setInitState(gl::InitState initState);

  private:
    friend class ImageSibling;

    // Called from ImageSibling only notify the image that a new target sibling exists for state
    // tracking.
    void addTargetSibling(ImageSibling *sibling);

    // Called from ImageSibling only to notify the image that a sibling (source or target) has
    // been respecified and state tracking should be updated.
    gl::Error orphanSibling(const gl::Context *context, ImageSibling *sibling);

    ImageState mState;
    rx::ImageImpl *mImplementation;
    bool mOrphanedAndNeedsInit;
};
}  // namespace egl

#endif  // LIBANGLE_IMAGE_H_
