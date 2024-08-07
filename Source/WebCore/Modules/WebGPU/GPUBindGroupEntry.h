/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "GPUBufferBinding.h"
#include "GPUExternalTexture.h"
#include "GPUIntegralTypes.h"
#include "GPUSampler.h"
#include "GPUTextureView.h"
#include "WebGPUBindGroupEntry.h"
#include <utility>
#include <variant>

namespace WebCore {

using GPUBindingResource = std::variant<RefPtr<GPUSampler>, RefPtr<GPUTextureView>, GPUBufferBinding, RefPtr<GPUExternalTexture>>;

inline WebGPU::BindingResource convertToBacking(const GPUBindingResource& bindingResource)
{
    return WTF::switchOn(bindingResource, [](const RefPtr<GPUSampler>& sampler) -> WebGPU::BindingResource {
        ASSERT(sampler);
        return sampler->backing();
    }, [](const RefPtr<GPUTextureView>& textureView) -> WebGPU::BindingResource {
        ASSERT(textureView);
        return textureView->backing();
    }, [](const GPUBufferBinding& bufferBinding) -> WebGPU::BindingResource {
        return bufferBinding.convertToBacking();
    }, [](const RefPtr<GPUExternalTexture>& externalTexture) -> WebGPU::BindingResource {
        ASSERT(externalTexture);
        return externalTexture->backing();
    });
}

struct GPUBindGroupEntry {
    WebGPU::BindGroupEntry convertToBacking() const
    {
        return {
            binding,
            WebCore::convertToBacking(resource),
        };
    }

    static bool equal(const GPUSampler& entry, const GPUBindingResource& otherEntry)
    {
        return WTF::switchOn(otherEntry, [&](const RefPtr<GPUSampler>& sampler) -> bool {
            return sampler.get() == &entry;
        }, [&](const RefPtr<GPUTextureView>&) -> bool {
            return false;
        }, [&](const GPUBufferBinding&) -> bool {
            return false;
        }, [&](const RefPtr<GPUExternalTexture>&) -> bool {
            return false;
        });
    }
    static bool equal(const GPUTextureView& entry, const GPUBindingResource& otherEntry)
    {
        return WTF::switchOn(otherEntry, [&](const RefPtr<GPUSampler>&) -> bool {
            return false;
        }, [&](const RefPtr<GPUTextureView>& textureView) -> bool {
            return textureView.get() == &entry;
        }, [&](const GPUBufferBinding&) -> bool {
            return false;
        }, [&](const RefPtr<GPUExternalTexture>&) -> bool {
            return false;
        });
    }
    static bool equal(const GPUBufferBinding& entry, const GPUBindingResource& otherEntry)
    {
        return WTF::switchOn(otherEntry, [&](const RefPtr<GPUSampler>&) -> bool {
            return false;
        }, [&](const RefPtr<GPUTextureView>&) -> bool {
            return false;
        }, [&](const GPUBufferBinding& bufferBinding) -> bool {
            return bufferBinding.buffer.get() == entry.buffer.get();
        }, [&](const RefPtr<GPUExternalTexture>&) -> bool {
            return false;
        });
    }
    static bool equal(const GPUExternalTexture& entry, const GPUBindingResource& otherEntry)
    {
        return WTF::switchOn(otherEntry, [&](const RefPtr<GPUSampler>&) -> bool {
            return false;
        }, [&](const RefPtr<GPUTextureView>&) -> bool {
            return false;
        }, [&](const GPUBufferBinding&) -> bool {
            return false;
        }, [&](const RefPtr<GPUExternalTexture>& externalTexture) -> bool {
            return externalTexture.get() == &entry;
        });
    }
    static bool equal(const GPUBindGroupEntry& entry, const GPUBindGroupEntry& otherEntry)
    {
        return WTF::switchOn(entry.resource, [&](const RefPtr<GPUSampler>& sampler) -> bool {
            return sampler.get() && equal(*sampler, otherEntry.resource);
        }, [&](const RefPtr<GPUTextureView>& textureView) -> bool {
            return textureView.get() && equal(*textureView, otherEntry.resource);
        }, [&](const GPUBufferBinding& bufferBinding) -> bool {
            return equal(bufferBinding, otherEntry.resource);
        }, [&](const RefPtr<GPUExternalTexture>& externalTexture) -> bool {
            return externalTexture.get() && equal(*externalTexture, otherEntry.resource);
        });
    }

    const RefPtr<GPUExternalTexture>* externalTexture() const
    {
        return std::get_if<RefPtr<GPUExternalTexture>>(&resource);
    }

    GPUIndex32 binding { 0 };
    GPUBindingResource resource;
};

}
