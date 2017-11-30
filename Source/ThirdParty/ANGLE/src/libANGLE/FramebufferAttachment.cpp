//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FramebufferAttachment.cpp: the gl::FramebufferAttachment class and its derived classes
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4.3 page 108.

#include "libANGLE/FramebufferAttachment.h"

#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/FramebufferAttachmentObjectImpl.h"
#include "libANGLE/renderer/FramebufferImpl.h"

namespace gl
{

namespace
{

std::vector<Offset> TransformViewportOffsetArrayToVectorOfOffsets(const GLint *viewportOffsets,
                                                                  GLsizei numViews)
{
    const size_t numViewsAsSizeT = static_cast<size_t>(numViews);
    std::vector<Offset> offsetVector;
    offsetVector.reserve(numViewsAsSizeT);
    for (size_t i = 0u; i < numViewsAsSizeT; ++i)
    {
        offsetVector.emplace_back(Offset(viewportOffsets[i * 2u], viewportOffsets[i * 2u + 1u], 0));
    }
    return offsetVector;
}

}  // namespace

////// FramebufferAttachment::Target Implementation //////

const GLsizei FramebufferAttachment::kDefaultNumViews         = 1;
const GLenum FramebufferAttachment::kDefaultMultiviewLayout   = GL_NONE;
const GLint FramebufferAttachment::kDefaultBaseViewIndex      = 0;
const GLint FramebufferAttachment::kDefaultViewportOffsets[2] = {0};

std::vector<Offset> FramebufferAttachment::GetDefaultViewportOffsetVector()
{
    return TransformViewportOffsetArrayToVectorOfOffsets(
        FramebufferAttachment::kDefaultViewportOffsets, FramebufferAttachment::kDefaultNumViews);
}

FramebufferAttachment::Target::Target()
    : mBinding(GL_NONE),
      mTextureIndex(ImageIndex::MakeInvalid())
{
}

FramebufferAttachment::Target::Target(GLenum binding, const ImageIndex &imageIndex)
    : mBinding(binding),
      mTextureIndex(imageIndex)
{
}

FramebufferAttachment::Target::Target(const Target &other)
    : mBinding(other.mBinding),
      mTextureIndex(other.mTextureIndex)
{
}

FramebufferAttachment::Target &FramebufferAttachment::Target::operator=(const Target &other)
{
    this->mBinding = other.mBinding;
    this->mTextureIndex = other.mTextureIndex;
    return *this;
}

////// FramebufferAttachment Implementation //////

FramebufferAttachment::FramebufferAttachment()
    : mType(GL_NONE),
      mResource(nullptr),
      mNumViews(kDefaultNumViews),
      mMultiviewLayout(kDefaultMultiviewLayout),
      mBaseViewIndex(kDefaultBaseViewIndex),
      mViewportOffsets(GetDefaultViewportOffsetVector())
{
}

FramebufferAttachment::FramebufferAttachment(const Context *context,
                                             GLenum type,
                                             GLenum binding,
                                             const ImageIndex &textureIndex,
                                             FramebufferAttachmentObject *resource)
    : mResource(nullptr)
{
    attach(context, type, binding, textureIndex, resource, kDefaultNumViews, kDefaultBaseViewIndex,
           kDefaultMultiviewLayout, kDefaultViewportOffsets);
}

FramebufferAttachment::FramebufferAttachment(FramebufferAttachment &&other)
    : FramebufferAttachment()
{
    *this = std::move(other);
}

FramebufferAttachment &FramebufferAttachment::operator=(FramebufferAttachment &&other)
{
    std::swap(mType, other.mType);
    std::swap(mTarget, other.mTarget);
    std::swap(mResource, other.mResource);
    std::swap(mNumViews, other.mNumViews);
    std::swap(mMultiviewLayout, other.mMultiviewLayout);
    std::swap(mBaseViewIndex, other.mBaseViewIndex);
    std::swap(mViewportOffsets, other.mViewportOffsets);
    return *this;
}

FramebufferAttachment::~FramebufferAttachment()
{
    ASSERT(!isAttached());
}

void FramebufferAttachment::detach(const Context *context)
{
    mType = GL_NONE;
    if (mResource != nullptr)
    {
        mResource->onDetach(context);
        mResource = nullptr;
    }
    mNumViews        = kDefaultNumViews;
    mMultiviewLayout = kDefaultMultiviewLayout;
    mBaseViewIndex   = kDefaultBaseViewIndex;
    mViewportOffsets = GetDefaultViewportOffsetVector();

    // not technically necessary, could omit for performance
    mTarget = Target();
}

void FramebufferAttachment::attach(const Context *context,
                                   GLenum type,
                                   GLenum binding,
                                   const ImageIndex &textureIndex,
                                   FramebufferAttachmentObject *resource,
                                   GLsizei numViews,
                                   GLuint baseViewIndex,
                                   GLenum multiviewLayout,
                                   const GLint *viewportOffsets)
{
    if (resource == nullptr)
    {
        detach(context);
        return;
    }

    mType = type;
    mTarget = Target(binding, textureIndex);
    mNumViews        = numViews;
    mBaseViewIndex   = baseViewIndex;
    mMultiviewLayout = multiviewLayout;
    if (multiviewLayout == GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE)
    {
        mViewportOffsets = TransformViewportOffsetArrayToVectorOfOffsets(viewportOffsets, numViews);
    }
    else
    {
        mViewportOffsets = GetDefaultViewportOffsetVector();
    }
    resource->onAttach(context);

    if (mResource != nullptr)
    {
        mResource->onDetach(context);
    }

    mResource = resource;
}

GLuint FramebufferAttachment::getRedSize() const
{
    return getFormat().info->redBits;
}

GLuint FramebufferAttachment::getGreenSize() const
{
    return getFormat().info->greenBits;
}

GLuint FramebufferAttachment::getBlueSize() const
{
    return getFormat().info->blueBits;
}

GLuint FramebufferAttachment::getAlphaSize() const
{
    return getFormat().info->alphaBits;
}

GLuint FramebufferAttachment::getDepthSize() const
{
    return getFormat().info->depthBits;
}

GLuint FramebufferAttachment::getStencilSize() const
{
    return getFormat().info->stencilBits;
}

GLenum FramebufferAttachment::getComponentType() const
{
    return getFormat().info->componentType;
}

GLenum FramebufferAttachment::getColorEncoding() const
{
    return getFormat().info->colorEncoding;
}

GLuint FramebufferAttachment::id() const
{
    return mResource->getId();
}

const ImageIndex &FramebufferAttachment::getTextureImageIndex() const
{
    ASSERT(type() == GL_TEXTURE);
    return mTarget.textureIndex();
}

GLenum FramebufferAttachment::cubeMapFace() const
{
    ASSERT(mType == GL_TEXTURE);

    const auto &index = mTarget.textureIndex();
    return IsCubeMapTextureTarget(index.type) ? index.type : GL_NONE;
}

GLint FramebufferAttachment::mipLevel() const
{
    ASSERT(type() == GL_TEXTURE);
    return mTarget.textureIndex().mipIndex;
}

GLint FramebufferAttachment::layer() const
{
    ASSERT(mType == GL_TEXTURE);

    const auto &index = mTarget.textureIndex();

    if (index.type == GL_TEXTURE_2D_ARRAY || index.type == GL_TEXTURE_3D)
    {
        return index.layerIndex;
    }
    return 0;
}

GLsizei FramebufferAttachment::getNumViews() const
{
    return mNumViews;
}

GLenum FramebufferAttachment::getMultiviewLayout() const
{
    return mMultiviewLayout;
}

GLint FramebufferAttachment::getBaseViewIndex() const
{
    return mBaseViewIndex;
}

const std::vector<Offset> &FramebufferAttachment::getMultiviewViewportOffsets() const
{
    return mViewportOffsets;
}

Texture *FramebufferAttachment::getTexture() const
{
    return rx::GetAs<Texture>(mResource);
}

Renderbuffer *FramebufferAttachment::getRenderbuffer() const
{
    return rx::GetAs<Renderbuffer>(mResource);
}

const egl::Surface *FramebufferAttachment::getSurface() const
{
    return rx::GetAs<egl::Surface>(mResource);
}

FramebufferAttachmentObject *FramebufferAttachment::getResource() const
{
    return mResource;
}

bool FramebufferAttachment::operator==(const FramebufferAttachment &other) const
{
    if (mResource != other.mResource || mType != other.mType || mNumViews != other.mNumViews ||
        mMultiviewLayout != other.mMultiviewLayout || mBaseViewIndex != other.mBaseViewIndex ||
        mViewportOffsets != other.mViewportOffsets)
    {
        return false;
    }

    if (mType == GL_TEXTURE && getTextureImageIndex() != other.getTextureImageIndex())
    {
        return false;
    }

    return true;
}

bool FramebufferAttachment::operator!=(const FramebufferAttachment &other) const
{
    return !(*this == other);
}

InitState FramebufferAttachment::initState() const
{
    return mResource ? mResource->initState(mTarget.textureIndex()) : InitState::Initialized;
}

Error FramebufferAttachment::initializeContents(const Context *context)
{
    ASSERT(mResource);
    ANGLE_TRY(mResource->initializeContents(context, mTarget.textureIndex()));
    setInitState(InitState::Initialized);
    return NoError();
}

void FramebufferAttachment::setInitState(InitState initState) const
{
    ASSERT(mResource);
    mResource->setInitState(mTarget.textureIndex(), initState);
}

////// FramebufferAttachmentObject Implementation //////

FramebufferAttachmentObject::FramebufferAttachmentObject()
{
}

FramebufferAttachmentObject::~FramebufferAttachmentObject()
{
}

Error FramebufferAttachmentObject::getAttachmentRenderTarget(
    const Context *context,
    GLenum binding,
    const ImageIndex &imageIndex,
    rx::FramebufferAttachmentRenderTarget **rtOut) const
{
    return getAttachmentImpl()->getAttachmentRenderTarget(context, binding, imageIndex, rtOut);
}

OnAttachmentDirtyChannel *FramebufferAttachmentObject::getDirtyChannel()
{
    return &mDirtyChannel;
}

Error FramebufferAttachmentObject::initializeContents(const Context *context,
                                                      const ImageIndex &imageIndex)
{
    ASSERT(context->isRobustResourceInitEnabled());

    // Because gl::Texture cannot support tracking individual layer dirtiness, we only handle
    // initializing entire mip levels for 2D array textures.
    if (imageIndex.type == GL_TEXTURE_2D_ARRAY && imageIndex.hasLayer())
    {
        ImageIndex fullMipIndex = imageIndex;
        fullMipIndex.layerIndex = ImageIndex::ENTIRE_LEVEL;
        return getAttachmentImpl()->initializeContents(context, fullMipIndex);
    }
    else
    {
        return getAttachmentImpl()->initializeContents(context, imageIndex);
    }
}

}  // namespace gl
