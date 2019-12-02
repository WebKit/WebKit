//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SamplerVk.cpp:
//    Implements the class methods for SamplerVk.
//

#include "libANGLE/renderer/vulkan/SamplerVk.h"

#include "common/debug.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/vk_utils.h"

namespace rx
{

SamplerVk::SamplerVk(const gl::SamplerState &state) : SamplerImpl(state), mSerial{} {}

SamplerVk::~SamplerVk() = default;

void SamplerVk::onDestroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);
    mSampler.release(contextVk->getRenderer());
}

angle::Result SamplerVk::syncState(const gl::Context *context, const bool dirty)
{
    ContextVk *contextVk = vk::GetImpl(context);

    RendererVk *renderer = contextVk->getRenderer();
    if (mSampler.valid())
    {
        if (!dirty)
        {
            return angle::Result::Continue;
        }
        mSampler.release(renderer);
    }

    const gl::Extensions &extensions = renderer->getNativeExtensions();
    float maxAnisotropy              = mState.getMaxAnisotropy();
    bool anisotropyEnable            = extensions.textureFilterAnisotropic && maxAnisotropy > 1.0f;

    VkSamplerCreateInfo samplerInfo     = {};
    samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.flags                   = 0;
    samplerInfo.magFilter               = gl_vk::GetFilter(mState.getMagFilter());
    samplerInfo.minFilter               = gl_vk::GetFilter(mState.getMinFilter());
    samplerInfo.mipmapMode              = gl_vk::GetSamplerMipmapMode(mState.getMinFilter());
    samplerInfo.addressModeU            = gl_vk::GetSamplerAddressMode(mState.getWrapS());
    samplerInfo.addressModeV            = gl_vk::GetSamplerAddressMode(mState.getWrapT());
    samplerInfo.addressModeW            = gl_vk::GetSamplerAddressMode(mState.getWrapR());
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.anisotropyEnable        = anisotropyEnable;
    samplerInfo.maxAnisotropy           = maxAnisotropy;
    samplerInfo.compareEnable           = mState.getCompareMode() == GL_COMPARE_REF_TO_TEXTURE;
    samplerInfo.compareOp               = gl_vk::GetCompareOp(mState.getCompareFunc());
    samplerInfo.minLod                  = mState.getMinLod();
    samplerInfo.maxLod                  = mState.getMaxLod();
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    if (!gl::IsMipmapFiltered(mState))
    {
        // Per the Vulkan spec, GL_NEAREST and GL_LINEAR do not map directly to Vulkan, so
        // they must be emulated (See "Mapping of OpenGL to Vulkan filter modes")
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.minLod     = 0.0f;
        samplerInfo.maxLod     = 0.25f;
    }

    ANGLE_VK_TRY(contextVk, mSampler.get().init(contextVk->getDevice(), samplerInfo));
    // Regenerate the serial on a sampler change.
    mSerial = contextVk->generateTextureSerial();
    return angle::Result::Continue;
}

}  // namespace rx
