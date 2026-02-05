/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "RemoteDeviceProxy.h"
#include "WebModelIdentifier.h"
#include <WebCore/Mesh.h>
#include <WebCore/WebModel.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(COCOA)
#include <WebGPU/Float4x4.h>
#include <simd/simd.h>
#endif

namespace WebModel {
struct Float4x4;
}

namespace WebKit {

class ModelConvertToBackingContext;

class RemoteMeshProxy final : public WebCore::Mesh {
    WTF_MAKE_TZONE_ALLOCATED(RemoteMeshProxy);
public:
    static Ref<RemoteMeshProxy> create(Ref<RemoteGPUProxy>&& root, ModelConvertToBackingContext& convertToBackingContext, WebModelIdentifier identifier)
    {
        return adoptRef(*new RemoteMeshProxy(WTF::move(root), convertToBackingContext, identifier));
    }

    virtual ~RemoteMeshProxy();

    RemoteGPUProxy& root() const { return m_root; }

private:
    friend class ModelDowncastConvertToBackingContext;

    RemoteMeshProxy(Ref<RemoteGPUProxy>&&, ModelConvertToBackingContext&, WebModelIdentifier);

    RemoteMeshProxy(const RemoteMeshProxy&) = delete;
    RemoteMeshProxy(RemoteMeshProxy&&) = delete;
    RemoteMeshProxy& operator=(const RemoteMeshProxy&) = delete;
    RemoteMeshProxy& operator=(RemoteMeshProxy&&) = delete;

    bool isRemoteMeshProxy() const final { return true; }

    WebModelIdentifier backing() const { return m_backing; }

    template<typename T>
    [[nodiscard]] IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTF::move(message), backing());
    }

    void update(const WebModel::UpdateMeshDescriptor&) final;
    void updateTexture(const WebModel::UpdateTextureDescriptor&) final;
    void updateMaterial(const WebModel::UpdateMaterialDescriptor&) final;
#if PLATFORM(COCOA)
    std::pair<simd_float4, simd_float4> getCenterAndExtents() const final;
#endif
    void play(bool) final;

    void render() final;
    void setLabelInternal(const String&) final;
    void setEntityTransform(const WebModel::Float4x4&) final;
#if PLATFORM(COCOA)
    std::optional<WebModel::Float4x4> entityTransform() const final;
#endif
    bool supportsTransform(const WebCore::TransformationMatrix&) const final;
    void setScale(float) final;
    void setCameraDistance(float) final;
    void setStageMode(WebCore::StageModeOperation) final;
#if ENABLE(GPU_PROCESS_MODEL)
    void setRotation(float yaw, float pitch, float roll) final;
#endif
    void setEnvironmentMap(const WebModel::ImageAsset&) final;

    const WebModelIdentifier m_backing;
    const Ref<ModelConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;
#if PLATFORM(COCOA)
    simd_float4 m_minCorner;
    simd_float4 m_maxCorner;
    std::optional<WebModel::Float4x4> m_transform;
#endif
#if ENABLE(GPU_PROCESS_MODEL)
    float m_cameraDistance { 1.f };
    WebCore::StageModeOperation m_stageMode;
#endif
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::RemoteMeshProxy)
    static bool isType(const WebCore::Mesh& mesh) { return mesh.isRemoteMeshProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
