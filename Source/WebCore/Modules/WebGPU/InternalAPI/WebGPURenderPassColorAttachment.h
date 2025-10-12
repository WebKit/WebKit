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

#include <WebCore/WebGPUColor.h>
#include <WebCore/WebGPUIntegralTypes.h>
#include <WebCore/WebGPULoadOp.h>
#include <WebCore/WebGPUStoreOp.h>
#include <WebCore/WebGPUTexture.h>
#include <WebCore/WebGPUTextureView.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/WeakRef.h>

namespace WebCore::WebGPU {

class Texture;
class TextureView;

using RenderPassColorAttachmentView = Variant<const WeakRef<Texture>, const WeakRef<TextureView>>;
using RenderPassResolveAttachmentView = Variant<WeakPtr<Texture>, WeakPtr<TextureView>>;

struct RenderPassColorAttachment {
    RenderPassColorAttachmentView view;
    std::optional<IntegerCoordinate> depthSlice;
    std::optional<RenderPassResolveAttachmentView> resolveTarget;

    std::optional<Color> clearValue;
    LoadOp loadOp { LoadOp::Load };
    StoreOp storeOp { StoreOp::Store };

    const RefPtr<Texture> protectedTexture() const
    {
        return WTF::switchOn(view, [&](const WeakRef<Texture>& texture) -> const RefPtr<Texture> {
            return texture.ptr();
        }, [&](const WeakRef<TextureView>&) -> const RefPtr<Texture> {
            return nullptr;
        });
    }
    const RefPtr<TextureView> protectedView() const
    {
        return WTF::switchOn(view, [&](const WeakRef<Texture>&) -> const RefPtr<TextureView> {
            return nullptr;
        }, [&](const WeakRef<TextureView>& view) -> const RefPtr<TextureView> {
            return view.ptr();
        });
    }
    RefPtr<Texture> protectedResolveTexture() const
    {
        if (!resolveTarget)
            return nullptr;

        return WTF::switchOn(*resolveTarget, [&](const WeakPtr<Texture>& texture) -> const RefPtr<Texture> {
            return texture.get();
        }, [&](const WeakPtr<TextureView>&) -> const RefPtr<Texture> {
            return nullptr;
        });
    }
    RefPtr<TextureView> protectedResolveTarget() const
    {
        if (!resolveTarget)
            return nullptr;

        return WTF::switchOn(*resolveTarget, [&](const WeakPtr<Texture>&) -> const RefPtr<TextureView> {
            return nullptr;
        }, [&](const WeakPtr<TextureView>& view) -> const RefPtr<TextureView> {
            return view.get();
        });
    }
};

} // namespace WebCore::WebGPU
