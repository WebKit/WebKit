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
#include "common/system_utils.h"
#include "gpu_info_util/SystemInfo.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/RenderTargetMtl.h"
#include "libANGLE/renderer/metal/mtl_render_utils.h"

namespace rx
{
namespace mtl
{

constexpr char kANGLEPrintMSLEnv[] = "ANGLE_METAL_PRINT_MSL_ENABLE";
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

    constexpr Vendor kVendors[] = {{@"AMD", angle::kVendorID_AMD},
                                   {@"Radeon", angle::kVendorID_AMD},
                                   {@"Intel", angle::kVendorID_Intel},
                                   {@"Geforce", angle::kVendorID_NVIDIA},
                                   {@"Quadro", angle::kVendorID_NVIDIA}};
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

}

angle::Result InitializeTextureContents(const gl::Context *context,
                                        const TextureRef &texture,
                                        const Format &textureObjFormat,
                                        const ImageNativeIndex &index)
{
    ASSERT(texture && texture->valid());
    ContextMtl *contextMtl = mtl::GetImpl(context);

    const angle::Format &actualAngleFormat           = textureObjFormat.actualAngleFormat();
    const gl::InternalFormat &intendedInternalFormat = textureObjFormat.intendedInternalFormat();

    bool forceGPUInitialization = false;
#if TARGET_OS_SIMULATOR
    forceGPUInitialization = true;
#endif // TARGET_OS_SIMULATOR
    
    // This function is called in many places to initialize the content of a texture.
    // So it's better we do the sanity check here instead of let the callers do it themselves:
    if (!textureObjFormat.valid() || actualAngleFormat.isBlock || actualAngleFormat.depthBits > 0 ||
        actualAngleFormat.stencilBits > 0)
    {
        // If dst format is compressed, ignore.
        return angle::Result::Continue;
    }

    gl::Extents size = texture->size(index);

    // Intialize the content to black
    GLint layer      = 0;
    GLint startDepth = 0;
    if (index.hasLayer())
    {
        switch (index.getType())
        {
            case gl::TextureType::CubeMap:
                layer = index.cubeMapFaceIndex();
                break;
            case gl::TextureType::_2DArray:
                layer = index.getLayerIndex();
                break;
            case gl::TextureType::_3D:
                startDepth = index.getLayerIndex();
                break;
            default:
                UNREACHABLE();
                break;
        }
    }

    if (texture->isCPUAccessible() && index.getType() != gl::TextureType::_2DMultisample &&
        index.getType() != gl::TextureType::_2DMultisampleArray && !forceGPUInitialization)
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
            if (!textureObjFormat.valid() || intendedInternalFormat.compressed ||
                intendedInternalFormat.depthBits > 0 || intendedInternalFormat.stencilBits > 0)
            {
                // If source format is compressed, ignore.
                return angle::Result::Continue;
            }

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
    }  // if (texture->isCPUAccessible())
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

