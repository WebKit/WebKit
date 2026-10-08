//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// IOSurfaceSurfaceMtl.mm:
//    Implements the class methods for IOSurfaceSurfaceMtl.
//

#include "libANGLE/renderer/metal/IOSurfaceSurfaceMtl.h"

#include <TargetConditionals.h>

#include "libANGLE/Display.h"
#include "libANGLE/Surface.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/FrameBufferMtl.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_utils.h"

// Compiler can turn on programmatical frame capture in release build by defining
// ANGLE_METAL_FRAME_CAPTURE flag.
#if defined(NDEBUG) && !defined(ANGLE_METAL_FRAME_CAPTURE)
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED 0
#else
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED 1
#endif
namespace rx
{

namespace
{

struct IOSurfaceFormatInfo
{
    GLenum internalFormat;
    GLenum type;
    size_t componentBytes;

    angle::FormatID nativeAngleFormatId;
};

// clang-format off
// GL_RGB is a special case. The native angle::FormatID would be either R8G8B8X8_UNORM
// or B8G8R8X8_UNORM based on the IOSurface's pixel format.
constexpr std::array<IOSurfaceFormatInfo, 9> kIOSurfaceFormats = {{
    {GL_RED,         GL_UNSIGNED_BYTE,                  1, angle::FormatID::R8_UNORM},
    {GL_RED,         GL_UNSIGNED_SHORT,                 2, angle::FormatID::R16_UNORM},
    {GL_RG,          GL_UNSIGNED_BYTE,                  2, angle::FormatID::R8G8_UNORM},
    {GL_RG,          GL_UNSIGNED_SHORT,                 4, angle::FormatID::R16G16_UNORM},
    {GL_RGB,         GL_UNSIGNED_BYTE,                  4, angle::FormatID::NONE},
    {GL_RGBA,        GL_UNSIGNED_BYTE,                  4, angle::FormatID::R8G8B8A8_UNORM},
    {GL_BGRA_EXT,    GL_UNSIGNED_BYTE,                  4, angle::FormatID::B8G8R8A8_UNORM},
    {GL_RGBA,        GL_HALF_FLOAT,                     8, angle::FormatID::R16G16B16A16_FLOAT},
    {GL_RGB10_A2,    GL_UNSIGNED_INT_2_10_10_10_REV,    4, angle::FormatID::B10G10R10A2_UNORM},
}};
// clang-format on

int FindIOSurfaceFormatIndex(GLenum internalFormat, GLenum type)
{
    for (int i = 0; i < static_cast<int>(kIOSurfaceFormats.size()); ++i)
    {
        const auto &formatInfo = kIOSurfaceFormats[i];
        if (formatInfo.internalFormat == internalFormat && formatInfo.type == type)
        {
            return i;
        }
    }
    return -1;
}

// Builds a row-major YCbCr->RGB matrix such that, for normalized samples,
// rgb[i] = dot(matrix.row(i), float4(y, cb, cr, 1)).  Derived from the Kb/Kr coefficients of ITU-R
// BT.601, BT.709, BT.2020 and SMPTE 240M, including video/full-range scaling.
std::array<float, 12> ComputeYCbCrToRGBMatrix(EGLint colorSpaceHint, EGLint rangeHint)
{
    float kb, kr;
    switch (colorSpaceHint)
    {
        case EGL_ITU_REC709_EXT:
            kb = 0.0722f;
            kr = 0.2126f;
            break;
        case EGL_ITU_REC2020_EXT:
            kb = 0.0593f;
            kr = 0.2627f;
            break;
        case EGL_SMPTE_240M_ANGLE:
            kb = 0.087f;
            kr = 0.212f;
            break;
        case EGL_ITU_REC601_EXT:
        default:
            kb = 0.114f;
            kr = 0.299f;
            break;
    }
    const bool full    = rangeHint == EGL_YUV_FULL_RANGE_EXT;
    const float kg     = 1.0f - kb - kr;
    const float yScale = full ? 1.0f : 255.0f / 219.0f;
    const float cScale = full ? 1.0f : 255.0f / 224.0f;

    std::array<std::array<float, 4>, 3> rows = {{
        {yScale, 0.0f, cScale * 2.0f * (1.0f - kr), 0.0f},
        {yScale, -cScale * 2.0f * (1.0f - kb) * (kb / kg), -cScale * 2.0f * (1.0f - kr) * (kr / kg),
         0.0f},
        {yScale, cScale * 2.0f * (1.0f - kb), 0.0f, 0.0f},
    }};

    std::array<float, 12> m{};
    for (int i = 0; i < 3; ++i)
    {
        rows[i][3] -= (rows[i][1] + rows[i][2]) * 128.0f / 255.0f;
        if (!full)
        {
            rows[i][3] -= rows[i][0] * 16.0f / 255.0f;
        }
        for (int j = 0; j < 4; ++j)
        {
            m[i * 4 + j] = rows[i][j];
        }
    }
    return m;
}

}  // anonymous namespace

// IOSurfaceSurfaceMtl implementation.
IOSurfaceSurfaceMtl::IOSurfaceSurfaceMtl(DisplayMtl *display,
                                         const egl::SurfaceState &state,
                                         EGLClientBuffer buffer,
                                         const egl::AttributeMap &attribs)
    : OffscreenSurfaceMtl(display, state, attribs), mIOSurface((__bridge IOSurfaceRef)(buffer))
{
    CFRetain(mIOSurface);

    mIOSurfacePlane = static_cast<int>(attribs.get(EGL_IOSURFACE_PLANE_ANGLE));

    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);

