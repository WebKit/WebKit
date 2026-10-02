/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#import "Texture.h"
#import "TextureView.h"

#import "WebGPUCppConversions.h"
#import <WebGPU/WebGPU.h>
#import <wtf/Ref.h>

namespace WebGPU::Metal {

class TextureOrTextureView {
public:
    TextureOrTextureView(Texture& texture)
        : m_texture(&texture)
    {
    }
    TextureOrTextureView(TextureView& view)
        : m_view(&view)
    {
    }
    TextureOrTextureView(Texture* texture)
        : m_texture(texture)
    {
    }
    TextureOrTextureView(TextureView* view)
        : m_view(view)
    {
    }

#define TEXTURE_OR_VIEW_INVOKE(x) return m_view ? protect(m_view)->x() : protect(m_texture)->x()
#define TEXTURE_OR_VIEW_HELPER(x) auto x() const { TEXTURE_OR_VIEW_INVOKE(x); }
#define TEXTURE_OR_VIEW_HELPER_NONCONST(x) auto x() { TEXTURE_OR_VIEW_INVOKE(x); }
#define TEXTURE_OR_VIEW_HELPER_REF(x) const auto& x() const { TEXTURE_OR_VIEW_INVOKE(x); }

    TEXTURE_OR_VIEW_HELPER(width)
    TEXTURE_OR_VIEW_HELPER(height)
    TEXTURE_OR_VIEW_HELPER(is2DTexture)
    TEXTURE_OR_VIEW_HELPER(is2DArrayTexture)
    TEXTURE_OR_VIEW_HELPER(is3DTexture)
    TEXTURE_OR_VIEW_HELPER(sampleCount)
    TEXTURE_OR_VIEW_HELPER(format)
    TEXTURE_OR_VIEW_HELPER(isDestroyed)
    TEXTURE_OR_VIEW_HELPER(depthOrArrayLayers)
    TEXTURE_OR_VIEW_HELPER(baseArrayLayer)
    TEXTURE_OR_VIEW_HELPER(baseMipLevel)
    TEXTURE_OR_VIEW_HELPER(parentTexture)
    TEXTURE_OR_VIEW_HELPER(parentRelativeSlice)
    TEXTURE_OR_VIEW_HELPER(previouslyCleared)
    TEXTURE_OR_VIEW_HELPER_NONCONST(setPreviouslyCleared)
    TEXTURE_OR_VIEW_HELPER(texture)
    TEXTURE_OR_VIEW_HELPER(isValid)
    TEXTURE_OR_VIEW_HELPER(usage)
    TEXTURE_OR_VIEW_HELPER(mipLevelCount)
    TEXTURE_OR_VIEW_HELPER(arrayLayerCount)

    TEXTURE_OR_VIEW_HELPER_REF(apiParentTexture)
    TEXTURE_OR_VIEW_HELPER_REF(device)

    void setCommandEncoder(CommandEncoder& encoder)
    {
        m_view ? protect(m_view)->setCommandEncoder(encoder) : protect(m_texture)->setCommandEncoder(encoder);
    }

    id<MTLRasterizationRateMap> rasterizationMapForSlice(uint32_t slice)
    {
        return m_view ? protect(m_view)->rasterizationMapForSlice(slice) : protect(m_texture)->rasterizationMapForSlice(slice);
    }

#undef TEXTURE_OR_VIEW_INVOKE
#undef TEXTURE_OR_VIEW_HELPER
#undef TEXTURE_OR_VIEW_HELPER_REF
#undef TEXTURE_OR_VIEW_HELPER_NONCONST

