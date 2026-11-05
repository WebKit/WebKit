/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#import "BindableResource.h"
#import <Metal/Metal.h>
#import <WebGPU/WebGPUCpp.h>
#import <wtf/FastMalloc.h>
#import <wtf/HashCountedSet.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/Lock.h>
#import <wtf/Ref.h>
#import <wtf/Seconds.h>
#import <wtf/SwiftBridging.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/ThreadSafeWeakHashSet.h>
#import <wtf/TypeCasts.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>
#import <wtf/WeakPtr.h>

namespace WebGPU::Metal {

class BindGroup;
class CommandEncoder;
class Device;
class RenderBundle;
class TextureView;
struct ResolvedTextureViewDescriptor;

// https://gpuweb.github.io/gpuweb/#gputexture
class Texture final : public WebGPU::Texture, public TrackedResource {
    WTF_MAKE_TZONE_ALLOCATED(Texture);
public:
    // The texture allows views in the given view formats, not in descriptor.viewFormats.
    static Ref<Texture> create(id<MTLTexture> texture, const WebGPU::TextureDescriptor& descriptor, Vector<WebGPU::TextureFormat>&& viewFormats, Device& device)
    {
        return adoptRef(*new Texture(texture, descriptor, WTF::move(viewFormats), device));
    }
    static Ref<Texture> createInvalid(Device& device)
    {
        return adoptRef(*new Texture(device));
    }

    ~Texture();

    // std::nullopt is a descriptor with all members at their defaults.
    RefPtr<WebGPU::TextureView> createView(const std::optional<WebGPU::TextureViewDescriptor>&) final;
    void destroy() final;
    void undestroy() final;
    void setLabel(String&&) final;

    NODELETE bool isValid() const final;

    NODELETE static uint32_t texelBlockWidth(WebGPU::TextureFormat); // Texels
    NODELETE static uint32_t texelBlockHeight(WebGPU::TextureFormat); // Texels
    NODELETE static NSUInteger bytesPerRow(WebGPU::TextureFormat, uint32_t textureWidth, uint32_t sampleCount);
    NODELETE static WebGPU::Extent3D physicalTextureExtent(WebGPU::TextureDimension, WebGPU::TextureFormat, WebGPU::Extent3D logicalExtent);

    // For depth-stencil textures, the input value to texelBlockSize()
    // needs to be the output of aspectSpecificFormat().
    NODELETE static Checked<uint32_t> texelBlockSize(WebGPU::TextureFormat); // Bytes
    NODELETE static bool containsDepthAspect(WebGPU::TextureFormat);
    NODELETE static bool containsStencilAspect(WebGPU::TextureFormat);
    NODELETE static bool isDepthOrStencilFormat(WebGPU::TextureFormat);
    NODELETE static std::optional<WebGPU::TextureFormat> aspectSpecificFormat(WebGPU::TextureFormat, WebGPU::TextureAspect);
    static NSString* errorValidatingImageCopyTexture(const WebGPU::TexelCopyTextureInfo&, const WebGPU::Extent3D&);
    static NSString* errorValidatingTextureCopyRange(const WebGPU::TexelCopyTextureInfo&, const WebGPU::Extent3D&);
    NODELETE static bool refersToSingleAspect(WebGPU::TextureFormat, WebGPU::TextureAspect);
    NODELETE static bool isValidDepthStencilCopySource(WebGPU::TextureFormat, WebGPU::TextureAspect);
    NODELETE static bool isValidDepthStencilCopyDestination(WebGPU::TextureFormat, WebGPU::TextureAspect);
    static NSString* errorValidatingLinearTextureData(const WebGPU::TexelCopyBufferLayout&, uint64_t, WebGPU::TextureFormat, const WebGPU::Extent3D&);
    NODELETE static MTLTextureUsage usage(OptionSet<WebGPU::TextureUsage>, WebGPU::TextureFormat);
    NODELETE static MTLPixelFormat pixelFormat(WebGPU::TextureFormat);
    NODELETE static std::optional<WebGPU::TextureFormat> textureFormat(MTLPixelFormat);
    NODELETE static std::optional<MTLPixelFormat> depthOnlyAspectMetalFormat(WebGPU::TextureFormat);
    NODELETE static std::optional<MTLPixelFormat> stencilOnlyAspectMetalFormat(WebGPU::TextureFormat);
    NODELETE static WebGPU::TextureFormat removeSRGBSuffix(WebGPU::TextureFormat);
    NODELETE static std::optional<WebGPU::TextureFormat> resolveTextureFormat(WebGPU::TextureFormat, WebGPU::TextureAspect);
    NODELETE static bool isCompressedFormat(WebGPU::TextureFormat);
    enum class CompressFormat {
        ASTC, // NOLINT
        BC, // NOLINT
        ETC // NOLINT
    };
    NODELETE static std::optional<CompressFormat> compressedFormatType(WebGPU::TextureFormat);
    static bool isRenderableFormat(WebGPU::TextureFormat, const Device&);
    static bool isColorRenderableFormat(WebGPU::TextureFormat, const Device&);
    NODELETE static bool isDepthStencilRenderableFormat(WebGPU::TextureFormat, const Device&);
    NODELETE static uint32_t renderTargetPixelByteCost(WebGPU::TextureFormat);
    NODELETE static uint32_t renderTargetPixelByteAlignment(WebGPU::TextureFormat);