    if (internalFormat == GL_G8_B8R8_2PLANE_420_UNORM_ANGLE)
    {
        // Multiplanar NV12: luma plane sampled as R8, chroma plane as RG8, converted to RGB in
        // the copy-texture draw using the colorspace/range hints.
        mIsYUV              = true;
        mIOSurfaceFormatIdx = -1;
        mColorFormat        = display->getPixelFormat(angle::FormatID::R8_UNORM);
        mChromaFormat       = display->getPixelFormat(angle::FormatID::R8G8_UNORM);
        mYUVOrientation     = attribs.getAsInt(EGL_IOSURFACE_ORIENTATION_ANGLE, 0);
        mYUVToRGBMatrix     = ComputeYCbCrToRGBMatrix(
            attribs.getAsInt(EGL_YUV_COLOR_SPACE_HINT_EXT, EGL_ITU_REC601_EXT),
            attribs.getAsInt(EGL_SAMPLE_RANGE_HINT_EXT, EGL_YUV_NARROW_RANGE_EXT));
        return;
    }

    mIOSurfaceFormatIdx =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));
    ASSERT(mIOSurfaceFormatIdx >= 0);

    angle::FormatID actualAngleFormatId =
        kIOSurfaceFormats[mIOSurfaceFormatIdx].nativeAngleFormatId;
    if (actualAngleFormatId == angle::FormatID::NONE)
    {
        // The actual angle::Format depends on the IOSurface's format.
        ASSERT(internalFormat == GL_RGB);
        switch (IOSurfaceGetPixelFormat(mIOSurface))
        {
            case 'BGRA':
                actualAngleFormatId = angle::FormatID::B8G8R8X8_UNORM;
                break;
            case 'RGBA':
                actualAngleFormatId = angle::FormatID::R8G8B8X8_UNORM;
                break;
            default:
                UNREACHABLE();
        }
    }

    mColorFormat = display->getPixelFormat(actualAngleFormatId);
}
IOSurfaceSurfaceMtl::~IOSurfaceSurfaceMtl()
{
    if (mIOSurface != nullptr)
    {
        CFRelease(mIOSurface);
        mIOSurface = nullptr;
    }
}

egl::Error IOSurfaceSurfaceMtl::bindTexImage(const gl::Context *context,
                                             gl::Texture *texture,
                                             EGLint buffer)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);
    StartFrameCapture(contextMtl);

    // Initialize offscreen texture if needed:
    ANGLE_TO_EGL_TRY(ensureColorTextureCreated(context));

    if (mColorTexture)
    {
        // Mark the resource as written by command buffer so that we wait for command buffer finish
        // before doing a readback on the CPU via getBytes. It's necessary to synchronize with the
        // shared event wait in the command buffer if the previous user of the IOSurface did not
        // call waitUntilScheduled e.g. Chromium will skip waitUntilScheduled on single GPU systems.
        contextMtl->markResourceWrittenByCommandBuffer(mColorTexture);
    }

    return OffscreenSurfaceMtl::bindTexImage(context, texture, buffer);
}

egl::Error IOSurfaceSurfaceMtl::releaseTexImage(const gl::Context *context, EGLint buffer)
{
    egl::Error re = OffscreenSurfaceMtl::releaseTexImage(context, buffer);
    StopFrameCapture();
    return re;
}

angle::Result IOSurfaceSurfaceMtl::getAttachmentRenderTarget(
    const gl::Context *context,
    GLenum binding,
    const gl::ImageIndex &imageIndex,
    GLsizei samples,
    FramebufferAttachmentRenderTarget **rtOut)
{
    // Initialize offscreen texture if needed:
    ANGLE_TRY(ensureColorTextureCreated(context));

    return OffscreenSurfaceMtl::getAttachmentRenderTarget(context, binding, imageIndex, samples,
                                                          rtOut);
}