    operator bool() const { return m_texture || m_view; }

private:
    RefPtr<Texture> m_texture;
    RefPtr<TextureView> m_view;
};

bool isAllowableTextureView(const auto& texture, WGPULoadOp loadOp, WGPUStoreOp storeOp)
{
    // A view can narrow the usages it allows, but it cannot make a memoryless texture behave like a
    // regular one, so the transient rule follows the texture the view is a view of.
    if (texture.apiParentTexture().usage().contains(WebGPU::TextureUsage::Transient)) {
        if (loadOp != WGPULoadOp_Clear || storeOp != WGPUStoreOp_Discard)
            return false;
    }

    return true;
}

// A depth stencil attachment has a load and store op per aspect, and only the aspects present in
// its format may specify them, so the transient rule has to be applied per present aspect.
bool isAllowableDepthStencilTextureView(const auto& texture, bool hasDepthComponent, WGPULoadOp depthLoadOp, WGPUStoreOp depthStoreOp, bool hasStencilComponent, WGPULoadOp stencilLoadOp, WGPUStoreOp stencilStoreOp)
{
    if (hasDepthComponent && !isAllowableTextureView(texture, depthLoadOp, depthStoreOp))
        return false;

    return !hasStencilComponent || isAllowableTextureView(texture, stencilLoadOp, stencilStoreOp);
}

bool hasRenderableTextureViewProperties(const auto& texture)
{
    return texture.usage().contains(WebGPU::TextureUsage::RenderAttachment) && (texture.is2DTexture() || texture.is2DArrayTexture() || texture.is3DTexture()) && texture.mipLevelCount() == 1 && texture.arrayLayerCount() <= 1;
}

bool isRenderableTextureView(const auto& texture, WGPULoadOp loadOp, WGPUStoreOp storeOp)
{
    return isAllowableTextureView(texture, loadOp, storeOp) && hasRenderableTextureViewProperties(texture);
}

// The renderable properties of a depth stencil attachment only describe something while the view
// still has memory behind it, but the per-aspect transient rule applies to a destroyed view just the
// same. Kept as one entry point taking the view once so Swift does not have to pass it repeatedly.
bool isRenderableDepthStencilTextureView(const auto& texture, const Device& device, bool isDestroyed, bool hasDepthComponent, WGPULoadOp depthLoadOp, WGPUStoreOp depthStoreOp, bool hasStencilComponent, WGPULoadOp stencilLoadOp, WGPUStoreOp stencilStoreOp)
{
    if (!isDestroyed && (!Texture::isDepthStencilRenderableFormat(texture.format(), device) || !hasRenderableTextureViewProperties(texture)))
        return false;

    return isAllowableDepthStencilTextureView(texture, hasDepthComponent, depthLoadOp, depthStoreOp, hasStencilComponent, stencilLoadOp, stencilStoreOp);
}

// The texture or texture view of a render pass attachment.
inline TextureOrTextureView textureOrTextureView(const WebGPU::RenderPassAttachmentView& view)
{
    return std::visit(WTF::makeVisitor([](const Ref<WebGPU::TextureView>& textureView) {
        return TextureOrTextureView(static_cast<TextureView&>(textureView.get()));
    }, [](const Ref<WebGPU::Texture>& texture) {
        return TextureOrTextureView(static_cast<Texture&>(texture.get()));
    }), view);
}

// A WebGPU::RenderPassColorAttachment with its views resolved and its operations as the C API
// values that the render pass creation compares. Swift cannot get the alternatives of the variant
// members of the C++ API struct, so the C++ and the Swift render pass creation both read this.
struct ResolvedRenderPassColorAttachment {
    TextureOrTextureView view;
    std::optional<TextureOrTextureView> resolveTarget;
    std::optional<uint32_t> depthSlice;
    WebGPU::Color clearValue;
    WGPULoadOp loadOp { WGPULoadOp_Undefined };
    WGPUStoreOp storeOp { WGPUStoreOp_Undefined };
};

// A WebGPU::RenderPassDepthStencilAttachment, as ResolvedRenderPassColorAttachment. The operations
// are WGPULoadOp_Undefined and WGPUStoreOp_Undefined when they are not given.
struct ResolvedRenderPassDepthStencilAttachment {
    TextureOrTextureView view;
    float depthClearValue { 0 };
    WGPULoadOp depthLoadOp { WGPULoadOp_Undefined };
    WGPUStoreOp depthStoreOp { WGPUStoreOp_Undefined };
    bool depthReadOnly { false };
    uint32_t stencilClearValue { 0 };
    WGPULoadOp stencilLoadOp { WGPULoadOp_Undefined };
    WGPUStoreOp stencilStoreOp { WGPUStoreOp_Undefined };
    bool stencilReadOnly { false };
};

// std::nullopt for an empty color attachment slot.
inline std::optional<ResolvedRenderPassColorAttachment> resolvedColorAttachment(const WebGPU::RenderPassDescriptor& descriptor, size_t index)
{
    auto& attachment = descriptor.colorAttachments[index];
    if (!attachment)
        return std::nullopt;
    return ResolvedRenderPassColorAttachment {
        .view = textureOrTextureView(attachment->view),
        .resolveTarget = attachment->resolveTarget ? std::optional { textureOrTextureView(*attachment->resolveTarget) } : std::nullopt,
        .depthSlice = attachment->depthSlice,
        .clearValue = attachment->clearValue,
        .loadOp = toAPI(attachment->loadOp),
        .storeOp = toAPI(attachment->storeOp),
    };
}

inline std::optional<ResolvedRenderPassDepthStencilAttachment> resolvedDepthStencilAttachment(const WebGPU::RenderPassDescriptor& descriptor)
{
    auto& attachment = descriptor.depthStencilAttachment;
    if (!attachment)
        return std::nullopt;
    return ResolvedRenderPassDepthStencilAttachment {
        .view = textureOrTextureView(attachment->view),
        .depthClearValue = attachment->depthClearValue,
        .depthLoadOp = toAPI(attachment->depthLoadOp),
        .depthStoreOp = toAPI(attachment->depthStoreOp),
        .depthReadOnly = attachment->depthReadOnly,
        .stencilClearValue = attachment->stencilClearValue,
        .stencilLoadOp = toAPI(attachment->stencilLoadOp),
        .stencilStoreOp = toAPI(attachment->stencilStoreOp),
        .stencilReadOnly = attachment->stencilReadOnly,
    };
}

}
