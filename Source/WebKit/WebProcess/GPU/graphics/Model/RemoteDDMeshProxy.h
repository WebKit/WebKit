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

#include "DDModelIdentifier.h"
#include "RemoteDeviceProxy.h"
#include <WebCore/DDFloat4x4.h>
#include <WebCore/DDMesh.h>
#include <WebCore/DDMeshDescriptor.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(COCOA)
#include <simd/simd.h>
#endif

namespace WebKit::DDModel {

class ConvertToBackingContext;

class RemoteDDMeshProxy final : public WebCore::DDModel::DDMesh {
    WTF_MAKE_TZONE_ALLOCATED(RemoteDDMeshProxy);
public:
    static Ref<RemoteDDMeshProxy> create(Ref<RemoteGPUProxy>&& root, ConvertToBackingContext& convertToBackingContext, DDModelIdentifier identifier)
    {
        return adoptRef(*new RemoteDDMeshProxy(WTFMove(root), convertToBackingContext, identifier));
    }

    virtual ~RemoteDDMeshProxy();

    RemoteGPUProxy& root() const { return m_root; }

private:
    friend class DowncastConvertToBackingContext;

    RemoteDDMeshProxy(Ref<RemoteGPUProxy>&&, ConvertToBackingContext&, DDModelIdentifier);

    RemoteDDMeshProxy(const RemoteDDMeshProxy&) = delete;
    RemoteDDMeshProxy(RemoteDDMeshProxy&&) = delete;
    RemoteDDMeshProxy& operator=(const RemoteDDMeshProxy&) = delete;
    RemoteDDMeshProxy& operator=(RemoteDDMeshProxy&&) = delete;

    bool isRemoteDDMeshProxy() const final { return true; }

    DDModelIdentifier backing() const { return m_backing; }

    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().protectedStreamClientConnection()->send(WTFMove(message), backing());
    }

    void update(const WebCore::DDModel::DDUpdateMeshDescriptor&) final;
    void updateTexture(const WebCore::DDModel::DDUpdateTextureDescriptor&) final;
    void updateMaterial(const WebCore::DDModel::DDUpdateMaterialDescriptor&) final;
#if PLATFORM(COCOA)
    std::pair<simd_float4, simd_float4> getCenterAndExtents() const final;
#endif
    void play(bool) final;

    void render() final;
    void setLabelInternal(const String&) final;
    void setEntityTransform(const WebCore::DDModel::DDFloat4x4&) final;
    std::optional<WebCore::DDModel::DDFloat4x4> entityTransform() const final;
    bool supportsTransform(const WebCore::TransformationMatrix&) const final;
    void setScale(float) final;
    void setCameraDistance(float) final;
    void setStageMode(WebCore::StageModeOperation) final;
#if ENABLE(GPU_PROCESS_MODEL)
    void setRotation(float yaw, float pitch, float roll) final;
#endif

    const DDModelIdentifier m_backing;
    const Ref<ConvertToBackingContext> m_convertToBackingContext;
    const Ref<RemoteGPUProxy> m_root;
#if PLATFORM(COCOA)
    simd_float4 m_minCorner;
    simd_float4 m_maxCorner;
#endif
    std::optional<WebCore::DDModel::DDFloat4x4> m_transform;
#if ENABLE(GPU_PROCESS_MODEL)
    float m_cameraDistance { 1.f };
    WebCore::StageModeOperation m_stageMode;
#endif
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::DDModel::RemoteDDMeshProxy)
    static bool isType(const WebCore::DDModel::DDMesh& mesh) { return mesh.isRemoteDDMeshProxy(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(GPU_PROCESS)
