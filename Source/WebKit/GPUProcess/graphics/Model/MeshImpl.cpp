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

#if ENABLE(GPU_PROCESS_MODEL)

#include "ModelTypes.h"
#include "WebKitMesh.h"
#include <WebCore/IOSurface.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MeshImpl);

MeshImpl::MeshImpl(Ref<WebMesh>&& mesh, Vector<UniqueRef<WebCore::IOSurface>>&& renderBuffers)
    : m_backing(WTF::move(mesh))
    , m_renderBuffers(WTF::move(renderBuffers))
{
}

MeshImpl::~MeshImpl() = default;

void MeshImpl::setLabelInternal(const String&)
{
    // FIXME: Implement this.
}

void MeshImpl::update(Vector<WebModel::UpdateMeshDescriptor>&& descriptor)
{
    m_backing->update(WTF::move(descriptor));
}

void MeshImpl::updateTexture(Vector<WebModel::UpdateTextureDescriptor>&& descriptor)
{
    m_backing->updateTexture(WTF::move(descriptor));
}

void MeshImpl::updateMaterial(Vector<WebModel::UpdateMaterialDescriptor>&& descriptor)
{
    m_backing->updateMaterial(WTF::move(descriptor));
}

void MeshImpl::render(uint32_t textureIndex, Function<void(bool)>&& completionHandler)
{
    m_backing->render(textureIndex, WTF::move(completionHandler));
}

void MeshImpl::setEntityTransform(const WebModel::Float4x4& transform)
{
    m_backing->setTransform(transform);
}

#if PLATFORM(COCOA)
std::optional<WebModel::Float4x4> MeshImpl::entityTransform() const
{
    return std::nullopt;
}
#endif

void MeshImpl::setFOV(float fovY)
{
    m_backing->setFOV(fovY);
}

void MeshImpl::setBackgroundColor(const WebModel::Float3& color)
{
    m_backing->setBackgroundColor(color);
}

void MeshImpl::play(bool play)
{
    m_backing->play(play);
}

void MeshImpl::setEnvironmentMap(const WebModel::UpdateTextureDescriptor& imageAsset)
{
    m_backing->setEnvironmentMap(imageAsset);
}

void MeshImpl::updateContentsHeadroom(float headroom)
{
#if HAVE(SUPPORT_HDR_DISPLAY) && PLATFORM(COCOA)
    for (auto& renderBuffer : m_renderBuffers)
        renderBuffer->setContentEDRHeadroom(headroom);
#else
    UNUSED_PARAM(headroom);
#endif
}

#if PLATFORM(COCOA)
Vector<MachSendRight> MeshImpl::ioSurfaceHandles()
{
    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}

void MeshImpl::updateRenderBuffers(WebModel::ResizeMeshDescriptor&& descriptor)
{
    m_backing->updateRenderBuffers(descriptor);
    m_renderBuffers = WTF::move(descriptor.renderBuffers);
}
#endif

void MeshImpl::processRemovals(Vector<WebModel::TypedResourceId>&& meshRemovals, Vector<WebModel::TypedResourceId>&& materialRemovals, Vector<WebModel::TypedResourceId>&& textureRemovals, CompletionHandler<void(bool)>&& completion)
{
    m_backing->processRemovals(WTF::move(meshRemovals), WTF::move(materialRemovals), WTF::move(textureRemovals), WTF::move(completion));
}

}

#endif
