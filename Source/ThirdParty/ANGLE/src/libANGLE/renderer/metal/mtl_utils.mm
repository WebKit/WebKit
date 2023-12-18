//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_utils.mm:
//    Implements utilities functions that create Metal shaders, convert from angle enums
//    to Metal enums and so on.
//

#include "libANGLE/renderer/metal/mtl_utils.h"

#include <Availability.h>
#include <TargetConditionals.h>

#include "common/MemoryBuffer.h"
#include "common/string_utils.h"
#include "common/system_utils.h"
#include "gpu_info_util/SystemInfo_internal.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/RenderTargetMtl.h"
#include "libANGLE/renderer/metal/mtl_render_utils.h"
#include "libANGLE/renderer/metal/process.h"
#include "platform/PlatformMethods.h"

// Compiler can turn on programmatical frame capture in release build by defining
// ANGLE_METAL_FRAME_CAPTURE flag.
#if defined(NDEBUG) && !defined(ANGLE_METAL_FRAME_CAPTURE)
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED 0
#else
#    define ANGLE_METAL_FRAME_CAPTURE_ENABLED ANGLE_WITH_MODERN_METAL_API
#endif

namespace rx
{

ANGLE_APPLE_UNUSED
bool IsFrameCaptureEnabled()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return false;
#else
    // We only support frame capture programmatically if the ANGLE_METAL_FRAME_CAPTURE
    // environment flag is set. Otherwise, it will slow down the rendering. This allows user to
    // finely control whether they want to capture the frame for particular application or not.
    auto var                  = std::getenv("ANGLE_METAL_FRAME_CAPTURE");
    static const bool enabled = var ? (strcmp(var, "1") == 0) : false;

    return enabled;
#endif
}

ANGLE_APPLE_UNUSED
std::string GetMetalCaptureFile()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return {};
#else
    auto var                   = std::getenv("ANGLE_METAL_FRAME_CAPTURE_FILE");
    const std::string filePath = var ? var : "";

    return filePath;
#endif
}

ANGLE_APPLE_UNUSED
size_t MaxAllowedFrameCapture()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return 0;
#else
    auto var                      = std::getenv("ANGLE_METAL_FRAME_CAPTURE_MAX");
    static const size_t maxFrames = var ? std::atoi(var) : 100;

    return maxFrames;
#endif
}

ANGLE_APPLE_UNUSED
size_t MinAllowedFrameCapture()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return 0;
#else
    auto var                     = std::getenv("ANGLE_METAL_FRAME_CAPTURE_MIN");
    static const size_t minFrame = var ? std::atoi(var) : 0;

    return minFrame;
#endif
}

ANGLE_APPLE_UNUSED
bool FrameCaptureDeviceScope()
{
#if !ANGLE_METAL_FRAME_CAPTURE_ENABLED
    return false;
#else
    auto var                      = std::getenv("ANGLE_METAL_FRAME_CAPTURE_SCOPE");
    static const bool scopeDevice = var ? (strcmp(var, "device") == 0) : false;

    return scopeDevice;
#endif
}

ANGLE_APPLE_UNUSED
std::atomic<size_t> gFrameCaptured(0);

ANGLE_APPLE_UNUSED
void StartFrameCapture(id<MTLDevice> metalDevice, id<MTLCommandQueue> metalCmdQueue)
{
#if ANGLE_METAL_FRAME_CAPTURE_ENABLED
    if (!IsFrameCaptureEnabled())
    {
        return;
    }

    if (gFrameCaptured >= MaxAllowedFrameCapture())
    {
        return;
    }

    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    if (captureManager.isCapturing)
    {
        return;
    }

    gFrameCaptured++;

    if (gFrameCaptured < MinAllowedFrameCapture())
    {
        return;
    }

#    ifdef __MAC_10_15
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13))
    {
        auto captureDescriptor = mtl::adoptObjCObj([[MTLCaptureDescriptor alloc] init]);
        captureDescriptor.get().captureObject = metalDevice;
        const std::string filePath            = GetMetalCaptureFile();
        if (filePath != "")
        {
            const std::string numberedPath =
                filePath + std::to_string(gFrameCaptured - 1) + ".gputrace";
            captureDescriptor.get().destination = MTLCaptureDestinationGPUTraceDocument;
            captureDescriptor.get().outputURL =
                [NSURL fileURLWithPath:[NSString stringWithUTF8String:numberedPath.c_str()]
                           isDirectory:false];
        }
        else
        {
            // This will pause execution only if application is being debugged inside Xcode
            captureDescriptor.get().destination = MTLCaptureDestinationDeveloperTools;
        }

        NSError *error;
        if (![captureManager startCaptureWithDescriptor:captureDescriptor.get() error:&error])
        {
            NSLog(@"Failed to start capture, error %@", error);
        }
    }
    else
#    endif  // __MAC_10_15
        if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13))
        {
            auto captureDescriptor = mtl::adoptObjCObj([[MTLCaptureDescriptor alloc] init]);
            captureDescriptor.get().captureObject = metalDevice;

            NSError *error;
            if (![captureManager startCaptureWithDescriptor:captureDescriptor.get() error:&error])
            {
                NSLog(@"Failed to start capture, error %@", error);
            }
        }
#endif  // ANGLE_METAL_FRAME_CAPTURE_ENABLED
}

void StartFrameCapture(ContextMtl *context)
{
    StartFrameCapture(context->getMetalDevice(), context->cmdQueue().get());
}

void StopFrameCapture()
{
#if ANGLE_METAL_FRAME_CAPTURE_ENABLED
    if (!IsFrameCaptureEnabled())
    {
        return;
    }
    MTLCaptureManager *captureManager = [MTLCaptureManager sharedCaptureManager];
    if (captureManager.isCapturing)
    {
        [captureManager stopCapture];
    }
#endif
}