    WebGPU::Extent3D logicalMiplevelSpecificTextureExtent(uint32_t mipLevel);
    WebGPU::Extent3D physicalMiplevelSpecificTextureExtent(uint32_t mipLevel);

    id<MTLTexture> texture() const { return m_texture; }
    // A texture bound directly, rather than through a view, has no swizzle to apply.
    NODELETE id<MTLTexture> sampledTexture() const { return m_texture; }
    bool hasSwizzle() const { return false; }

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t depthOrArrayLayers() const { return m_depthOrArrayLayers; }
    uint32_t mipLevelCount() const { return m_mipLevelCount; }
    uint32_t sampleCount() const { return m_sampleCount; }
    NODELETE WebGPU::TextureDimension dimension() const;
    // Invalid textures may have no format. format() then reports an arbitrary one, which only the
    // validation that rejects the texture sees.
    NODELETE WebGPU::TextureFormat format() const;
    const std::optional<WebGPU::TextureFormat>& optionalFormat() const { return m_format; }
    OptionSet<WebGPU::TextureUsage> usage() const { return m_usage; }

    Device& device() const { return m_device; }

    NODELETE bool previouslyCleared() const;
    void setPreviouslyCleared();
    bool previouslyCleared(uint32_t mipLevel, uint32_t slice) const;
    void setPreviouslyCleared(uint32_t mipLevel, uint32_t slice, bool = true);
    void setProvisionallyCleared(uint32_t mipLevel, uint32_t slice, const CommandEncoder&);
    void setProvisionallyCleared(const CommandEncoder&);
    void promoteProvisionalClear(uint32_t mipLevel, uint32_t slice);
    void dropProvisionalClear(uint32_t mipLevel, uint32_t slice);
    void discardProvisionalClear(uint32_t mipLevel, uint32_t slice);
    bool isDestroyed() const { return m_destroyed; }

    static bool hasStorageBindingCapability(WebGPU::TextureFormat, const Device&, std::optional<WebGPU::StorageTextureAccess> = std::nullopt);
    static bool supportsMultisampling(WebGPU::TextureFormat, const Device&);
    static bool supportsResolve(WebGPU::TextureFormat, const Device&);
    static bool supportsBlending(WebGPU::TextureFormat, const Device&);
    void recreateIfNeeded();
    NODELETE void makeCanvasBacking();
    void setCommandEncoder(CommandEncoder&) const;
    void addResourceHolder(const BindGroup&) const;
    void addResourceHolder(const RenderBundle&) const;
    static ASCIILiteral formatToString(WebGPU::TextureFormat);
    bool isCanvasBacking() const { return m_canvasBacking; }