angle::Result IOSurfaceSurfaceMtl::ensureTexturesSizeCorrect(const gl::Context *context)
{
    // The YUV plane textures have the size of the IOSurface planes, which differs from the pbuffer
    // size if the orientation swaps the axes.  They are never resized.
    if (mIsYUV)
    {
        return ensureColorTextureCreated(context);
    }
    return OffscreenSurfaceMtl::ensureTexturesSizeCorrect(context);
}

angle::Result IOSurfaceSurfaceMtl::ensureColorTextureCreated(const gl::Context *context)
{
    if (mColorTexture)
    {
        return angle::Result::Continue;
    }
    ContextMtl *contextMtl = mtl::GetImpl(context);

    ANGLE_MTL_OBJC_SCOPE
    {
        if (mIsYUV)
        {
            // Luma plane (plane 0), full plane size, sampled as R8.  The pbuffer size is the
            // oriented plane size, see ValidateAttributes.
            auto lumaDesc  = [MTLTextureDescriptor
                texture2DDescriptorWithPixelFormat:mColorFormat.metalFormat
                                             width:IOSurfaceGetWidthOfPlane(mIOSurface, 0)
                                            height:IOSurfaceGetHeightOfPlane(mIOSurface, 0)
                                         mipmapped:NO];
            lumaDesc.usage = MTLTextureUsageShaderRead;
            mColorTexture  = mtl::Texture::MakeFromMetal(
                contextMtl->getMetalDevice().newTextureWithDescriptor(lumaDesc, mIOSurface, 0));

            // Chroma plane (plane 1), half resolution, sampled as RG8.
            auto chromaDesc  = [MTLTextureDescriptor
                texture2DDescriptorWithPixelFormat:mChromaFormat.metalFormat
                                             width:IOSurfaceGetWidthOfPlane(mIOSurface, 1)
                                            height:IOSurfaceGetHeightOfPlane(mIOSurface, 1)
                                         mipmapped:NO];
            chromaDesc.usage = MTLTextureUsageShaderRead;
            mChromaTexture   = mtl::Texture::MakeFromMetal(
                contextMtl->getMetalDevice().newTextureWithDescriptor(chromaDesc, mIOSurface, 1));

            if (!mColorTexture->valid() || !mChromaTexture->valid())
            {
                mColorTexture  = nullptr;
                mChromaTexture = nullptr;
                ANGLE_CHECK_GL_ALLOC(contextMtl, false);
            }

            mColorRenderTarget.set(mColorTexture, mtl::kZeroNativeMipLevel, 0, mColorFormat);
            mColorTextureInitialized = false;
            return angle::Result::Continue;
        }

        auto texDesc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mColorFormat.metalFormat
                                                               width:mSize.width
                                                              height:mSize.height
                                                           mipmapped:NO];

        texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;

        mColorTexture =
            mtl::Texture::MakeFromMetal(contextMtl->getMetalDevice().newTextureWithDescriptor(
                texDesc, mIOSurface, mIOSurfacePlane));

        if (mColorTexture)
        {
            size_t resourceSize = EstimateTextureSizeInBytes(
                mColorFormat, mColorTexture->widthAt0(), mColorTexture->heightAt0(),
                mColorTexture->depthAt0(), mColorTexture->samples(), mColorTexture->mipmapLevels());

            mColorTexture->setEstimatedByteSize(resourceSize);
        }

        mColorRenderTarget.set(mColorTexture, mtl::kZeroNativeMipLevel, 0, mColorFormat);

        if (kIOSurfaceFormats[mIOSurfaceFormatIdx].internalFormat == GL_RGB)
        {
            // This format has emulated alpha channel. Initialize texture's alpha channel to 1.0.
            const mtl::Format &rgbClearFormat =
                contextMtl->getPixelFormat(angle::FormatID::R8G8B8_UNORM);
            ANGLE_TRY(mtl::InitializeTextureContentsGPU(
                context, mColorTexture, rgbClearFormat,
                mtl::ImageNativeIndex::FromBaseZeroGLIndex(gl::ImageIndex::Make2D(0)),
                MTLColorWriteMaskAlpha));

            // Disable subsequent rendering to alpha channel.
            mColorTexture->setColorWritableMask(MTLColorWriteMaskAll & (~MTLColorWriteMaskAlpha));
        }
        // Robust resource init: currently we do not allow passing contents with IOSurfaces.
        mColorTextureInitialized = false;

        return angle::Result::Continue;
    }
}

