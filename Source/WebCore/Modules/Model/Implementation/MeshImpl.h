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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUPtr.h"
#include <WebCore/Mesh.h>
#include <WebCore/Model.h>
#include <WebCore/WebGPUPredefinedColorSpace.h>
#include <WebGPU/WebGPU.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
class IOSurface;
}

namespace WebCore {

class ModelConvertToBackingContext;

class MeshImpl final : public Mesh {
    WTF_MAKE_TZONE_ALLOCATED(MeshImpl);
public:
    static Ref<MeshImpl> create(WebGPU::WebGPUPtr<WebMesh>&& mesh, Vector<UniqueRef<WebCore::IOSurface>>&& renderBuffers, ModelConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new MeshImpl(WTF::move(mesh), WTF::move(renderBuffers), convertToBackingContext));
    }

    virtual ~MeshImpl();

    WebMesh backing() const { return m_backing.get(); };
#if PLATFORM(COCOA)
    Vector<MachSendRight> ioSurfaceHandles() final;
#endif

private:
    friend class ModelDowncastConvertToBackingContext;

    MeshImpl(WebGPU::WebGPUPtr<WebMesh>&&, Vector<UniqueRef<WebCore::IOSurface>>&&, ModelConvertToBackingContext&);

    MeshImpl(const MeshImpl&) = delete;
    MeshImpl(MeshImpl&&) = delete;
    MeshImpl& operator=(const MeshImpl&) = delete;
    MeshImpl& operator=(MeshImpl&&) = delete;

    bool isMeshImpl() const final { return true; }

    void setLabelInternal(const String&) final;
    void update(const WebModel::UpdateMeshDescriptor&) final;
    void updateTexture(const WebModel::UpdateTextureDescriptor&) final;
    void updateMaterial(const WebModel::UpdateMaterialDescriptor&) final;
    void setEntityTransform(const WebModel::Float4x4&) final;
#if PLATFORM(COCOA)
    std::optional<WebModel::Float4x4> entityTransform() const final;
#endif
    void setCameraDistance(float) final;
    void play(bool) final;
    void setEnvironmentMap(const WebModel::ImageAsset&) final;

    void render() final;

    const Ref<ModelConvertToBackingContext> m_convertToBackingContext;

    WebGPU::WebGPUPtr<WebMesh> m_backing;
#if PLATFORM(COCOA)
    Vector<UniqueRef<WebCore::IOSurface>> m_renderBuffers;
#endif
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::MeshImpl)
    static bool isType(const WebCore::Mesh& mesh) { return mesh.isMeshImpl(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // HAVE(WEBGPU_IMPLEMENTATION)