    bool waitForCommandBufferCompletion();
    void recordGPUExecutionWindow(double startTime, double endTime) const;
    Seconds gpuFrameCost() const;
    void resetGPUFrameCost() const;
    NODELETE void updateCompletionEvent(const std::pair<id<MTLSharedEvent>, uint64_t>&);
    NODELETE id<MTLSharedEvent> sharedEvent() const;
    NODELETE uint64_t sharedEventSignalValue() const;
    void setRasterizationRateMaps(std::pair<id<MTLRasterizationRateMap>, id<MTLRasterizationRateMap>>&& rateMaps) { m_leftRightRasterizationMaps = WTF::move(rateMaps); }
    id<MTLRasterizationRateMap> rasterizationMapForSlice(uint32_t slice) const { return slice ? m_leftRightRasterizationMaps.second : m_leftRightRasterizationMaps.first; }
    NODELETE uint32_t arrayLayerCount() const;
    WebGPU::TextureAspect aspect() const { return WebGPU::TextureAspect::All; }
    uint32_t baseArrayLayer() const { return 0; }
    uint32_t baseMipLevel() const { return 0; }
    uint32_t parentRelativeSlice() const { return 0; }
    bool is2DTexture() const { return m_dimension == WebGPU::TextureDimension::_2d; }
    bool is2DArrayTexture() const { return is2DTexture() && arrayLayerCount() > 1; }
    bool is3DTexture() const { return m_dimension == WebGPU::TextureDimension::_3d; }
    id<MTLTexture> parentTexture() const { return texture(); }
    const Texture& apiParentTexture() const { return *this; }

private:
    Texture(id<MTLTexture>, const WebGPU::TextureDescriptor&, Vector<WebGPU::TextureFormat>&& viewFormats, Device&);
    Texture(Device&);

    std::optional<ResolvedTextureViewDescriptor> resolveTextureViewDescriptorDefaults(const WebGPU::TextureViewDescriptor&) const;
    NSString* errorValidatingTextureViewCreation(const ResolvedTextureViewDescriptor&) const;

    bool durablyCleared(uint32_t mipLevel, uint32_t slice) const;

    id<MTLTexture> m_texture { nil };

    const uint32_t m_width { 0 };
    const uint32_t m_height { 0 };
    const uint32_t m_depthOrArrayLayers { 0 };
    const uint32_t m_mipLevelCount { 0 };
    const uint32_t m_sampleCount { 0 };
    const WebGPU::TextureDimension m_dimension { WebGPU::TextureDimension::_2d };
    const std::optional<WebGPU::TextureFormat> m_format; // std::nullopt only for invalid textures.
    const OptionSet<WebGPU::TextureUsage> m_usage;

    const Vector<WebGPU::TextureFormat> m_viewFormats;

    const Ref<Device> m_device;
    using ClearedToZeroInnerContainer = HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using ClearedToZeroContainer = HashMap<uint32_t, ClearedToZeroInnerContainer, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    ClearedToZeroContainer m_clearedToZero;
    HashCountedSet<uint64_t, DefaultHash<uint64_t>, WTF::UnsignedWithZeroKeyHashTraits<uint64_t>> m_provisionalClears;
    ThreadSafeWeakHashSet<TextureView> m_textureViews;
    mutable ThreadSafeWeakHashSet<BindGroup> m_bindGroups;
    mutable ThreadSafeWeakHashSet<RenderBundle> m_renderBundles;
    bool m_destroyed { false };
    bool m_canvasBacking { false };
    mutable Lock m_gpuFrameCostLock;
    mutable double m_gpuFrameCostSeconds WTF_GUARDED_BY_LOCK(m_gpuFrameCostLock) { 0 };
    id<MTLSharedEvent> m_sharedEvent { nil };
    std::pair<id<MTLRasterizationRateMap>, id<MTLRasterizationRateMap>> m_leftRightRasterizationMaps;

    uint64_t m_sharedEventSignalValue { 0 };
} SWIFT_SHARED_REFERENCE(refTexture, derefTexture) SWIFT_RETURNED_AS_UNRETAINED_BY_DEFAULT;

} // namespace WebGPU::Metal

inline void refTexture(WebGPU::Metal::Texture* obj)
{
    obj->ref();
}

inline void derefTexture(WebGPU::Metal::Texture* obj)
{
    obj->deref();
}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebGPU::Metal::Texture)
    static bool isType(const WGPUTextureImpl&) { return true; }
    static bool isType(const WebGPU::Texture&) { return true; }
SPECIALIZE_TYPE_TRAITS_END()
