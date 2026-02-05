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

#include "config.h"
#include "MeshImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "Model.h"
#include "ModelConvertToBackingContext.h"
#include "WebGPUImpl.h"
#include <WebGPU/ModelTypes.h>
#include <WebGPU/WebGPUExt.h>
#include <wtf/TZoneMallocInlines.h>
#include <WebCore/IOSurface.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MeshImpl);

MeshImpl::MeshImpl(WebGPU::WebGPUPtr<WebMesh>&& mesh, Vector<UniqueRef<WebCore::IOSurface>>&& renderBuffers, ModelConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
    , m_backing(WTF::move(mesh))
    , m_renderBuffers(WTF::move(renderBuffers))
{
}

MeshImpl::~MeshImpl() = default;

void MeshImpl::setLabelInternal(const String&)
{
    // FIXME: Implement this.
}

void MeshImpl::update(const WebModel::UpdateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshUpdate(m_backing.get(), descriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void MeshImpl::updateTexture(const WebModel::UpdateTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshTextureUpdate(m_backing.get(), descriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void MeshImpl::updateMaterial(const WebModel::UpdateMaterialDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshMaterialUpdate(m_backing.get(), descriptor);
#else
    UNUSED_PARAM(descriptor);
#endif
}

void MeshImpl::render()
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshRender(m_backing.get());
#endif
}

void MeshImpl::setEntityTransform(const WebModel::Float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshSetTransform(m_backing.get(), transform);
#else
    UNUSED_PARAM(transform);
#endif
}

#if PLATFORM(COCOA)
std::optional<WebModel::Float4x4> MeshImpl::entityTransform() const
{
    return std::nullopt;
}
#endif

void MeshImpl::setCameraDistance(float distance)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshSetCameraDistance(m_backing.get(), distance);
#else
    UNUSED_PARAM(distance);
#endif
}

void MeshImpl::play(bool play)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshPlay(m_backing.get(), play);
#else
    UNUSED_PARAM(play);
#endif
}

void MeshImpl::setEnvironmentMap(const WebModel::ImageAsset& imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    webModelMeshSetEnvironmentMap(m_backing.get(), imageAsset);
#else
    UNUSED_PARAM(imageAsset);
#endif
}

#if PLATFORM(COCOA)
Vector<MachSendRight> MeshImpl::ioSurfaceHandles()
{
    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}
#endif

}

namespace WebCore::WebGPU {

#if ENABLE(GPU_PROCESS_MODEL)
static Vector<UniqueRef<WebCore::IOSurface>> createIOSurfaces(unsigned width, unsigned height)
{
    const auto colorFormat = IOSurface::Format::BGRA;
    const auto colorSpace = DestinationColorSpace::LinearDisplayP3();

    Vector<UniqueRef<WebCore::IOSurface>> ioSurfaces;

    constexpr auto surfaceCount = 3;
    for (auto i = 0; i < surfaceCount; ++i) {
        if (auto buffer = WebCore::IOSurface::create(nullptr, WebCore::IntSize(width, height), colorSpace, IOSurface::Name::WebGPU, colorFormat))
            ioSurfaces.append(makeUniqueRefFromNonNullUniquePtr(WTF::move(buffer)));
    }

    return ioSurfaces;
}
#endif

RefPtr<WebCore::Mesh> GPUImpl::createModelBacking(unsigned width, unsigned height, const WebModel::ImageAsset& diffuseTexture, const WebModel::ImageAsset& specularTexture, CompletionHandler<void(Vector<MachSendRight>&&)>&& callback)
{
#if ENABLE(GPU_PROCESS_MODEL)
    auto ioSurfaceVector = createIOSurfaces(width, height);
    Vector<RetainPtr<IOSurfaceRef>> ioSurfaces;
    for (UniqueRef<WebCore::IOSurface>& ioSurface : ioSurfaceVector)
        ioSurfaces.append(ioSurface->surface());

    WebModelCreateMeshDescriptor backingDescriptor {
        .width = width,
        .height = height,
        .ioSurfaces = WTF::move(ioSurfaces),
        .diffuseTexture = diffuseTexture,
        .specularTexture = specularTexture,
    };

    Ref convertToBackingContext = m_modelConvertToBackingContext;
    auto mesh = WebCore::MeshImpl::create(adoptWebGPU(webModelMeshCreate(m_backing.get(), &backingDescriptor)), WTF::move(ioSurfaceVector), convertToBackingContext);
    callback(mesh->ioSurfaceHandles());
    return mesh;
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(diffuseTexture);
    UNUSED_PARAM(specularTexture);
    UNUSED_PARAM(callback);

    RELEASE_ASSERT_NOT_REACHED("createModelBacking should not be called when GPU_PROCESS_MODEL is 0");
#endif
}

}
#endif // HAVE(WEBGPU_IMPLEMENTATION)
