/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DawnCaps_DEFINED
#define skgpu_graphite_DawnCaps_DEFINED

#include "src/gpu/graphite/Caps.h"

#include <array>

#include "webgpu/webgpu_cpp.h"  // NO_G3_REWRITE

namespace skgpu::graphite {
struct ContextOptions;

struct DawnBackendContext;

class DawnCaps final : public Caps {
public:
    DawnCaps(const DawnBackendContext&, const ContextOptions&);
    ~DawnCaps() override;

    bool useAsyncPipelineCreation() const { return fUseAsyncPipelineCreation; }
    bool allowScopedErrorChecks() const { return fAllowScopedErrorChecks; }

    // If this has no value then loading the resolve texture via a LoadOp is not supported.
    std::optional<wgpu::LoadOp> resolveTextureLoadOp() const {
        return fSupportedResolveTextureLoadOp;
    }
    bool supportsPartialLoadResolve() const { return fSupportsPartialLoadResolve; }

    TextureInfo getDefaultSampledTextureInfo(SkColorType,
                                             Mipmapped mipmapped,
                                             Protected,
                                             Renderable) const override;
    TextureInfo getTextureInfoForSampledCopy(const TextureInfo& textureInfo,
                                             Mipmapped mipmapped) const override;
    TextureInfo getDefaultCompressedTextureInfo(SkTextureCompressionType,
                                                Mipmapped mipmapped,
                                                Protected) const override;
    TextureInfo getDefaultMSAATextureInfo(const TextureInfo& singleSampledInfo,
                                          Discardable discardable) const override;
    TextureInfo getDefaultDepthStencilTextureInfo(SkEnumBitMask<DepthStencilFlags>,
                                                  uint32_t sampleCount,
                                                  Protected) const override;
    TextureInfo getDefaultStorageTextureInfo(SkColorType) const override;
    SkISize getDepthAttachmentDimensions(const TextureInfo&,
                                         const SkISize colorAttachmentDimensions) const override;
    UniqueKey makeGraphicsPipelineKey(const GraphicsPipelineDesc&,
                                      const RenderPassDesc&) const override;
    UniqueKey makeComputePipelineKey(const ComputePipelineDesc&) const override;
    ImmutableSamplerInfo getImmutableSamplerInfo(const TextureProxy* proxy) const override;
    GraphiteResourceKey makeSamplerKey(const SamplerDesc&) const override;
    uint32_t channelMask(const TextureInfo&) const override;
    bool isRenderable(const TextureInfo&) const override;
    bool isStorage(const TextureInfo&) const override;
    void buildKeyForTexture(SkISize dimensions,
                            const TextureInfo&,
                            ResourceType,
                            Shareable,
                            GraphiteResourceKey*) const override;
    uint32_t getRenderPassDescKeyForPipeline(const RenderPassDesc& renderPassDesc) const;

private:
    const ColorTypeInfo* getColorTypeInfo(SkColorType, const TextureInfo&) const override;
    bool onIsTexturable(const TextureInfo&) const override;
    bool supportsWritePixels(const TextureInfo& textureInfo) const override;
    bool supportsReadPixels(const TextureInfo& textureInfo) const override;
    std::pair<SkColorType, bool /*isRGBFormat*/> supportedWritePixelsColorType(
            SkColorType dstColorType,
            const TextureInfo& dstTextureInfo,
            SkColorType srcColorType) const override;
    std::pair<SkColorType, bool /*isRGBFormat*/> supportedReadPixelsColorType(
            SkColorType srcColorType,
            const TextureInfo& srcTextureInfo,
            SkColorType dstColorType) const override;

    void initCaps(const DawnBackendContext& backendContext, const ContextOptions& options);
    void initShaderCaps(const wgpu::Device& device);
    void initFormatTable(const wgpu::Device& device);

    wgpu::TextureFormat getFormatFromColorType(SkColorType colorType) const {
        int idx = static_cast<int>(colorType);
        return fColorTypeToFormatTable[idx];
    }

    uint32_t maxRenderTargetSampleCount(wgpu::TextureFormat format) const;
    bool isTexturable(wgpu::TextureFormat format) const;
    bool isRenderable(wgpu::TextureFormat format, uint32_t numSamples) const;

    struct FormatInfo {
        uint32_t colorTypeFlags(SkColorType colorType) const {
            for (int i = 0; i < fColorTypeInfoCount; ++i) {
                if (fColorTypeInfos[i].fColorType == colorType) {
                    return fColorTypeInfos[i].fFlags;
                }
            }
            return 0;
        }

        enum {
            kTexturable_Flag  = 0x01,
            kRenderable_Flag  = 0x02, // Color attachment and blendable
            kMSAA_Flag        = 0x04,
            kResolve_Flag     = 0x08,
            kStorage_Flag     = 0x10,
        };
        static const uint16_t kAllFlags =
                kTexturable_Flag | kRenderable_Flag | kMSAA_Flag | kResolve_Flag | kStorage_Flag;

        uint16_t fFlags = 0;

        std::unique_ptr<ColorTypeInfo[]> fColorTypeInfos;
        int fColorTypeInfoCount = 0;
    };
    // Size here must be at least the size of kFormats in DawnCaps.cpp.
    static constexpr size_t kFormatCount = 17;
    std::array<FormatInfo, kFormatCount> fFormatTable;

    static size_t GetFormatIndex(wgpu::TextureFormat format);
    const FormatInfo& getFormatInfo(wgpu::TextureFormat format) const {
        size_t index = GetFormatIndex(format);
        return fFormatTable[index];
    }

    wgpu::TextureFormat fColorTypeToFormatTable[kSkColorTypeCnt];
    void setColorType(SkColorType, std::initializer_list<wgpu::TextureFormat> formats);

    // When supported, this value will hold the TransientAttachment usage symbol that is only
    // defined in Dawn native builds and not EMSCRIPTEN but this avoids having to #define guard it.
    wgpu::TextureUsage fSupportedTransientAttachmentUsage = wgpu::TextureUsage::None;
    // When supported this holds the ExpandResolveTexture load op, otherwise holds no value.
    std::optional<wgpu::LoadOp> fSupportedResolveTextureLoadOp;
    // When 'fSupportedResolveTextureLoadOp' is supported, it by default performs full size expand
    // and resolve. With this feature, we can do that partially according to the actual damage
    // region.
    bool fSupportsPartialLoadResolve = false;

    bool fUseAsyncPipelineCreation = true;
    bool fAllowScopedErrorChecks = true;
};

} // namespace skgpu::graphite

#endif // skgpu_graphite_DawnCaps_DEFINED