namespace mtl
{

constexpr char kANGLEPrintMSLEnv[]        = "ANGLE_METAL_PRINT_MSL_ENABLE";
constexpr char kANGLEMSLVersionMajorEnv[] = "ANGLE_MSL_VERSION_MAJOR";
constexpr char kANGLEMSLVersionMinorEnv[] = "ANGLE_MSL_VERSION_MINOR";

namespace
{

uint32_t GetDeviceVendorIdFromName(id<MTLDevice> metalDevice)
{
    struct Vendor
    {
        NSString *const trademark;
        uint32_t vendorId;
    };

    constexpr Vendor kVendors[] = {
        {@"AMD", angle::kVendorID_AMD},        {@"Apple", angle::kVendorID_Apple},
        {@"Radeon", angle::kVendorID_AMD},     {@"Intel", angle::kVendorID_Intel},
        {@"Geforce", angle::kVendorID_NVIDIA}, {@"Quadro", angle::kVendorID_NVIDIA}};
    ANGLE_MTL_OBJC_SCOPE
    {
        if (metalDevice)
        {
            for (const Vendor &it : kVendors)
            {
                if ([metalDevice.name rangeOfString:it.trademark].location != NSNotFound)
                {
                    return it.vendorId;
                }
            }
        }

        return 0;
    }
}

#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
uint32_t GetDeviceVendorIdFromIOKit(id<MTLDevice> device)
{
    return angle::GetVendorIDFromMetalDeviceRegistryID(device.registryID);
}
#endif

void GetSliceAndDepth(const ImageNativeIndex &index, GLint *layer, GLint *startDepth)
{
    *layer = *startDepth = 0;
    if (!index.hasLayer())
    {
        return;
    }

    switch (index.getType())
    {
        case gl::TextureType::CubeMap:
            *layer = index.cubeMapFaceIndex();
            break;
        case gl::TextureType::_2DArray:
            *layer = index.getLayerIndex();
            break;
        case gl::TextureType::_3D:
            *startDepth = index.getLayerIndex();
            break;
        default:
            break;
    }
}
GLint GetSliceOrDepth(const ImageNativeIndex &index)
{
    GLint layer, startDepth;
    GetSliceAndDepth(index, &layer, &startDepth);

    return std::max(layer, startDepth);
}

bool GetCompressedBufferSizeAndRowLengthForTextureWithFormat(const TextureRef &texture,
                                                             const Format &textureObjFormat,
                                                             const ImageNativeIndex &index,
                                                             size_t *bytesPerRowOut,
                                                             size_t *bytesPerImageOut)
{
    gl::Extents size = texture->size(index);
    ASSERT(size.depth == 1);
    GLuint bufferRowInBytes;
    if (!textureObjFormat.intendedInternalFormat().computeCompressedImageRowPitch(
            size.width, &bufferRowInBytes))
    {
        return false;
    }
    GLuint bufferSizeInBytes;
    if (!textureObjFormat.intendedInternalFormat().computeCompressedImageDepthPitch(
            size.height, bufferRowInBytes, &bufferSizeInBytes))
    {
        return false;
    }
    *bytesPerRowOut   = bufferRowInBytes;
    *bytesPerImageOut = bufferSizeInBytes;
    return true;
}
static angle::Result InitializeCompressedTextureContents(const gl::Context *context,
                                                         const TextureRef &texture,
                                                         const Format &textureObjFormat,
                                                         const ImageNativeIndex &index,
                                                         const uint layer,
                                                         const uint startDepth)
{
    assert(textureObjFormat.actualAngleFormat().isBlock);
    size_t bytesPerRow   = 0;
    size_t bytesPerImage = 0;
    if (!GetCompressedBufferSizeAndRowLengthForTextureWithFormat(texture, textureObjFormat, index,
                                                                 &bytesPerRow, &bytesPerImage))
    {
        return angle::Result::Stop;
    }
    ContextMtl *contextMtl = mtl::GetImpl(context);
    gl::Extents extents    = texture->size(index);
    if (texture->isCPUAccessible())
    {
        if (textureObjFormat.isPVRTC())
        {
            // Replace Region Validation: rowBytes must be 0
            bytesPerRow = 0;
        }

        angle::MemoryBuffer buffer;
        if (!buffer.resize(bytesPerImage))
        {
            return angle::Result::Stop;
        }
        buffer.fill(0);
        for (NSUInteger d = 0; d < static_cast<NSUInteger>(extents.depth); ++d)
        {
            auto mtlTextureRegion     = MTLRegionMake2D(0, 0, extents.width, extents.height);
            mtlTextureRegion.origin.z = d + startDepth;
            texture->replaceRegion(contextMtl, mtlTextureRegion, index.getNativeLevel(), layer,
                                   buffer.data(), bytesPerRow, 0);
        }
    }
    else
    {
        mtl::BufferRef zeroBuffer;
        ANGLE_TRY(mtl::Buffer::MakeBuffer(contextMtl, bytesPerImage, nullptr, &zeroBuffer));
        mtl::BlitCommandEncoder *blitEncoder = contextMtl->getBlitCommandEncoder();
        for (NSUInteger d = 0; d < static_cast<NSUInteger>(extents.depth); ++d)
        {
            auto blitOrigin = MTLOriginMake(0, 0, d + startDepth);
            blitEncoder->copyBufferToTexture(zeroBuffer, 0, bytesPerRow, 0,
                                             MTLSizeMake(extents.width, extents.height, 1), texture,
                                             layer, index.getNativeLevel(), blitOrigin, 0);
        }
        blitEncoder->endEncoding();
    }
    return angle::Result::Continue;
}

}  // namespace

bool PreferStagedTextureUploads(const gl::Context *context,
                                const TextureRef &texture,
                                const Format &textureObjFormat,
                                StagingPurpose purpose)
{
    // The simulator MUST upload all textures as staged.
    if (TARGET_OS_SIMULATOR)
    {
        return true;
    }

    ContextMtl *contextMtl             = mtl::GetImpl(context);
    const angle::FeaturesMtl &features = contextMtl->getDisplay()->getFeatures();

    const gl::InternalFormat &intendedInternalFormat = textureObjFormat.intendedInternalFormat();
    if (intendedInternalFormat.compressed || textureObjFormat.actualAngleFormat().isBlock)
    {
        return false;
    }

    // If the intended internal format is luminance, we can still
    // initialize the texture using the GPU. However, if we're
    // uploading data to it, we avoid using a staging buffer, due to
    // the (current) need to re-pack the data from L8 -> RGBA8 and LA8
    // -> RGBA8. This could be better optimized by emulating L8
    // textures with R8 and LA8 with RG8, and using swizzlig for the
    // resulting textures.
    if (intendedInternalFormat.isLUMA())
    {
        return (purpose == StagingPurpose::Initialization);
    }

    if (features.disableStagedInitializationOfPackedTextureFormats.enabled)
    {
        switch (intendedInternalFormat.sizedInternalFormat)
        {
            case GL_RGB9_E5:
            case GL_R11F_G11F_B10F:
                return false;

            default:
                break;
        }
    }

    return (texture->hasIOSurface() && features.uploadDataToIosurfacesWithStagingBuffers.enabled) ||
           features.alwaysPreferStagedTextureUploads.enabled;
}

angle::Result InitializeTextureContents(const gl::Context *context,
                                        const TextureRef &texture,
                                        const Format &textureObjFormat,
                                        const ImageNativeIndex &index)
{
    ASSERT(texture && texture->valid());
    // Only one slice can be initialized at a time.
    ASSERT(!index.isLayered() || index.getType() == gl::TextureType::_3D);
    ContextMtl *contextMtl = mtl::GetImpl(context);

    const gl::InternalFormat &intendedInternalFormat = textureObjFormat.intendedInternalFormat();

    bool preferGPUInitialization = PreferStagedTextureUploads(context, texture, textureObjFormat,
                                                              StagingPurpose::Initialization);

    // This function is called in many places to initialize the content of a texture.
    // So it's better we do the initial check here instead of let the callers do it themselves:
    if (!textureObjFormat.valid())
    {
        return angle::Result::Continue;
    }

    if ((textureObjFormat.hasDepthOrStencilBits() && !textureObjFormat.getCaps().depthRenderable) ||
        !textureObjFormat.getCaps().colorRenderable)
    {
        // Texture is not appropriately color- or depth-renderable, so do not attempt
        // to use GPU initialization (clears for initialization).
        preferGPUInitialization = false;
    }

    gl::Extents size = texture->size(index);

    // Intiialize the content to black
    GLint layer, startDepth;
    GetSliceAndDepth(index, &layer, &startDepth);

    // Use compressed texture initialization only when both the intended and the actual ANGLE
    // formats are compressed. Emulated opaque ETC2 formats use uncompressed fallbacks and require
    // custom initialization.
    if (intendedInternalFormat.compressed && textureObjFormat.actualAngleFormat().isBlock)
    {
        return InitializeCompressedTextureContents(context, texture, textureObjFormat, index, layer,
                                                   startDepth);
    }
    else if (texture->isCPUAccessible() && index.getType() != gl::TextureType::_2DMultisample &&
             index.getType() != gl::TextureType::_2DMultisampleArray && !preferGPUInitialization)
    {
        const angle::Format &dstFormat = angle::Format::Get(textureObjFormat.actualFormatId);
        const size_t dstRowPitch       = dstFormat.pixelBytes * size.width;
        angle::MemoryBuffer conversionRow;
        ANGLE_CHECK_GL_ALLOC(contextMtl, conversionRow.resize(dstRowPitch));

        if (textureObjFormat.initFunction)
        {
            textureObjFormat.initFunction(size.width, 1, 1, conversionRow.data(), dstRowPitch, 0);
        }
        else
        {
            const angle::Format &srcFormat = angle::Format::Get(
                intendedInternalFormat.alphaBits > 0 ? angle::FormatID::R8G8B8A8_UNORM
                                                     : angle::FormatID::R8G8B8_UNORM);
            const size_t srcRowPitch = srcFormat.pixelBytes * size.width;
            angle::MemoryBuffer srcRow;
            ANGLE_CHECK_GL_ALLOC(contextMtl, srcRow.resize(srcRowPitch));
            memset(srcRow.data(), 0, srcRowPitch);

            CopyImageCHROMIUM(srcRow.data(), srcRowPitch, srcFormat.pixelBytes, 0,
                              srcFormat.pixelReadFunction, conversionRow.data(), dstRowPitch,
                              dstFormat.pixelBytes, 0, dstFormat.pixelWriteFunction,
                              intendedInternalFormat.format, dstFormat.componentType, size.width, 1,
                              1, false, false, false);
        }

        auto mtlRowRegion = MTLRegionMake2D(0, 0, size.width, 1);

        for (NSUInteger d = 0; d < static_cast<NSUInteger>(size.depth); ++d)
        {
            mtlRowRegion.origin.z = d + startDepth;
            for (NSUInteger r = 0; r < static_cast<NSUInteger>(size.height); ++r)
            {
                mtlRowRegion.origin.y = r;

                // Upload to texture
                texture->replace2DRegion(contextMtl, mtlRowRegion, index.getNativeLevel(), layer,
                                         conversionRow.data(), dstRowPitch);
            }
        }
    }
    else
    {
        ANGLE_TRY(InitializeTextureContentsGPU(context, texture, textureObjFormat, index,
                                               MTLColorWriteMaskAll));
    }

    return angle::Result::Continue;
}

angle::Result InitializeTextureContentsGPU(const gl::Context *context,
                                           const TextureRef &texture,
                                           const Format &textureObjFormat,
                                           const ImageNativeIndex &index,
                                           MTLColorWriteMask channelsToInit)
{
    // Only one slice can be initialized at a time.
    ASSERT(!index.isLayered() || index.getType() == gl::TextureType::_3D);
    if (index.isLayered() && index.getType() == gl::TextureType::_3D)
    {
        ImageNativeIndexIterator ite =
            index.getLayerIterator(texture->depth(index.getNativeLevel()));
        while (ite.hasNext())
        {
            ImageNativeIndex depthLayerIndex = ite.next();
            ANGLE_TRY(InitializeTextureContentsGPU(context, texture, textureObjFormat,
                                                   depthLayerIndex, MTLColorWriteMaskAll));
        }

        return angle::Result::Continue;
    }

    if (textureObjFormat.hasDepthOrStencilBits())
    {
        // Depth stencil texture needs dedicated function.
        return InitializeDepthStencilTextureContentsGPU(context, texture, textureObjFormat, index);
    }

    ContextMtl *contextMtl = mtl::GetImpl(context);
    GLint sliceOrDepth     = GetSliceOrDepth(index);

    // Use clear render command
    RenderTargetMtl tempRtt;
    tempRtt.set(texture, index.getNativeLevel(), sliceOrDepth, textureObjFormat);

    int clearAlpha = 0;
    if (!textureObjFormat.intendedAngleFormat().alphaBits)
    {
        // if intended format doesn't have alpha, set it to 1.0.
        clearAlpha = kEmulatedAlphaValue;
    }

    RenderCommandEncoder *encoder;
    if (channelsToInit == MTLColorWriteMaskAll)
    {
        // If all channels will be initialized, use clear loadOp.
        Optional<MTLClearColor> blackColor = MTLClearColorMake(0, 0, 0, clearAlpha);
        encoder = contextMtl->getRenderTargetCommandEncoderWithClear(tempRtt, blackColor);
    }
    else
    {
        // temporarily enable color channels requested via channelsToInit. Some emulated format has
        // some channels write mask disabled when the texture is created.
        MTLColorWriteMask oldMask = texture->getColorWritableMask();
        texture->setColorWritableMask(channelsToInit);

        // If there are some channels don't need to be initialized, we must use clearWithDraw.
        encoder = contextMtl->getRenderTargetCommandEncoder(tempRtt);

        const angle::Format &angleFormat = textureObjFormat.actualAngleFormat();

        ClearRectParams clearParams;
        ClearColorValue clearColor;
        if (angleFormat.isSint())
        {
            clearColor.setAsInt(0, 0, 0, clearAlpha);
        }
        else if (angleFormat.isUint())
        {
            clearColor.setAsUInt(0, 0, 0, clearAlpha);
        }
        else
        {
            clearColor.setAsFloat(0, 0, 0, clearAlpha);
        }
        clearParams.clearColor     = clearColor;
        clearParams.dstTextureSize = texture->sizeAt0();
        clearParams.enabledBuffers.set(0);
        clearParams.clearArea = gl::Rectangle(0, 0, texture->widthAt0(), texture->heightAt0());

        ANGLE_TRY(
            contextMtl->getDisplay()->getUtils().clearWithDraw(context, encoder, clearParams));

        // Restore texture's intended write mask
        texture->setColorWritableMask(oldMask);
    }
    encoder->setStoreAction(MTLStoreActionStore);

    return angle::Result::Continue;
}

angle::Result InitializeDepthStencilTextureContentsGPU(const gl::Context *context,
                                                       const TextureRef &texture,
                                                       const Format &textureObjFormat,
                                                       const ImageNativeIndex &index)
{
    const MipmapNativeLevel &level = index.getNativeLevel();
    // Use clear operation
    ContextMtl *contextMtl           = mtl::GetImpl(context);
    const angle::Format &angleFormat = textureObjFormat.actualAngleFormat();
    RenderTargetMtl rtMTL;

    uint32_t layer = index.hasLayer() ? index.getLayerIndex() : 0;
    rtMTL.set(texture, level, layer, textureObjFormat);
    mtl::RenderPassDesc rpDesc;
    if (angleFormat.depthBits)
    {
        rtMTL.toRenderPassAttachmentDesc(&rpDesc.depthAttachment);
        rpDesc.depthAttachment.loadAction = MTLLoadActionClear;
        rpDesc.depthAttachment.clearDepth = 1.0;
    }
    if (angleFormat.stencilBits)
    {
        rtMTL.toRenderPassAttachmentDesc(&rpDesc.stencilAttachment);
        rpDesc.stencilAttachment.loadAction = MTLLoadActionClear;
    }
    rpDesc.sampleCount = texture->samples();

    // End current render pass
    contextMtl->endEncoding(true);

    RenderCommandEncoder *encoder = contextMtl->getRenderPassCommandEncoder(rpDesc);
    encoder->setStoreAction(MTLStoreActionStore);

    return angle::Result::Continue;
}

angle::Result ReadTexturePerSliceBytes(const gl::Context *context,
                                       const TextureRef &texture,
                                       size_t bytesPerRow,
                                       const gl::Rectangle &fromRegion,
                                       const MipmapNativeLevel &mipLevel,
                                       uint32_t sliceOrDepth,
                                       uint8_t *dataOut)
{
    ASSERT(texture && texture->valid());
    ContextMtl *contextMtl = mtl::GetImpl(context);
    GLint layer            = 0;
    GLint startDepth       = 0;
    switch (texture->textureType())
    {
        case MTLTextureTypeCube:
        case MTLTextureType2DArray:
            layer = sliceOrDepth;
            break;
        case MTLTextureType3D:
            startDepth = sliceOrDepth;
            break;
        default:
            break;
    }

    MTLRegion mtlRegion = MTLRegionMake3D(fromRegion.x, fromRegion.y, startDepth, fromRegion.width,
                                          fromRegion.height, 1);

    texture->getBytes(contextMtl, bytesPerRow, 0, mtlRegion, mipLevel, layer, dataOut);

    return angle::Result::Continue;
}

angle::Result ReadTexturePerSliceBytesToBuffer(const gl::Context *context,
                                               const TextureRef &texture,
                                               size_t bytesPerRow,
                                               const gl::Rectangle &fromRegion,
                                               const MipmapNativeLevel &mipLevel,
                                               uint32_t sliceOrDepth,
                                               uint32_t dstOffset,
                                               const BufferRef &dstBuffer)
{
    ASSERT(texture && texture->valid());
    ContextMtl *contextMtl = mtl::GetImpl(context);
    GLint layer            = 0;
    GLint startDepth       = 0;
    switch (texture->textureType())
    {
        case MTLTextureTypeCube:
        case MTLTextureType2DArray:
            layer = sliceOrDepth;
            break;
        case MTLTextureType3D:
            startDepth = sliceOrDepth;
            break;
        default:
            break;
    }

    MTLRegion mtlRegion = MTLRegionMake3D(fromRegion.x, fromRegion.y, startDepth, fromRegion.width,
                                          fromRegion.height, 1);

    BlitCommandEncoder *blitEncoder = contextMtl->getBlitCommandEncoder();
    blitEncoder->copyTextureToBuffer(texture, layer, mipLevel, mtlRegion.origin, mtlRegion.size,
                                     dstBuffer, dstOffset, bytesPerRow, 0, MTLBlitOptionNone);

    return angle::Result::Continue;
}

MTLViewport GetViewport(const gl::Rectangle &rect, double znear, double zfar)
{
    MTLViewport re;

    re.originX = rect.x;
    re.originY = rect.y;
    re.width   = rect.width;
    re.height  = rect.height;
    re.znear   = znear;
    re.zfar    = zfar;

    return re;
}

MTLViewport GetViewportFlipY(const gl::Rectangle &rect,
                             NSUInteger screenHeight,
                             double znear,
                             double zfar)
{
    MTLViewport re;

    re.originX = rect.x;
    re.originY = static_cast<double>(screenHeight) - rect.y1();
    re.width   = rect.width;
    re.height  = rect.height;
    re.znear   = znear;
    re.zfar    = zfar;

    return re;
}

MTLViewport GetViewport(const gl::Rectangle &rect,
                        NSUInteger screenHeight,
                        bool flipY,
                        double znear,
                        double zfar)
{
    if (flipY)
    {
        return GetViewportFlipY(rect, screenHeight, znear, zfar);
    }

    return GetViewport(rect, znear, zfar);
}

MTLScissorRect GetScissorRect(const gl::Rectangle &rect, NSUInteger screenHeight, bool flipY)
{
    MTLScissorRect re;

    re.x      = rect.x;
    re.y      = flipY ? (screenHeight - rect.y1()) : rect.y;
    re.width  = rect.width;
    re.height = rect.height;

    return re;
}

uint32_t GetDeviceVendorId(id<MTLDevice> metalDevice)
{
    uint32_t vendorId = 0;
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    if (ANGLE_APPLE_AVAILABLE_XC(10.13, 13.1))
    {
        vendorId = GetDeviceVendorIdFromIOKit(metalDevice);
    }
#endif
    if (!vendorId)
    {
        vendorId = GetDeviceVendorIdFromName(metalDevice);
    }

    return vendorId;
}

static MTLLanguageVersion GetUserSetOrHighestMSLVersion(const MTLLanguageVersion currentVersion)
{
    const std::string major_str = angle::GetEnvironmentVar(kANGLEMSLVersionMajorEnv);
    const std::string minor_str = angle::GetEnvironmentVar(kANGLEMSLVersionMinorEnv);
    if (major_str != "" && minor_str != "")
    {
        const int major = std::stoi(major_str);
        const int minor = std::stoi(minor_str);
#if !defined(NDEBUG)
        NSLog(@"Forcing MSL Version: MTLLanguageVersion%d_%d\n", major, minor);
#endif
        switch (major)
        {
            case 1:
                switch (minor)
                {
#if (defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_9_0) && \
    (TARGET_OS_IOS || TARGET_OS_TV) && !TARGET_OS_MACCATALYST
                    case 0:
                        return MTLLanguageVersion1_0;
#endif
#if (defined(__MAC_10_11) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_11) ||    \
    (defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_9_0) || \
    (defined(__TVOS_9_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_9_0)
                    case 1:
                        return MTLLanguageVersion1_1;
#endif
#if (defined(__MAC_10_12) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_12) ||      \
    (defined(__IPHONE_10_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0) || \
    (defined(__TVOS_10_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_10_0)
                    case 2:
                        return MTLLanguageVersion1_2;
#endif
                    default:
                        assert(0 && "Unsupported MSL Minor Language Version.");
                }
                break;
            case 2:
                switch (minor)
                {
#if (defined(__MAC_10_13) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_13) ||      \
    (defined(__IPHONE_11_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_11_0) || \
    (defined(__TVOS_11_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_11_0)
                    case 0:
                        return MTLLanguageVersion2_0;
#endif
#if (defined(__MAC_10_14) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14) ||      \
    (defined(__IPHONE_12_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_12_0) || \
    (defined(__TVOS_12_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_12_0)
                    case 1:
                        return MTLLanguageVersion2_1;
#endif
#if (defined(__MAC_10_15) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_15) ||      \
    (defined(__IPHONE_13_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_13_0) || \
    (defined(__TVOS_13_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_13_0)
                    case 2:
                        return MTLLanguageVersion2_2;
#endif
#if (defined(__MAC_11_0) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0) ||        \
    (defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_14_0) || \
    (defined(__TVOS_14_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_14_0)
                    case 3:
                        return MTLLanguageVersion2_3;
#endif
                    default:
                        assert(0 && "Unsupported MSL Minor Language Version.");
                }
                break;
            default:
                assert(0 && "Unsupported MSL Major Language Version.");
        }
    }
    return currentVersion;
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(
    const mtl::ContextDevice &metalDevice,
    const std::string &source,
    const std::map<std::string, std::string> &substitutionMacros,
    bool disableFastMath,
    bool usesInvariance,
    AutoObjCPtr<NSError *> *error)
{
    return CreateShaderLibrary(metalDevice, source.c_str(), source.size(), substitutionMacros,
                               disableFastMath, usesInvariance, error);
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(const mtl::ContextDevice &metalDevice,
                                                const std::string &source,
                                                AutoObjCPtr<NSError *> *error)
{
    // Use fast math, but conservatively assume the shader uses invariance.
    return CreateShaderLibrary(metalDevice, source.c_str(), source.size(), {}, false, true, error);
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(
    const mtl::ContextDevice &metalDevice,
    const char *source,
    size_t sourceLen,
    const std::map<std::string, std::string> &substitutionMacros,
    bool disableFastMath,
    bool usesInvariance,
    AutoObjCPtr<NSError *> *errorOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSError *nsError = nil;
        auto nsSource    = [[NSString alloc] initWithBytesNoCopy:const_cast<char *>(source)
                                                       length:sourceLen
                                                     encoding:NSUTF8StringEncoding
                                                 freeWhenDone:NO];
        auto options     = [[[MTLCompileOptions alloc] init] ANGLE_MTL_AUTORELEASE];

        // Mark all positions in VS with attribute invariant as non-optimizable
        bool canPerserveInvariance = false;
#if defined(__MAC_11_0) || defined(__IPHONE_14_0) || defined(__TVOS_14_0)
        if (ANGLE_APPLE_AVAILABLE_XCI(11.0, 14.0, 14.0))
        {
            canPerserveInvariance      = true;
            options.preserveInvariance = usesInvariance;
        }
#endif

        // If either:
        //   - fastmath is force-disabled
        // or:
        //   - preserveInvariance is not available when compiling from
        //     source, and the sources use invariance
        // Disable fastmath.
        //
        // Write this logic out as if-tests rather than a nested
        // logical expression to make it clearer.
        if (disableFastMath)
        {
            options.fastMathEnabled = false;
        }
        else if (usesInvariance && !canPerserveInvariance)
        {
            options.fastMathEnabled = false;
        }

        options.languageVersion = GetUserSetOrHighestMSLVersion(options.languageVersion);

        if (!substitutionMacros.empty())
        {
            auto macroDict = [NSMutableDictionary dictionary];
            for (const auto &macro : substitutionMacros)
            {
                [macroDict setObject:@(macro.second.c_str()) forKey:@(macro.first.c_str())];
            }
            options.preprocessorMacros = macroDict;
        }

        auto *platform   = ANGLEPlatformCurrent();
        double startTime = platform->currentTime(platform);

        auto library = metalDevice.newLibraryWithSource(nsSource, options, &nsError);
        if (angle::GetEnvironmentVar(kANGLEPrintMSLEnv)[0] == '1')
        {
            NSLog(@"%@\n", nsSource);
        }
        [nsSource ANGLE_MTL_AUTORELEASE];
        *errorOut = std::move(nsError);

        double endTime = platform->currentTime(platform);
        int us         = static_cast<int>((endTime - startTime) * 1000'000.0);
        ANGLE_HISTOGRAM_COUNTS("GPU.ANGLE.MetalShaderCompilationTimeUs", us);

        return library;
    }
}

std::string CompileShaderLibraryToFile(const std::string &source,
                                       const std::map<std::string, std::string> &macros,
                                       bool disableFastMath,
                                       bool usesInvariance)
{
    auto tmpDir = angle::GetTempDirectory();
    if (!tmpDir.valid())
    {
        FATAL() << "angle::GetTempDirectory() failed";
    }
    // NOTE: metal/metallib seem to require extensions, otherwise they interpret the files
    // differently.
    auto metalFileName =
        angle::CreateTemporaryFileInDirectoryWithExtension(tmpDir.value(), ".metal");
    auto airFileName = angle::CreateTemporaryFileInDirectoryWithExtension(tmpDir.value(), ".air");
    auto metallibFileName =
        angle::CreateTemporaryFileInDirectoryWithExtension(tmpDir.value(), ".metallib");
    if (!metalFileName.valid() || !airFileName.valid() || !metallibFileName.valid())
    {
        FATAL() << "Unable to generate temporary files for compiling metal";
    }
    // Save the source.
    {
        angle::SaveFileHelper saveFileHelper(metalFileName.value());
        saveFileHelper << source;
    }

    // metal -> air
    std::vector<std::string> metalToAirArgv{"/usr/bin/xcrun",
                                            "/usr/bin/xcrun",
                                            "-sdk",
                                            "macosx",
                                            "metal",
                                            "-std=macos-metal2.0",
                                            "-mmacosx-version-min=10.13",
                                            "-c",
                                            metalFileName.value(),
                                            "-o",
                                            airFileName.value()};
    // Macros are passed using `-D key=value`.
    for (const auto &macro : macros)
    {
        metalToAirArgv.push_back("-D");
        // TODO: not sure if this needs to escape strings or what (for example, might
        // a space cause problems)?
        metalToAirArgv.push_back(macro.first + "=" + macro.second);
    }
    // TODO: is this right, not sure if MTLCompileOptions.fastMathEnabled is same as -ffast-math.
    if (!disableFastMath)
    {
        metalToAirArgv.push_back("-ffast-math");
    }
    if (usesInvariance)
    {
        metalToAirArgv.push_back("-fpreserve-invariance");
    }
    Process metalToAirProcess(metalToAirArgv);
    int exitCode = -1;
    if (!metalToAirProcess.DidLaunch() || !metalToAirProcess.WaitForExit(exitCode) || exitCode != 0)
    {
        FATAL() << "Generating air file failed";
    }

    // air -> metallib
    const std::vector<std::string> airToMetallibArgv{
        "xcrun",    "/usr/bin/xcrun",    "-sdk", "macosx",
        "metallib", airFileName.value(), "-o",   metallibFileName.value()};
    Process air_to_metallib_process(airToMetallibArgv);
    if (!air_to_metallib_process.DidLaunch() || !air_to_metallib_process.WaitForExit(exitCode) ||
        exitCode != 0)
    {
        FATAL() << "Ggenerating metallib file failed";
    }
    return metallibFileName.value();
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const char *source,
                                                size_t sourceLen,
                                                AutoObjCPtr<NSError *> *errorOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        auto nsSource = [[NSString alloc] initWithBytesNoCopy:const_cast<char *>(source)
                                                       length:sourceLen
                                                     encoding:NSUTF8StringEncoding
                                                 freeWhenDone:NO];
        auto options  = [[[MTLCompileOptions alloc] init] ANGLE_MTL_AUTORELEASE];

        NSError *nsError = nil;
        auto library = [metalDevice newLibraryWithSource:nsSource options:options error:&nsError];

        [nsSource ANGLE_MTL_AUTORELEASE];

        *errorOut = std::move(nsError);

        return [library ANGLE_MTL_AUTORELEASE];
    }
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibraryFromBinary(id<MTLDevice> metalDevice,
                                                          const uint8_t *binarySource,
                                                          size_t binarySourceLen,
                                                          AutoObjCPtr<NSError *> *errorOut)
{
    ANGLE_MTL_OBJC_SCOPE
    {
        NSError *nsError = nil;
        auto shaderSourceData =
            dispatch_data_create(binarySource, binarySourceLen, dispatch_get_main_queue(),
                                 ^{
                                 });

        auto library = [metalDevice newLibraryWithData:shaderSourceData error:&nsError];

        dispatch_release(shaderSourceData);

        *errorOut = std::move(nsError);

        return [library ANGLE_MTL_AUTORELEASE];
    }
}

MTLTextureType GetTextureType(gl::TextureType glType)
{
    switch (glType)
    {
        case gl::TextureType::_2D:
            return MTLTextureType2D;
        case gl::TextureType::_2DArray:
            return MTLTextureType2DArray;
        case gl::TextureType::_3D:
            return MTLTextureType3D;
        case gl::TextureType::CubeMap:
            return MTLTextureTypeCube;
        default:
            return MTLTextureTypeInvalid;
    }
}

MTLSamplerMinMagFilter GetFilter(GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_LINEAR_MIPMAP_NEAREST:
        case GL_LINEAR:
            return MTLSamplerMinMagFilterLinear;
        case GL_NEAREST_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_NEAREST:
            return MTLSamplerMinMagFilterNearest;
        default:
            UNIMPLEMENTED();
            return MTLSamplerMinMagFilterNearest;
    }
}

MTLSamplerMipFilter GetMipmapFilter(GLenum filter)
{
    switch (filter)
    {
        case GL_LINEAR:
        case GL_NEAREST:
            return MTLSamplerMipFilterNotMipmapped;
        case GL_LINEAR_MIPMAP_LINEAR:
        case GL_NEAREST_MIPMAP_LINEAR:
            return MTLSamplerMipFilterLinear;
        case GL_NEAREST_MIPMAP_NEAREST:
        case GL_LINEAR_MIPMAP_NEAREST:
            return MTLSamplerMipFilterNearest;
        default:
            UNIMPLEMENTED();
            return MTLSamplerMipFilterNotMipmapped;
    }
}

MTLSamplerAddressMode GetSamplerAddressMode(GLenum wrap)
{
    switch (wrap)
    {
        case GL_CLAMP_TO_EDGE:
            return MTLSamplerAddressModeClampToEdge;
#if !ANGLE_PLATFORM_WATCHOS
        case GL_MIRROR_CLAMP_TO_EDGE_EXT:
            return MTLSamplerAddressModeMirrorClampToEdge;
#endif
        case GL_REPEAT:
            return MTLSamplerAddressModeRepeat;
        case GL_MIRRORED_REPEAT:
            return MTLSamplerAddressModeMirrorRepeat;
        default:
            UNIMPLEMENTED();
            return MTLSamplerAddressModeClampToEdge;
    }
}

MTLBlendFactor GetBlendFactor(gl::BlendFactorType factor)
{
    switch (factor)
    {
        case gl::BlendFactorType::Zero:
            return MTLBlendFactorZero;
        case gl::BlendFactorType::One:
            return MTLBlendFactorOne;
        case gl::BlendFactorType::SrcColor:
            return MTLBlendFactorSourceColor;
        case gl::BlendFactorType::OneMinusSrcColor:
            return MTLBlendFactorOneMinusSourceColor;
        case gl::BlendFactorType::SrcAlpha:
            return MTLBlendFactorSourceAlpha;
        case gl::BlendFactorType::OneMinusSrcAlpha:
            return MTLBlendFactorOneMinusSourceAlpha;
        case gl::BlendFactorType::DstColor:
            return MTLBlendFactorDestinationColor;
        case gl::BlendFactorType::OneMinusDstColor:
            return MTLBlendFactorOneMinusDestinationColor;
        case gl::BlendFactorType::DstAlpha:
            return MTLBlendFactorDestinationAlpha;
        case gl::BlendFactorType::OneMinusDstAlpha:
            return MTLBlendFactorOneMinusDestinationAlpha;
        case gl::BlendFactorType::SrcAlphaSaturate:
            return MTLBlendFactorSourceAlphaSaturated;
        case gl::BlendFactorType::ConstantColor:
            return MTLBlendFactorBlendColor;
        case gl::BlendFactorType::OneMinusConstantColor:
            return MTLBlendFactorOneMinusBlendColor;
        case gl::BlendFactorType::ConstantAlpha:
            return MTLBlendFactorBlendAlpha;
        case gl::BlendFactorType::OneMinusConstantAlpha:
            return MTLBlendFactorOneMinusBlendAlpha;
        case gl::BlendFactorType::Src1Color:
            return MTLBlendFactorSource1Color;
        case gl::BlendFactorType::OneMinusSrc1Color:
            return MTLBlendFactorOneMinusSource1Color;
        case gl::BlendFactorType::Src1Alpha:
            return MTLBlendFactorSource1Alpha;
        case gl::BlendFactorType::OneMinusSrc1Alpha:
            return MTLBlendFactorOneMinusSource1Alpha;
        default:
            UNREACHABLE();
            return MTLBlendFactorZero;
    }
}

MTLBlendOperation GetBlendOp(gl::BlendEquationType op)
{
    switch (op)
    {
        case gl::BlendEquationType::Add:
            return MTLBlendOperationAdd;
        case gl::BlendEquationType::Subtract:
            return MTLBlendOperationSubtract;
        case gl::BlendEquationType::ReverseSubtract:
            return MTLBlendOperationReverseSubtract;
        case gl::BlendEquationType::Min:
            return MTLBlendOperationMin;
        case gl::BlendEquationType::Max:
            return MTLBlendOperationMax;
        default:
            UNREACHABLE();
            return MTLBlendOperationAdd;
    }
}

MTLCompareFunction GetCompareFunc(GLenum func)
{
    switch (func)
    {
        case GL_NEVER:
            return MTLCompareFunctionNever;
        case GL_ALWAYS:
            return MTLCompareFunctionAlways;
        case GL_LESS:
            return MTLCompareFunctionLess;
        case GL_LEQUAL:
            return MTLCompareFunctionLessEqual;
        case GL_EQUAL:
            return MTLCompareFunctionEqual;
        case GL_GREATER:
            return MTLCompareFunctionGreater;
        case GL_GEQUAL:
            return MTLCompareFunctionGreaterEqual;
        case GL_NOTEQUAL:
            return MTLCompareFunctionNotEqual;
        default:
            UNREACHABLE();
            return MTLCompareFunctionAlways;
    }
}

MTLStencilOperation GetStencilOp(GLenum op)
{
    switch (op)
    {
        case GL_KEEP:
            return MTLStencilOperationKeep;
        case GL_ZERO:
            return MTLStencilOperationZero;
        case GL_REPLACE:
            return MTLStencilOperationReplace;
        case GL_INCR:
            return MTLStencilOperationIncrementClamp;
        case GL_DECR:
            return MTLStencilOperationDecrementClamp;
        case GL_INCR_WRAP:
            return MTLStencilOperationIncrementWrap;
        case GL_DECR_WRAP:
            return MTLStencilOperationDecrementWrap;
        case GL_INVERT:
            return MTLStencilOperationInvert;
        default:
            UNREACHABLE();
            return MTLStencilOperationKeep;
    }
}

MTLWinding GetFrontfaceWinding(GLenum frontFaceMode, bool invert)
{
    switch (frontFaceMode)
    {
        case GL_CW:
            return invert ? MTLWindingCounterClockwise : MTLWindingClockwise;
        case GL_CCW:
            return invert ? MTLWindingClockwise : MTLWindingCounterClockwise;
        default:
            UNREACHABLE();
            return MTLWindingClockwise;
    }
}

#if ANGLE_MTL_PRIMITIVE_TOPOLOGY_CLASS_AVAILABLE
PrimitiveTopologyClass GetPrimitiveTopologyClass(gl::PrimitiveMode mode)
{
    // NOTE(hqle): Support layered renderring in future.
    // In non-layered rendering mode, unspecified is enough.
    return MTLPrimitiveTopologyClassUnspecified;
}
#else  // ANGLE_MTL_PRIMITIVE_TOPOLOGY_CLASS_AVAILABLE
PrimitiveTopologyClass GetPrimitiveTopologyClass(gl::PrimitiveMode mode)
{
    return kPrimitiveTopologyClassTriangle;
}
#endif

MTLPrimitiveType GetPrimitiveType(gl::PrimitiveMode mode)
{
    switch (mode)
    {
        case gl::PrimitiveMode::Triangles:
            return MTLPrimitiveTypeTriangle;
        case gl::PrimitiveMode::Points:
            return MTLPrimitiveTypePoint;
        case gl::PrimitiveMode::Lines:
            return MTLPrimitiveTypeLine;
        case gl::PrimitiveMode::LineStrip:
        case gl::PrimitiveMode::LineLoop:
            return MTLPrimitiveTypeLineStrip;
        case gl::PrimitiveMode::TriangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
        case gl::PrimitiveMode::TriangleFan:
            // NOTE(hqle): Emulate triangle fan.
        default:
            return MTLPrimitiveTypeInvalid;
    }
}

MTLIndexType GetIndexType(gl::DrawElementsType type)
{
    switch (type)
    {
        case gl::DrawElementsType::UnsignedShort:
            return MTLIndexTypeUInt16;
        case gl::DrawElementsType::UnsignedInt:
            return MTLIndexTypeUInt32;
        case gl::DrawElementsType::UnsignedByte:
            // NOTE(hqle): Convert to supported type
        default:
            return MTLIndexTypeInvalid;
    }
}

#if ANGLE_MTL_SWIZZLE_AVAILABLE
MTLTextureSwizzle GetTextureSwizzle(GLenum swizzle)
{
    switch (swizzle)
    {
        case GL_RED:
            return MTLTextureSwizzleRed;
        case GL_GREEN:
            return MTLTextureSwizzleGreen;
        case GL_BLUE:
            return MTLTextureSwizzleBlue;
        case GL_ALPHA:
            return MTLTextureSwizzleAlpha;
        case GL_ZERO:
            return MTLTextureSwizzleZero;
        case GL_ONE:
            return MTLTextureSwizzleOne;
        default:
            UNREACHABLE();
            return MTLTextureSwizzleZero;
    }
}
#endif

MTLColorWriteMask GetEmulatedColorWriteMask(const mtl::Format &mtlFormat, bool *isEmulatedOut)
{
    const angle::Format &intendedFormat = mtlFormat.intendedAngleFormat();
    const angle::Format &actualFormat   = mtlFormat.actualAngleFormat();
    bool isFormatEmulated               = false;
    MTLColorWriteMask colorWritableMask = MTLColorWriteMaskAll;
    if (intendedFormat.alphaBits == 0 && actualFormat.alphaBits)
    {
        isFormatEmulated = true;
        // Disable alpha write to this texture
        colorWritableMask = colorWritableMask & (~MTLColorWriteMaskAlpha);
    }
    if (intendedFormat.luminanceBits == 0)
    {
        if (intendedFormat.redBits == 0 && actualFormat.redBits)
        {
            isFormatEmulated = true;
            // Disable red write to this texture
            colorWritableMask = colorWritableMask & (~MTLColorWriteMaskRed);
        }
        if (intendedFormat.greenBits == 0 && actualFormat.greenBits)
        {
            isFormatEmulated = true;
            // Disable green write to this texture
            colorWritableMask = colorWritableMask & (~MTLColorWriteMaskGreen);
        }
        if (intendedFormat.blueBits == 0 && actualFormat.blueBits)
        {
            isFormatEmulated = true;
            // Disable blue write to this texture
            colorWritableMask = colorWritableMask & (~MTLColorWriteMaskBlue);
        }
    }

    *isEmulatedOut = isFormatEmulated;

    return colorWritableMask;
}

MTLColorWriteMask GetEmulatedColorWriteMask(const mtl::Format &mtlFormat)
{
    // Ignore isFormatEmulated boolean value
    bool isFormatEmulated;
    return GetEmulatedColorWriteMask(mtlFormat, &isFormatEmulated);
}

bool IsFormatEmulated(const mtl::Format &mtlFormat)
{
    bool isFormatEmulated;
    (void)GetEmulatedColorWriteMask(mtlFormat, &isFormatEmulated);
    return isFormatEmulated;
}

size_t EstimateTextureSizeInBytes(const mtl::Format &mtlFormat,
                                  size_t width,
                                  size_t height,
                                  size_t depth,
                                  size_t sampleCount,
                                  size_t numMips)
{
    size_t textureSizeInBytes;
    if (mtlFormat.getCaps().compressed)
    {
        GLuint textureSize;
        gl::Extents size((int)width, (int)height, (int)depth);
        if (!mtlFormat.intendedInternalFormat().computeCompressedImageSize(size, &textureSize))
        {
            return 0;
        }
        textureSizeInBytes = textureSize;
    }
    else
    {
        textureSizeInBytes = mtlFormat.getCaps().pixelBytes * width * height * depth * sampleCount;
    }
    if (numMips > 1)
    {
        // Estimate mipmap size.
        textureSizeInBytes = textureSizeInBytes * 4 / 3;
    }
    return textureSizeInBytes;
}

MTLClearColor EmulatedAlphaClearColor(MTLClearColor color, MTLColorWriteMask colorMask)
{
    MTLClearColor re = color;

    if (!(colorMask & MTLColorWriteMaskAlpha))
    {
        re.alpha = kEmulatedAlphaValue;
    }

    return re;
}

NSUInteger GetMaxRenderTargetSizeForDeviceInBytes(const mtl::ContextDevice &device)
{
    if (SupportsAppleGPUFamily(device, 4))
    {
        return 64;
    }
    else if (SupportsAppleGPUFamily(device, 2))
    {
        return 32;
    }
    else
    {
        return 16;
    }
}

NSUInteger GetMaxNumberOfRenderTargetsForDevice(const mtl::ContextDevice &device)
{
    if (SupportsAppleGPUFamily(device, 2) || SupportsMacGPUFamily(device, 1))
    {
        return 8;
    }
    else
    {
        return 4;
    }
}

bool DeviceHasMaximumRenderTargetSize(id<MTLDevice> device)
{
    return !SupportsMacGPUFamily(device, 1);
}

bool SupportsAppleGPUFamily(id<MTLDevice> device, uint8_t appleFamily)
{
#if (__MAC_OS_X_VERSION_MAX_ALLOWED >= 101500 || __IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) || \
    (__TV_OS_VERSION_MAX_ALLOWED >= 130000)
    // If device supports [MTLDevice supportsFamily:], then use it.
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13))
    {
        MTLGPUFamily family;
        switch (appleFamily)
        {
            case 1:
                family = MTLGPUFamilyApple1;
                break;
            case 2:
                family = MTLGPUFamilyApple2;
                break;
            case 3:
                family = MTLGPUFamilyApple3;
                break;
            case 4:
                family = MTLGPUFamilyApple4;
                break;
            case 5:
                family = MTLGPUFamilyApple5;
                break;
#    if TARGET_OS_IOS || (TARGET_OS_OSX && __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000)
            case 6:
                family = MTLGPUFamilyApple6;
                break;
#    endif
            default:
                return false;
        }
        return [device supportsFamily:family];
    }   // Metal 2.2
#endif  // __IPHONE_OS_VERSION_MAX_ALLOWED

#if (!TARGET_OS_IOS && !TARGET_OS_TV) || TARGET_OS_MACCATALYST
    return false;
#else
    // If device doesn't support [MTLDevice supportsFamily:], then use
    // [MTLDevice supportsFeatureSet:].
    MTLFeatureSet featureSet;
    switch (appleFamily)
    {
#    if TARGET_OS_IOS
        case 1:
            featureSet = MTLFeatureSet_iOS_GPUFamily1_v1;
            break;
        case 2:
            featureSet = MTLFeatureSet_iOS_GPUFamily2_v1;
            break;
        case 3:
            featureSet = MTLFeatureSet_iOS_GPUFamily3_v1;
            break;
        case 4:
            featureSet = MTLFeatureSet_iOS_GPUFamily4_v1;
            break;
#        if __IPHONE_OS_VERSION_MAX_ALLOWED >= 120000
        case 5:
            featureSet = MTLFeatureSet_iOS_GPUFamily5_v1;
            break;
#        endif  // __IPHONE_OS_VERSION_MAX_ALLOWED
#    elif TARGET_OS_TV
        case 1:
        case 2:
            featureSet = MTLFeatureSet_tvOS_GPUFamily1_v1;
            break;
#    endif  // TARGET_OS_IOS
        default:
            return false;
    }

    return [device supportsFeatureSet:featureSet];
#endif      // TARGET_OS_IOS || TARGET_OS_TV
}

#if (defined(__MAC_13_0) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_13_0) ||        \
    (defined(__IPHONE_16_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_16_0) || \
    (defined(__TVOS_16_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_16_0)
#    define ANGLE_MTL_FEATURE_SET_DEPRECATED 1
#    define ANGLE_MTL_GPU_FAMILY_MAC1_DEPRECATED 1
#endif

#if ANGLE_MTL_GPU_FAMILY_MAC1_DEPRECATED
#    define ANGLE_MTL_GPU_FAMILY_MAC1 MTLGPUFamilyMac2
#    define ANGLE_MTL_GPU_FAMILY_MAC2 MTLGPUFamilyMac2
#elif TARGET_OS_MACCATALYST
#    define ANGLE_MTL_GPU_FAMILY_MAC1 MTLGPUFamilyMacCatalyst1
#    define ANGLE_MTL_GPU_FAMILY_MAC2 MTLGPUFamilyMacCatalyst2
#else  // !ANGLE_MTL_GPU_FAMILY_MAC1_DEPRECATED && !TARGET_OS_MACCATALYST
#    define ANGLE_MTL_GPU_FAMILY_MAC1 MTLGPUFamilyMac1
#    define ANGLE_MTL_GPU_FAMILY_MAC2 MTLGPUFamilyMac2
#endif

bool SupportsMacGPUFamily(id<MTLDevice> device, uint8_t macFamily)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
#    if defined(__MAC_10_15)
    // If device supports [MTLDevice supportsFamily:], then use it.
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13))
    {
        MTLGPUFamily family;

        switch (macFamily)
        {
#        if TARGET_OS_MACCATALYST
            ANGLE_APPLE_ALLOW_DEPRECATED_BEGIN
            case 1:
                family = ANGLE_MTL_GPU_FAMILY_MAC1;
                break;
            case 2:
                family = ANGLE_MTL_GPU_FAMILY_MAC2;
                break;
                ANGLE_APPLE_ALLOW_DEPRECATED_END
#        else   // TARGET_OS_MACCATALYST
            ANGLE_APPLE_ALLOW_DEPRECATED_BEGIN
            case 1:
                family = MTLGPUFamilyMac1;
                break;
                ANGLE_APPLE_ALLOW_DEPRECATED_END
            case 2:
                family = MTLGPUFamilyMac2;
                break;
#        endif  // TARGET_OS_MACCATALYST
            default:
                return false;
        }

        return [device supportsFamily:family];
    }  // Metal 2.2
#    endif

    // If device doesn't support [MTLDevice supportsFamily:], then use
    // [MTLDevice supportsFeatureSet:].
#    if TARGET_OS_MACCATALYST || ANGLE_MTL_FEATURE_SET_DEPRECATED
    UNREACHABLE();
    return false;
#    else

    ANGLE_APPLE_ALLOW_DEPRECATED_BEGIN
    MTLFeatureSet featureSet;
    switch (macFamily)
    {
        case 1:
            featureSet = MTLFeatureSet_macOS_GPUFamily1_v1;
            break;
#        if defined(__MAC_10_14)
        case 2:
            featureSet = MTLFeatureSet_macOS_GPUFamily2_v1;
            break;
#        endif
        default:
            return false;
    }
    return [device supportsFeatureSet:featureSet];
    ANGLE_APPLE_ALLOW_DEPRECATED_END
#    endif  // TARGET_OS_MACCATALYST
#else       // #if TARGET_OS_OSX || TARGET_OS_MACCATALYST

    return false;

#endif
}

static NSUInteger getNextLocationForFormat(const FormatCaps &caps,
                                           bool isMSAA,
                                           NSUInteger currentRenderTargetSize)
{
    assert(!caps.compressed);
    uint8_t alignment         = caps.alignment;
    NSUInteger pixelBytes     = caps.pixelBytes;
    NSUInteger pixelBytesMSAA = caps.pixelBytesMSAA;
    pixelBytes                = isMSAA ? pixelBytesMSAA : pixelBytes;

    currentRenderTargetSize = (currentRenderTargetSize + (alignment - 1)) & ~(alignment - 1);
    currentRenderTargetSize += pixelBytes;
    return currentRenderTargetSize;
}

static NSUInteger getNextLocationForAttachment(const mtl::RenderPassAttachmentDesc &attachment,
                                               const Context *context,
                                               NSUInteger currentRenderTargetSize)
{
    mtl::TextureRef texture =
        attachment.implicitMSTexture ? attachment.implicitMSTexture : attachment.texture;

    if (texture)
    {
        MTLPixelFormat pixelFormat = texture->pixelFormat();
        bool isMsaa                = texture->samples();
        const FormatCaps &caps     = context->getDisplay()->getNativeFormatCaps(pixelFormat);
        currentRenderTargetSize = getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
    }
    return currentRenderTargetSize;
}

NSUInteger ComputeTotalSizeUsedForMTLRenderPassDescriptor(const mtl::RenderPassDesc &descriptor,
                                                          const Context *context,
                                                          const mtl::ContextDevice &device)
{
    NSUInteger currentRenderTargetSize = 0;

    for (NSUInteger i = 0; i < GetMaxNumberOfRenderTargetsForDevice(device); i++)
    {
        currentRenderTargetSize = getNextLocationForAttachment(descriptor.colorAttachments[i],
                                                               context, currentRenderTargetSize);
    }
    if (descriptor.depthAttachment.texture == descriptor.stencilAttachment.texture)
    {
        currentRenderTargetSize = getNextLocationForAttachment(descriptor.depthAttachment, context,
                                                               currentRenderTargetSize);
    }
    else
    {
        currentRenderTargetSize = getNextLocationForAttachment(descriptor.depthAttachment, context,
                                                               currentRenderTargetSize);
        currentRenderTargetSize = getNextLocationForAttachment(descriptor.stencilAttachment,
                                                               context, currentRenderTargetSize);
    }

    return currentRenderTargetSize;
}

NSUInteger ComputeTotalSizeUsedForMTLRenderPipelineDescriptor(
    const MTLRenderPipelineDescriptor *descriptor,
    const Context *context,
    const mtl::ContextDevice &device)
{
    NSUInteger currentRenderTargetSize = 0;
    ANGLE_APPLE_ALLOW_DEPRECATED_BEGIN
    bool isMsaa = descriptor.sampleCount > 1;
    ANGLE_APPLE_ALLOW_DEPRECATED_END
    for (NSUInteger i = 0; i < GetMaxNumberOfRenderTargetsForDevice(device); i++)
    {
        MTLRenderPipelineColorAttachmentDescriptor *color = descriptor.colorAttachments[i];
        if (color.pixelFormat != MTLPixelFormatInvalid)
        {
            const FormatCaps &caps = context->getDisplay()->getNativeFormatCaps(color.pixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
    }
    if (descriptor.depthAttachmentPixelFormat == descriptor.stencilAttachmentPixelFormat)
    {
        if (descriptor.depthAttachmentPixelFormat != MTLPixelFormatInvalid)
        {
            const FormatCaps &caps =
                context->getDisplay()->getNativeFormatCaps(descriptor.depthAttachmentPixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
    }
    else
    {
        if (descriptor.depthAttachmentPixelFormat != MTLPixelFormatInvalid)
        {
            const FormatCaps &caps =
                context->getDisplay()->getNativeFormatCaps(descriptor.depthAttachmentPixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
        if (descriptor.stencilAttachmentPixelFormat != MTLPixelFormatInvalid)
        {
            const FormatCaps &caps =
                context->getDisplay()->getNativeFormatCaps(descriptor.stencilAttachmentPixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
    }
    return currentRenderTargetSize;
}

gl::Box MTLRegionToGLBox(const MTLRegion &mtlRegion)
{
    return gl::Box(static_cast<int>(mtlRegion.origin.x), static_cast<int>(mtlRegion.origin.y),
                   static_cast<int>(mtlRegion.origin.z), static_cast<int>(mtlRegion.size.width),
                   static_cast<int>(mtlRegion.size.height), static_cast<int>(mtlRegion.size.depth));
}

MipmapNativeLevel GetNativeMipLevel(GLuint level, GLuint base)
{
    ASSERT(level >= base);
    return MipmapNativeLevel(level - base);
}

GLuint GetGLMipLevel(const MipmapNativeLevel &nativeLevel, GLuint base)
{
    return nativeLevel.get() + base;
}

angle::Result TriangleFanBoundCheck(ContextMtl *context, size_t numTris)
{
    bool indexCheck =
        (numTris > std::numeric_limits<unsigned int>::max() / (sizeof(unsigned int) * 3));
    ANGLE_CHECK(context, !indexCheck,
                "Failed to create a scratch index buffer for GL_TRIANGLE_FAN, "
                "too many indices required.",
                GL_OUT_OF_MEMORY);
    return angle::Result::Continue;
}

angle::Result GetTriangleFanIndicesCount(ContextMtl *context,
                                         GLsizei vetexCount,
                                         uint32_t *numElemsOut)
{
    size_t numTris = vetexCount - 2;
    ANGLE_TRY(TriangleFanBoundCheck(context, numTris));
    size_t numIndices = numTris * 3;
    ANGLE_CHECK(context, numIndices <= std::numeric_limits<uint32_t>::max(),
                "Failed to create a scratch index buffer for GL_TRIANGLE_FAN, "
                "too many indices required.",
                GL_OUT_OF_MEMORY);

    *numElemsOut = static_cast<uint32_t>(numIndices);
    return angle::Result::Continue;
}

angle::Result CreateMslShader(mtl::Context *context,
                              id<MTLLibrary> shaderLib,
                              NSString *shaderName,
                              MTLFunctionConstantValues *funcConstants,
                              id<MTLFunction> *shaderOut)
{
    NSError *nsErr = nil;

    id<MTLFunction> mtlShader;
    if (funcConstants)
    {
        mtlShader = [shaderLib newFunctionWithName:shaderName
                                    constantValues:funcConstants
                                             error:&nsErr];
    }
    else
    {
        mtlShader = [shaderLib newFunctionWithName:shaderName];
    }

    [mtlShader ANGLE_MTL_AUTORELEASE];
    if (nsErr && !mtlShader)
    {
        std::ostringstream ss;
        ss << "Internal error compiling Metal shader:\n"
           << nsErr.localizedDescription.UTF8String << "\n";

        ERR() << ss.str();

        ANGLE_MTL_CHECK(context, false, GL_INVALID_OPERATION);
    }
    *shaderOut = mtlShader;
    return angle::Result::Continue;
}

angle::Result CreateMslShader(Context *context,
                              id<MTLLibrary> shaderLib,
                              NSString *shaderName,
                              MTLFunctionConstantValues *funcConstants,
                              AutoObjCPtr<id<MTLFunction>> *shaderOut)
{
    id<MTLFunction> outFunction;
    ANGLE_TRY(CreateMslShader(context, shaderLib, shaderName, funcConstants, &outFunction));
    shaderOut->retainAssign(outFunction);
    return angle::Result::Continue;
}
}  // namespace mtl
}  // namespace rx