    float clearAlpha = 0;
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
            clearColor.setAsInt(0, 0, 0, (int)clearAlpha);
        }
        else if (angleFormat.isUint())
        {
            clearColor.setAsUInt(0, 0, 0, (int)clearAlpha);
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
    // For formats such as MTLPixelFormatStencil8/GL_STENCIL_INDEX8 we only want to set the stencil attachment.
    if (angleFormat.stencilBits && !angleFormat.depthBits)
    {
        rtMTL.toRenderPassAttachmentDesc(&rpDesc.stencilAttachment);
    }
    else
    {
        rtMTL.toRenderPassAttachmentDesc(&rpDesc.depthAttachment);
    }
    rpDesc.sampleCount = texture->samples();
    if (angleFormat.depthBits)
    {
        rpDesc.depthAttachment.loadAction   = MTLLoadActionClear;
        rpDesc.depthAttachment.clearDepth   = 1.0;
    }
    if (angleFormat.stencilBits)
    {
        rpDesc.stencilAttachment.loadAction   = MTLLoadActionClear;
    }

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
    if (ANGLE_APPLE_AVAILABLE_XC(10.13, 13.0))
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
#if (defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_9_0) &&\
    (TARGET_OS_IOS || TARGET_OS_TV) && !TARGET_OS_MACCATALYST
                    case 0: return MTLLanguageVersion1_0;
#endif
#if (defined(__MAC_10_11) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_11) ||\
    (defined(__IPHONE_9_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_9_0) ||\
    (defined(__TVOS_9_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_9_0)
                    case 1: return MTLLanguageVersion1_1;
#endif
#if (defined(__MAC_10_12) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_12) ||\
    (defined(__IPHONE_10_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0) ||\
    (defined(__TVOS_10_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_10_0)
                    case 2: return MTLLanguageVersion1_2;
#endif
                    default:
                        assert(0 && "Unsupported MSL Minor Language Version.");
                }
                break;
            case 2:
                switch (minor)
                {
#if (defined(__MAC_10_13) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_13) ||\
    (defined(__IPHONE_11_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_11_0) ||\
    (defined(__TVOS_11_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_11_0)
                    case 0: return MTLLanguageVersion2_0;
#endif
#if (defined(__MAC_10_14) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_14) ||\
    (defined(__IPHONE_12_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_12_0) ||\
    (defined(__TVOS_12_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_12_0)
                    case 1: return MTLLanguageVersion2_1;
#endif
#if (defined(__MAC_10_15) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_15) ||\
    (defined(__IPHONE_13_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_13_0) ||\
    (defined(__TVOS_13_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_13_0)
                    case 2: return MTLLanguageVersion2_2;
#endif
#if (defined(__MAC_11_0) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0) ||\
    (defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_14_0) ||\
    (defined(__TVOS_14_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_14_0)
                    case 3: return MTLLanguageVersion2_3;
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

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const std::string &source,
                                                NSDictionary<NSString *, NSObject *> * substitutionMacros,
                                                bool enableFastMath,
                                                AutoObjCPtr<NSError *> *error)
{
    return CreateShaderLibrary(metalDevice, source.c_str(), source.size(), substitutionMacros, enableFastMath, error);
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const std::string &source,
                                                AutoObjCPtr<NSError *> *error)
{
    return CreateShaderLibrary(metalDevice, source.c_str(), source.size(),@{}, true, error);
}

AutoObjCPtr<id<MTLLibrary>> CreateShaderLibrary(id<MTLDevice> metalDevice,
                                                const char *source,
                                                size_t sourceLen,
                                                NSDictionary<NSString *, NSObject *> * substitutionMacros,
                                                bool enableFastMath,
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
#if (defined(__MAC_11_0) && __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_11_0) ||\
    (defined(__IPHONE_14_0) && __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_14_0) ||\
    (defined(__TVOS_14_0) && __TV_OS_VERSION_MIN_REQUIRED >= __TVOS_14_0)
        options.preserveInvariance = true;
#else
        // No preserveInvariance available compiling from source, so just disable fastmath.
        options.fastMathEnabled = false;
#endif
        options.languageVersion = GetUserSetOrHighestMSLVersion(options.languageVersion);
        // TODO(jcunningham): workaround for intel driver not preserving invariance on all shaders
        options.fastMathEnabled &= enableFastMath;
        options.preprocessorMacros = substitutionMacros;
        auto library = [metalDevice newLibraryWithSource:nsSource options:options error:&nsError];
        if (angle::GetEnvironmentVar(kANGLEPrintMSLEnv)[0] == '1')
        {
            NSLog(@"%@\n", nsSource);
        }
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

        [shaderSourceData ANGLE_MTL_AUTORELEASE];

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
        case GL_REPEAT:
            return MTLSamplerAddressModeRepeat;
        case GL_MIRRORED_REPEAT:
            return MTLSamplerAddressModeMirrorRepeat;
        case GL_CLAMP_TO_BORDER:
            // ES doesn't have border support
            return MTLSamplerAddressModeClampToEdge;
        case GL_CLAMP_TO_EDGE:
            return MTLSamplerAddressModeClampToEdge;
        default:
            UNIMPLEMENTED();
            return MTLSamplerAddressModeClampToEdge;
    }
}

MTLBlendFactor GetBlendFactor(GLenum factor)
{
    switch (factor)
    {
        case GL_ZERO:
            return MTLBlendFactorZero;
        case GL_ONE:
            return MTLBlendFactorOne;
        case GL_SRC_COLOR:
            return MTLBlendFactorSourceColor;
        case GL_DST_COLOR:
            return MTLBlendFactorDestinationColor;
        case GL_ONE_MINUS_SRC_COLOR:
            return MTLBlendFactorOneMinusSourceColor;
        case GL_SRC_ALPHA:
            return MTLBlendFactorSourceAlpha;
        case GL_ONE_MINUS_SRC_ALPHA:
            return MTLBlendFactorOneMinusSourceAlpha;
        case GL_DST_ALPHA:
            return MTLBlendFactorDestinationAlpha;
        case GL_ONE_MINUS_DST_ALPHA:
            return MTLBlendFactorOneMinusDestinationAlpha;
        case GL_ONE_MINUS_DST_COLOR:
            return MTLBlendFactorOneMinusDestinationColor;
        case GL_SRC_ALPHA_SATURATE:
            return MTLBlendFactorSourceAlphaSaturated;
        case GL_CONSTANT_COLOR:
            return MTLBlendFactorBlendColor;
        case GL_CONSTANT_ALPHA:
            return MTLBlendFactorBlendAlpha;
        case GL_ONE_MINUS_CONSTANT_COLOR:
            return MTLBlendFactorOneMinusBlendColor;
        case GL_ONE_MINUS_CONSTANT_ALPHA:
            return MTLBlendFactorOneMinusBlendAlpha;
        default:
            UNREACHABLE();
            return MTLBlendFactorZero;
    }
}

MTLBlendOperation GetBlendOp(GLenum op)
{
    switch (op)
    {
        case GL_FUNC_ADD:
            return MTLBlendOperationAdd;
        case GL_FUNC_SUBTRACT:
            return MTLBlendOperationSubtract;
        case GL_FUNC_REVERSE_SUBTRACT:
            return MTLBlendOperationReverseSubtract;
        case GL_MIN:
            return MTLBlendOperationMin;
        case GL_MAX:
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

MTLWinding GetFontfaceWinding(GLenum frontFaceMode, bool invert)
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

MTLColorWriteMask GetEmulatedColorWriteMask(const mtl::Format &mtlFormat, bool *emulatedChannelsOut)
{
    const angle::Format &intendedFormat = mtlFormat.intendedAngleFormat();
    const angle::Format &actualFormat   = mtlFormat.actualAngleFormat();
    bool emulatedChannels               = false;
    MTLColorWriteMask colorWritableMask = MTLColorWriteMaskAll;
    if (intendedFormat.alphaBits == 0 && actualFormat.alphaBits)
    {
        emulatedChannels = true;
        // Disable alpha write to this texture
        colorWritableMask = colorWritableMask & (~MTLColorWriteMaskAlpha);
    }
    if (intendedFormat.luminanceBits == 0)
    {
        if (intendedFormat.redBits == 0 && actualFormat.redBits)
        {
            emulatedChannels = true;
            // Disable red write to this texture
            colorWritableMask = colorWritableMask & (~MTLColorWriteMaskRed);
        }
        if (intendedFormat.greenBits == 0 && actualFormat.greenBits)
        {
            emulatedChannels = true;
            // Disable green write to this texture
            colorWritableMask = colorWritableMask & (~MTLColorWriteMaskGreen);
        }
        if (intendedFormat.blueBits == 0 && actualFormat.blueBits)
        {
            emulatedChannels = true;
            // Disable blue write to this texture
            colorWritableMask = colorWritableMask & (~MTLColorWriteMaskBlue);
        }
    }

    *emulatedChannelsOut = emulatedChannels;

    return colorWritableMask;
}

MTLColorWriteMask GetEmulatedColorWriteMask(const mtl::Format &mtlFormat)
{
    // Ignore emulatedChannels boolean value
    bool emulatedChannels;
    return GetEmulatedColorWriteMask(mtlFormat, &emulatedChannels);
}

bool IsFormatEmulated(const mtl::Format &mtlFormat)
{
    bool emulatedChannels;
    (void)GetEmulatedColorWriteMask(mtlFormat, &emulatedChannels);
    return emulatedChannels;
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

NSUInteger GetMaxRenderTargetSizeForDeviceInBytes(id<MTLDevice> device)
{
    if (SupportsIOSGPUFamily(device, 4))
    {
        return 64;
    }
    else if (SupportsIOSGPUFamily(device, 2))
    {
        return 32;
    }
    else
    {
        return 16;
    }
}

NSUInteger GetMaxNumberOfRenderTargetsForDevice(id<MTLDevice> device)
{
    if (SupportsIOSGPUFamily(device, 2)||SupportsMacGPUFamily(device, 1))
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
        return SupportsIOSGPUFamily(device, 1);
}

bool SupportsIOSGPUFamily(id<MTLDevice> device, uint8_t iOSFamily)
{
#if (!TARGET_OS_IOS && !TARGET_OS_TV) || TARGET_OS_MACCATALYST
    return false;
#else
#    if (__IPHONE_OS_VERSION_MAX_ALLOWED >= 130000) || (__TV_OS_VERSION_MAX_ALLOWED >= 130000)
    // If device supports [MTLDevice supportsFamily:], then use it.
    if (ANGLE_APPLE_AVAILABLE_I(13.0))
    {
        MTLGPUFamily family;
        switch (iOSFamily)
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
#        if TARGET_OS_IOS
            case 6:
                family = MTLGPUFamilyApple6;
                break;
#        endif
            default:
                return false;
        }
        return [device supportsFamily:family];
    }  // Metal 2.2
#    endif  // __IPHONE_OS_VERSION_MAX_ALLOWED

    // If device doesn't support [MTLDevice supportsFamily:], then use
    // [MTLDevice supportsFeatureSet:].
    MTLFeatureSet featureSet;
    switch (iOSFamily)
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

bool SupportsMacGPUFamily(id<MTLDevice> device, uint8_t macFamily)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
#    if defined(__MAC_10_15)
    // If device supports [MTLDevice supportsFamily:], then use it.
    if (ANGLE_APPLE_AVAILABLE_XC(10.15, 13.0))
    {
        MTLGPUFamily family;

        switch (macFamily)
        {
#        if TARGET_OS_MACCATALYST
            case 1:
                family = MTLGPUFamilyMacCatalyst1;
                break;
            case 2:
                family = MTLGPUFamilyMacCatalyst2;
                break;
#        else   // TARGET_OS_MACCATALYST
            case 1:
                family = MTLGPUFamilyMac1;
                break;
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
#    if TARGET_OS_MACCATALYST
    UNREACHABLE();
    return false;
#    else
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

NSUInteger ComputeTotalSizeUsedForMTLRenderPassDescriptor(const MTLRenderPassDescriptor *descriptor,
                                                          const Context *context,
                                                          id<MTLDevice> device)
{
    NSUInteger currentRenderTargetSize = 0;

    for (NSUInteger i = 0; i < GetMaxNumberOfRenderTargetsForDevice(device); i++)
    {
        MTLPixelFormat pixelFormat = descriptor.colorAttachments[i].texture.pixelFormat;
        bool isMsaa                = descriptor.colorAttachments[i].texture.sampleCount > 1;
        if (pixelFormat != MTLPixelFormatInvalid)
        {
            const FormatCaps &caps = context->getDisplay()->getNativeFormatCaps(pixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
    }
    if (descriptor.depthAttachment.texture.pixelFormat ==
        descriptor.stencilAttachment.texture.pixelFormat)
    {
        bool isMsaa = descriptor.depthAttachment.texture.sampleCount > 1;
        if (descriptor.depthAttachment.texture.pixelFormat != MTLPixelFormatInvalid)
        {
            const FormatCaps &caps = context->getDisplay()->getNativeFormatCaps(
                descriptor.depthAttachment.texture.pixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
    }
    else
    {
        if (descriptor.depthAttachment.texture.pixelFormat != MTLPixelFormatInvalid)
        {
            bool isMsaa            = descriptor.depthAttachment.texture.sampleCount > 1;
            const FormatCaps &caps = context->getDisplay()->getNativeFormatCaps(
                descriptor.depthAttachment.texture.pixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
        if (descriptor.stencilAttachment.texture.pixelFormat != MTLPixelFormatInvalid)
        {
            bool isMsaa            = descriptor.stencilAttachment.texture.sampleCount > 1;
            const FormatCaps &caps = context->getDisplay()->getNativeFormatCaps(
                descriptor.stencilAttachment.texture.pixelFormat);
            currentRenderTargetSize =
                getNextLocationForFormat(caps, isMsaa, currentRenderTargetSize);
        }
    }
    return currentRenderTargetSize;
}
NSUInteger ComputeTotalSizeUsedForMTLRenderPipelineDescriptor(
    const MTLRenderPipelineDescriptor *descriptor,
    const Context *context,
    id<MTLDevice> device)
{
    NSUInteger currentRenderTargetSize = 0;
    bool isMsaa                        = descriptor.sampleCount > 1;
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
                              NSString * shaderName,
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
                              NSString * shaderName,
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
