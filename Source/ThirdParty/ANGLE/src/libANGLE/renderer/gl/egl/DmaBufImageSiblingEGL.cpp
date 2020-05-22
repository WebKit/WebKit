//
// Copyright The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DmaBufImageSiblingEGL.cpp: Defines the DmaBufImageSiblingEGL to wrap EGL images
// created from dma_buf objects

#include "libANGLE/renderer/gl/egl/DmaBufImageSiblingEGL.h"

namespace rx
{
DmaBufImageSiblingEGL::DmaBufImageSiblingEGL(const egl::AttributeMap &attribs)
    : mAttribs(attribs), mFormat(GL_NONE)
{
    ASSERT(mAttribs.contains(EGL_WIDTH));
    mSize.width = mAttribs.getAsInt(EGL_WIDTH);
    ASSERT(mAttribs.contains(EGL_HEIGHT));
    mSize.height = mAttribs.getAsInt(EGL_HEIGHT);
    mSize.depth  = 1;

    // TODO(geofflang):  Implement a conversion table from DRM_FORMAT_* to GL format to expose a
    // valid mFormat to know if this image can be rendered to. For now it's fine to always say we're
    // RGBA8. http://anglebug.com/4529
    mFormat = gl::Format(GL_RGBA8);
}

DmaBufImageSiblingEGL::~DmaBufImageSiblingEGL() {}

egl::Error DmaBufImageSiblingEGL::initialize(const egl::Display *display)
{
    return egl::NoError();
}

gl::Format DmaBufImageSiblingEGL::getFormat() const
{
    return mFormat;
}

bool DmaBufImageSiblingEGL::isRenderable(const gl::Context *context) const
{
    return true;
}

bool DmaBufImageSiblingEGL::isTexturable(const gl::Context *context) const
{
    return true;
}

gl::Extents DmaBufImageSiblingEGL::getSize() const
{
    return mSize;
}

size_t DmaBufImageSiblingEGL::getSamples() const
{
    return 0;
}

EGLClientBuffer DmaBufImageSiblingEGL::getBuffer() const
{
    return nullptr;
}

void DmaBufImageSiblingEGL::getImageCreationAttributes(std::vector<EGLint> *outAttributes) const
{
    EGLenum kForwardedAttribs[] = {EGL_WIDTH,
                                   EGL_HEIGHT,
                                   EGL_LINUX_DRM_FOURCC_EXT,
                                   EGL_DMA_BUF_PLANE0_FD_EXT,
                                   EGL_DMA_BUF_PLANE0_OFFSET_EXT,
                                   EGL_DMA_BUF_PLANE0_PITCH_EXT,
                                   EGL_DMA_BUF_PLANE1_FD_EXT,
                                   EGL_DMA_BUF_PLANE1_OFFSET_EXT,
                                   EGL_DMA_BUF_PLANE1_PITCH_EXT,
                                   EGL_DMA_BUF_PLANE2_FD_EXT,
                                   EGL_DMA_BUF_PLANE2_OFFSET_EXT,
                                   EGL_DMA_BUF_PLANE2_PITCH_EXT,
                                   EGL_YUV_COLOR_SPACE_HINT_EXT,
                                   EGL_SAMPLE_RANGE_HINT_EXT,
                                   EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT,
                                   EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT,

                                   EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
                                   EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
                                   EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
                                   EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
                                   EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
                                   EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
                                   EGL_DMA_BUF_PLANE3_FD_EXT,
                                   EGL_DMA_BUF_PLANE3_OFFSET_EXT,
                                   EGL_DMA_BUF_PLANE3_PITCH_EXT,
                                   EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
                                   EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT};

    for (EGLenum forwardedAttrib : kForwardedAttribs)
    {
        if (mAttribs.contains(forwardedAttrib))
        {
            outAttributes->push_back(forwardedAttrib);
            outAttributes->push_back(mAttribs.getAsInt(forwardedAttrib));
        }
    }
}

}  // namespace rx
