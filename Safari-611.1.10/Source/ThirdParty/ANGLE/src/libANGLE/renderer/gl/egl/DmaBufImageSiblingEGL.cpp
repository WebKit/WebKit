//
// Copyright The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DmaBufImageSiblingEGL.cpp: Defines the DmaBufImageSiblingEGL to wrap EGL images
// created from dma_buf objects

#include "libANGLE/renderer/gl/egl/DmaBufImageSiblingEGL.h"

namespace
{

// Refer to the following link
// https://source.chromium.org/chromium/chromium/src/+/master:ui/gl/gl_image_native_pixmap.cc;l=24
#define FOURCC(a, b, c, d)                                          \
    ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
     (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_R8 FOURCC('R', '8', ' ', ' ')
#define DRM_FORMAT_R16 FOURCC('R', '1', '6', ' ')
#define DRM_FORMAT_GR88 FOURCC('G', 'R', '8', '8')
#define DRM_FORMAT_RGB565 FOURCC('R', 'G', '1', '6')
#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')
#define DRM_FORMAT_ABGR2101010 FOURCC('A', 'B', '3', '0')
#define DRM_FORMAT_ARGB2101010 FOURCC('A', 'R', '3', '0')
#define DRM_FORMAT_YVU420 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')
#define DRM_FORMAT_P010 FOURCC('P', '0', '1', '0')

GLenum FourCCFormatToGLInternalFormat(int format)
{
    switch (format)
    {
        case DRM_FORMAT_R8:
            return GL_R8;
        case DRM_FORMAT_R16:
            return GL_R16_EXT;
        case DRM_FORMAT_GR88:
            return GL_RG8_EXT;
        case DRM_FORMAT_ABGR8888:
            return GL_RGBA8;
        case DRM_FORMAT_XBGR8888:
            return GL_RGB8;
        case DRM_FORMAT_ARGB8888:
            return GL_BGRA8_EXT;
        case DRM_FORMAT_XRGB8888:
            return GL_RGB8;
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_ARGB2101010:
            return GL_RGB10_A2_EXT;
        case DRM_FORMAT_RGB565:
            return GL_RGB565;
        case DRM_FORMAT_NV12:
        case DRM_FORMAT_YVU420:
        case DRM_FORMAT_P010:
            return GL_RGB8;
        default:
            NOTREACHED();
            WARN() << "Unknown dma_buf format " << format << " used to initialize an EGL image.";
            return GL_RGB8;
    }
}

}  // namespace

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

    int fourCCFormat = mAttribs.getAsInt(EGL_LINUX_DRM_FOURCC_EXT);
    mFormat          = gl::Format(FourCCFormatToGLInternalFormat(fourCCFormat));
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