// static
bool IOSurfaceSurfaceMtl::ValidateAttributes(EGLClientBuffer buffer,
                                             const egl::AttributeMap &attribs)
{
    IOSurfaceRef ioSurface = (__bridge IOSurfaceRef)(buffer);

    const EGLAttrib orientation = attribs.get(EGL_IOSURFACE_ORIENTATION_ANGLE, 0);

    // Multiplanar NV12 source for GL_CHROMIUM_copy_texture: requires an 8-bit luma plane and an
    // 8-bit CbCr plane of half the size, rounded up.  The pbuffer covers the whole surface, with
    // width and height swapped if the orientation swaps the axes.
    if (attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE) == GL_G8_B8R8_2PLANE_420_UNORM_ANGLE)
    {
        if (attribs.get(EGL_TEXTURE_TYPE_ANGLE) != GL_UNSIGNED_BYTE)
        {
            return false;
        }
        if (IOSurfaceGetPlaneCount(ioSurface) < 2)
        {
            return false;
        }
        size_t width  = IOSurfaceGetWidthOfPlane(ioSurface, 0);
        size_t height = IOSurfaceGetHeightOfPlane(ioSurface, 0);
        if (width == 0 || height == 0 ||
            IOSurfaceGetWidthOfPlane(ioSurface, 1) != (width + 1) / 2 ||
            IOSurfaceGetHeightOfPlane(ioSurface, 1) != (height + 1) / 2)
        {
            return false;
        }
        // Compressed layouts have elements of multiple samples, e.g. 32x32 samples of 1024 bytes.
        auto bytesPerSample = [ioSurface](size_t plane) -> size_t {
            const size_t samplesPerElement = IOSurfaceGetElementWidthOfPlane(ioSurface, plane) *
                                             IOSurfaceGetElementHeightOfPlane(ioSurface, plane);
            const size_t bytesPerElement = IOSurfaceGetBytesPerElementOfPlane(ioSurface, plane);
            if (samplesPerElement == 0 || bytesPerElement % samplesPerElement != 0)
            {
                return 0;
            }
            return bytesPerElement / samplesPerElement;
        };
        if (bytesPerSample(0) != 1 || bytesPerSample(1) != 2)
        {
            return false;
        }
        const bool swapXY = (orientation & EGL_IOSURFACE_ORIENTATION_SWAP_XY_ANGLE) != 0;
        if (swapXY)
        {
            std::swap(width, height);
        }
        return attribs.get(EGL_WIDTH) == static_cast<EGLAttrib>(width) &&
               attribs.get(EGL_HEIGHT) == static_cast<EGLAttrib>(height);
    }

    // Orientation is supported only for YUV sources.
    if (orientation != 0)
    {
        return false;
    }

    // The plane must exist for this IOSurface. IOSurfaceGetPlaneCount can return 0 for non-planar
    // ioSurfaces but we will treat non-planar like it is a single plane.
    size_t surfacePlaneCount = std::max(size_t(1), IOSurfaceGetPlaneCount(ioSurface));
    EGLAttrib plane          = attribs.get(EGL_IOSURFACE_PLANE_ANGLE);
    if (plane < 0 || static_cast<size_t>(plane) >= surfacePlaneCount)
    {
        return false;
    }

    // The width height specified must be at least (1, 1) and at most the plane size
    EGLAttrib width  = attribs.get(EGL_WIDTH);
    EGLAttrib height = attribs.get(EGL_HEIGHT);
    if (width <= 0 || static_cast<size_t>(width) > IOSurfaceGetWidthOfPlane(ioSurface, plane) ||
        height <= 0 || static_cast<size_t>(height) > IOSurfaceGetHeightOfPlane(ioSurface, plane))
    {
        return false;
    }

    // Find this IOSurface format
    EGLAttrib internalFormat = attribs.get(EGL_TEXTURE_INTERNAL_FORMAT_ANGLE);
    EGLAttrib type           = attribs.get(EGL_TEXTURE_TYPE_ANGLE);

    int formatIndex =
        FindIOSurfaceFormatIndex(static_cast<GLenum>(internalFormat), static_cast<GLenum>(type));

    if (formatIndex < 0)
    {
        return false;
    }

    // FIXME: Check that the format matches this IOSurface plane for pixel formats that we know of.
    // We could map IOSurfaceGetPixelFormat to expected type plane and format type.
    // However, the caller might supply us non-public pixel format, which makes exhaustive checks
    // problematic.
    if (IOSurfaceGetBytesPerElementOfPlane(ioSurface, plane) !=
        kIOSurfaceFormats[formatIndex].componentBytes)
    {
        WARN() << "IOSurface bytes per elements does not match the pbuffer internal format.";
    }

    return true;
}
}  // namespace rx
